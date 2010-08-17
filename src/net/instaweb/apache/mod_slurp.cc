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
#include "net/instaweb/util/public/simple_meta_data.h"

#include "apr_strings.h"
#include "base/string_util.h"
#include "httpd.h"
#include "http_core.h"
#include "http_config.h"
#include "http_log.h"
#include "http_protocol.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/http_response_parser.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/string_writer.h"

namespace {

// Define the slurp content root directory
const char* kSlurpDir = "/slurp/";

// Default handler when the file is not found
int SlurpDefaultHandler(const std::string& filename, request_rec* r) {
  ap_set_content_type(r, "text/html; charset=utf-8");
  ap_rputs("<html><head><title>Slurp Error</title></head>", r);
  ap_rputs("<body><h1>Slurp failed</h1>OK", r);
  ap_rputs("<hr>NOT FOUND:", r);
  ap_rputs(filename.c_str(), r);
  ap_rputs("</body></html>", r);
  return OK;
}

// TODO(jmarantz): The ApacheWriter defined below is much more
// efficient than the mechanism we are currently using, which is to
// buffer the entire response in a string and then send it later.
// For some reason, this did not work when I tried it, but it's
// worth another look.

#if 0
class ApacheWriter : public net_instaweb::Writer {
 public:
  ApacheWriter(request_rec* r)
      : request_(r),
        size_(0) {
  }

  virtual bool Write(const net_instaweb::StringPiece& str,
                     net_instaweb::MessageHandler* handler) {
    ap_rwrite(str.data(), str.size(), request_);
    size_ += str.size();
    return true;
  }

  virtual bool Flush(net_instaweb::MessageHandler* handler) {
    return true;
  }

  int size() const { return size_; }

 private:
  request_rec* request_;
  int size_;
};
#endif

// Process decoded file -- ungziped, unchunked
int ProcessDecodedFile(const std::string& filename, request_rec* r) {
  FILE* file = fopen(filename.c_str(), "rb");
  if (file == NULL) {
    ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_SUCCESS, r,
                  "Can't open file %s", filename.c_str());
    return SlurpDefaultHandler(filename, r);
  }


  net_instaweb::SimpleMetaData headers;
  std::string contents;
  net_instaweb::StringWriter writer(&contents);
  net_instaweb::GoogleMessageHandler handler;
  net_instaweb::HttpResponseParser parser(&headers, &writer, &handler);
  bool ok = parser.Parse(file);
  fclose(file);

  if (!ok) {
    ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_SUCCESS, r,
                  "Can't parse from fd for %s", filename.c_str());
    return SlurpDefaultHandler(filename, r);
  }

  std::string protocol_version = StringPrintf(
      "%d.%d", headers.major_version(), headers.minor_version());

  // Apache2 defaults to set the status line as HTTP/1.1
  // If the original content was HTTP/1.0, we need to force the server
  // to use HTTP/1.0
  if (protocol_version == "HTTP/1.0") {
    apr_table_set(r->subprocess_env, "force-response-1.0", "1");
  }

  // Set the response status code
  r->status = headers.status_code();

  // Translate the headers from Instaweb to Apache format.
  std::string content_type;
  for (int idx = 0; idx < headers.NumAttributes(); ++idx) {
    std::string lowercase_header = StringToLowerASCII(
        std::string(headers.Name(idx)));
    if (lowercase_header == "content-encoding") {
      // Skip original encoding, since the content is decoded.
      // The content can be gziped with mod_delate.
    } else if (lowercase_header == "content-length") {
      // Skip the original content-length. Always set the content-length
      // before sending the body.
    } else if (lowercase_header == "content-type") {
      // ap_set_content_type does not make a copy of the string, we need
      // to duplicate it.
      char* ptr = apr_pstrdup(r->pool, headers.Value(idx));
      ap_set_content_type(r, ptr);
    } else {
      // apr_table_add makes copies of both head key and value, so we do not
      // have to duplicate them.
      apr_table_add(r->headers_out, headers.Name(idx), headers.Value(idx));
    }
  }

  // Recompute the content-length, because the content is decoded.
  ap_set_content_length(r, contents.size());
  ap_rwrite(contents.c_str(), contents.size(), r);

  return OK;
}

// Convert URI to encoded filename
std::string get_request_filename(request_rec* r) {
  net_instaweb::FilenameEncoder encoder;
  std::string encoded_filename;
  const char* url = r->unparsed_uri;
  if (strncmp(url, "http://", 7) == 0) {
    url += 7;
  }
  encoder.Encode(kSlurpDir, url, &encoded_filename);
  return encoded_filename;
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
  std::string full_filename = get_request_filename(r);
  return ProcessDecodedFile(full_filename, r);
}

void slurp_hook(apr_pool_t* p) {
  // Register the content generator, or handler.
  ap_hook_handler(SlurpHandler, NULL, NULL, APR_HOOK_MIDDLE);
}

}  // namespace

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
    NULL,  // create per-server config structure
    NULL,  // merge per-server config structures

    // This argument supplies a table describing the configuration directives
    // implemented by this module (however, we don't currently have any):
    NULL,

    // Finally, this function will be called to register hooks for this module:
    slurp_hook
  };

#if defined(__linux)
#pragma GCC visibility pop
#endif
}
