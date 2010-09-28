// Copyright 2010 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>

// Must precede any Apache includes, for now, due a conflict in
// the use of 'OK" as an Instaweb enum and as an Apache #define.

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "net/instaweb/apache/header_util.h"
#include "net/instaweb/apache/log_message_handler.h"

// TODO(jmarantz): serf_url_async_fetcher evidently sets
// 'gzip' unconditionally, and the response includes the 'gzip'
// encoding header, but serf unzips the response itself.
//
// I think the correct behavior is that our async fetcher should
// transmit the 'gzip' request header if it was specified in the call
// to StreamingFetch.  This would be easy to fix.
//
// Unfortunately, serf 0.31 appears to unzip the content for us, which
// is not what we are asking for.  And it leaves the 'gzip' response
// header in despite having unzipped it itself.  I have tried later
// versions of serf, but the API is not compatible (easy to deal with)
// but the resulting binary has unresolved symbols.  I am wondering
// whether we will have to go to libcurl.
//
// For now use wget when slurping additional files.

#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/apache/serf_url_fetcher.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/http_dump_url_fetcher.h"
#include "net/instaweb/util/public/http_dump_url_writer.h"
#include "net/instaweb/util/public/meta_data.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/string_writer.h"

// The httpd headers must be after instaweb headers.  Otherwise, the
// compiler will complain
//   "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "apr_strings.h"
#include "httpd.h"
#include "http_core.h"
#include "http_config.h"
#include "http_log.h"
#include "http_protocol.h"

extern "C" {
extern module AP_MODULE_DECLARE_DATA slurp_module;
}

namespace {

// TODO(jmarantz): refactor this code to share a util class with
// mod_instweb.cc.  Also (especially) refactor the initialization and
// parsing of the config option array.

enum ConfigSwitch {CONFIG_ON, CONFIG_OFF, CONFIG_ERROR};

ConfigSwitch get_config_switch(const char* arg) {
  if (strcasecmp(arg, "on") == 0) {
    return CONFIG_ON;
  } else if (strcasecmp(arg, "off") == 0) {
    return CONFIG_OFF;
  } else {
    return CONFIG_ERROR;
  }
}

}  // namespace

namespace net_instaweb {


class SlurpContext {
 public:
  SlurpContext() : read_only_(false) {
    apr_pool_create(&pool_, NULL);
  }

  void set_slurp_root_dir(const char* s) {
    slurp_root_dir_ = s;
    EnsureEndsInSlash(&slurp_root_dir_);
  }
  const std::string& slurp_root_dir() { return slurp_root_dir_; }

  void set_read_only(bool read_only) { read_only_ = read_only; }

  UrlFetcher* fetcher() {
    if (url_fetcher_.get() == NULL) {
      if (read_only_) {
        url_fetcher_.reset(
            new HttpDumpUrlFetcher(slurp_root_dir_, &file_system_, &timer_));
      } else {
        const char* proxy = "";  // TODO(jmarantz): allow setting this
        SerfUrlAsyncFetcher* async_fetcher =
            new SerfUrlAsyncFetcher(proxy, pool_);
        int64 timeout_ms = 5000;
        SerfUrlFetcher* fetcher = new SerfUrlFetcher(timeout_ms, async_fetcher);
        url_fetcher_.reset(new HttpDumpUrlWriter(
            slurp_root_dir_, fetcher, &file_system_, &timer_));
      }
    }
    return url_fetcher_.get();
  }

  MessageHandler* message_handler() { return &message_handler_; }
  bool ProcessUrl(request_rec* r);

 private:
  apr_pool_t* pool_;
  StdioFileSystem file_system_;
  std::string slurp_root_dir_;
  bool read_only_;
  scoped_ptr<UrlFetcher> url_fetcher_;
  GoogleMessageHandler message_handler_;
  html_rewriter::AprTimer timer_;
};

SlurpContext* GetSlurpContext(server_rec* server) {
  return static_cast<SlurpContext*> ap_get_module_config(
      server->module_config, &slurp_module);
}

void* mod_slurp_create_server_config(apr_pool_t* pool, server_rec* server) {
  SlurpContext* sc = GetSlurpContext(server);
  if (sc == NULL) {
    sc = new (apr_pcalloc(pool, sizeof(SlurpContext))) SlurpContext;
  }
  return sc;
}

// Default handler when the file is not found
void SlurpDefaultHandler(request_rec* r) {
  ap_set_content_type(r, "text/html; charset=utf-8");
  std::string buf = StringPrintf(
      "<html><head><title>Slurp Error</title></head>"
      "<body><h1>Slurp failed</h1>\n"
      "<p>host=%s\n"
      "<p>uri=%s\n"
      "</body></html>",
      r->hostname, r->unparsed_uri);
  ap_rputs(buf.c_str(), r);
  r->status = HttpStatus::kNotFound;
  r->status_line = "Not Found";
}

// TODO(jmarantz): The ApacheWriter defined below is much more
// efficient than the mechanism we are currently using, which is to
// buffer the entire response in a string and then send it later.
// For some reason, this did not work when I tried it, but it's
// worth another look.

#if 0
class ApacheWriter : public Writer {
 public:
  explicit ApacheWriter(request_rec* r)
      : request_(r),
        size_(0) {
  }

  virtual bool Write(const StringPiece& str, MessageHandler* handler) {
    ap_rwrite(str.data(), str.size(), request_);
    size_ += str.size();
    return true;
  }

  virtual bool Flush(MessageHandler* handler) {
    return true;
  }

  int size() const { return size_; }

 private:
  request_rec* request_;
  int size_;

  DISALLOW_COPY_AND_ASSIGN(ApacheWriter);
};
#endif

bool SlurpContext::ProcessUrl(request_rec* r) {
  SimpleMetaData request_headers, response_headers;
  ApacheHeaderToMetaData(r->headers_in, 0, 0, &request_headers);
  std::string contents;
  StringWriter writer(&contents);
  std::string uri;
  if (strncmp(r->unparsed_uri, "http://", 7) == 0) {
    uri = r->unparsed_uri;
  } else {
    uri = StrCat("http://", r->hostname, r->unparsed_uri);
  }

  // TODO(jmarantz) Strip out instaweb pass-through directive,
  // changing "?instaweb=0&" --> "?", "?instaweb=0" --> "", and
  // "&instaweb=0" --> "".

  if (!fetcher()->StreamingFetchUrl(uri.c_str(), request_headers,
                                    &response_headers, &writer,
                                    &message_handler_)) {
    return false;
  }

  // Apache2 defaults to set the status line as HTTP/1.1.  If the
  // original content was HTTP/1.0, we need to force the server to use
  // HTTP/1.0.  I'm not sure why/whether we need to do this; it was in
  // mod_static from the sdpy project, which is where I copied this
  // code from.
  if ((response_headers.major_version() == 1) &&
      (response_headers.minor_version() == 0)) {
    apr_table_set(r->subprocess_env, "force-response-1.0", "1");
  }

  char* content_type = NULL;
  CharStarVector v;
  if (response_headers.Lookup("content-type", &v)) {
    CHECK(!v.empty());
    // ap_set_content_type does not make a copy of the string, we need
    // to duplicate it.  Note that we will update the content type below,
    // after transforming the headers.
    content_type = apr_pstrdup(r->pool, v[v.size() - 1]);
    response_headers.RemoveAll("content-type");
  }
  response_headers.RemoveAll("transfer-encoding");

  // TODO(jmarantz): centralize standard header names; probably as
  // static const member variables in util/public/meta_data.h.
  response_headers.RemoveAll("content-length");  // we will recompute
  MetaDataToApacheHeader(response_headers, r->headers_out,
                         &r->status, &r->proto_num);
  LOG(INFO) << "slurp output headers: " << response_headers.ToString();
  if (content_type != NULL) {
    ap_set_content_type(r, content_type);
  }

  // Recompute the content-length, because the content is decoded.
  ap_set_content_length(r, contents.size());
  ap_rwrite(contents.c_str(), contents.size(), r);

  return true;
}

int SlurpHandler(request_rec* r) {
  // Check if the request is for our slurp content generator
  // Decline the request so that other handler may process
  if (!r->handler || (strcmp(r->handler, "slurp") != 0)) {
    ap_log_rerror(APLOG_MARK, APLOG_WARNING, APR_SUCCESS, r,
                  "Not slurp request.");
    return DECLINED;
  }

  // Only handle GET request
  if (r->method_number != M_GET) {
    ap_log_rerror(APLOG_MARK, APLOG_WARNING, APR_SUCCESS, r,
                  "Not GET request: %d.", r->method_number);
    return HTTP_METHOD_NOT_ALLOWED;
  }

  SlurpContext* sc = GetSlurpContext(r->server);
  if (!sc->ProcessUrl(r)) {
    SlurpDefaultHandler(r);
  }

  // TODO(jmarantz): consider returning NOT_FOUND in addition to,
  // or instead of explictly setting the http status code.
  // (Does this matter?)
  return OK;
}

void slurp_register_hooks(apr_pool_t* p) {
  // Register the content generator, or handler.
  ap_hook_handler(SlurpHandler, NULL, NULL, APR_HOOK_MIDDLE);
}

const char* ProcessOption(cmd_parms* cmd, void* data, const char* arg) {
  SlurpContext* sc = GetSlurpContext(cmd->server);
  const char* directive = (cmd->directive->directive);

  if (strcasecmp(directive, "slurp_directory") == 0) {
    sc->set_slurp_root_dir(arg);
  } else if (strcasecmp(directive, "slurp_write") == 0) {
    ConfigSwitch config_switch = get_config_switch(arg);
    if (config_switch == CONFIG_ERROR) {
      return apr_pstrcat(cmd->pool, "slurp_write", " on|off", NULL);
    }
    sc->set_read_only(config_switch == CONFIG_OFF);
  } else {
    return "Unknown directive.";
  }
  return NULL;
}

const command_rec mod_slurp_options[] = {
  AP_INIT_TAKE1("slurp_directory",
                reinterpret_cast<const char*(*)()>(ProcessOption),
                NULL, RSRC_CONF,
                "Set the directory used to find slurped files"),
  AP_INIT_TAKE1("slurp_write",
                reinterpret_cast<const char*(*)()>(ProcessOption),
                NULL, RSRC_CONF,
                "If set to true, fetches resources not in slurp dir"),
  {NULL}
};

}  // namespace net_instaweb

extern "C" {

  // Export our module so Apache is able to load us.
  // See http://gcc.gnu.org/wiki/Visibility for more information.
#if defined(__linux)
#pragma GCC visibility push(default)
#endif

  // Declare our module object (note that "module" is a typedef for "struct
  // module_struct"; see http_config.h for the definition of module_struct).
  module AP_MODULE_DECLARE_DATA slurp_module = {
    // This next macro indicates that this is a (non-MPM) Apache 2.0 module
    // (the macro actually expands to multiple comma-separated arguments; see
    // http_config.h for the definition):
    STANDARD20_MODULE_STUFF,

    // These next four arguments are callbacks, but we currently don't need
    // them, so they are left null:
    NULL,  // create per-directory config structure
    NULL,  // merge per-directory config structures
    net_instaweb::mod_slurp_create_server_config,
    NULL,  // merge per-server config structures
    net_instaweb::mod_slurp_options,

    // Finally, this function will be called to register hooks for this module:
    net_instaweb::slurp_register_hooks
  };

#if defined(__linux)
#pragma GCC visibility pop
#endif
}
