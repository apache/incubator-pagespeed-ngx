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
//
// Author: jmarantz@google.com (Joshua Marantz)

// Must precede any Apache includes, for now, due a conflict in
// the use of 'OK" as an Instaweb enum and as an Apache #define.
#include "base/string_util.h"
#include "net/instaweb/apache/header_util.h"
#include "net/instaweb/apache/instaweb_context.h"

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

#include "net/instaweb/apache/apache_resource_manager.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/util/public/chunking_writer.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/thread_system.h"

// The Apache headers must be after instaweb headers.  Otherwise, the
// compiler will complain
//   "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "apr_strings.h"  // for apr_pstrdup
#include "http_protocol.h"

namespace net_instaweb {

namespace {

// Default handler when the file is not found
void SlurpDefaultHandler(request_rec* r) {
  ap_set_content_type(r, "text/html; charset=utf-8");
  GoogleString buf = StringPrintf(
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

class ApacheWriter : public Writer {
 public:
  ApacheWriter(request_rec* r, ResponseHeaders* response_headers)
      : request_(r),
        response_headers_(response_headers),
        headers_out_(false) {
  }

  virtual bool Write(const StringPiece& str, MessageHandler* handler) {
    if (!headers_out_) {
      OutputHeaders();
    }
    ap_rwrite(str.data(), str.size(), request_);
    return true;
  }

  virtual bool Flush(MessageHandler* handler) {
    ap_rflush(request_);
    return true;
  }

  void OutputHeaders() {
    if (headers_out_) {
      return;
    }
    headers_out_ = true;

    // Apache2 defaults to set the status line as HTTP/1.1.  If the
    // original content was HTTP/1.0, we need to force the server to use
    // HTTP/1.0.  I'm not sure why/whether we need to do this; it was in
    // mod_static from the sdpy project, which is where I copied this
    // code from.
    if ((response_headers_->major_version() == 1) &&
        (response_headers_->minor_version() == 0)) {
      apr_table_set(request_->subprocess_env, "force-response-1.0", "1");
    }

    char* content_type = NULL;
    ConstStringStarVector v;
    CHECK(response_headers_->headers_complete());
    if (response_headers_->Lookup(HttpAttributes::kContentType, &v)) {
      CHECK(!v.empty());
      // ap_set_content_type does not make a copy of the string, we need
      // to duplicate it.  Note that we will update the content type below,
      // after transforming the headers.
      const GoogleString* last = v[v.size() - 1];
      content_type = apr_pstrdup(request_->pool,
                                 (last == NULL) ? NULL : last->c_str());
    }
    response_headers_->RemoveAll(HttpAttributes::kTransferEncoding);
    response_headers_->RemoveAll(HttpAttributes::kContentLength);
    ResponseHeadersToApacheRequest(*response_headers_, request_);
    if (content_type != NULL) {
      ap_set_content_type(request_, content_type);
    }

    // Recompute the content-length, because the content is decoded.
    // TODO(lsong): We don't know the content size, do we?
    // ap_set_content_length(request_, contents.size());
  }

 private:
  request_rec* request_;
  ResponseHeaders* response_headers_;
  bool headers_out_;

  DISALLOW_COPY_AND_ASSIGN(ApacheWriter);
};

// Remove any mod-pagespeed-specific modifiers before we go to our slurped
// fetcher.
//
// TODO(jmarantz): share the string constants from mod_instaweb.cc and
// formalize the prefix-matching assumed here.
GoogleString RemoveModPageSpeedQueryParams(
    const GoogleString& uri, const char* query_param_string) {
  QueryParams query_params, stripped_query_params;
  query_params.Parse(query_param_string);
  bool rewrite_query_params = false;

  for (int i = 0; i < query_params.size(); ++i) {
    const char* name = query_params.name(i);
    static const char kModPagespeed[] = "ModPagespeed";
    if (strncmp(name, kModPagespeed, STATIC_STRLEN(kModPagespeed)) == 0) {
      rewrite_query_params = true;
    } else {
      const GoogleString* value = query_params.value(i);
      StringPiece value_piece;  // NULL data by default.
      if (value != NULL) {
        value_piece = *value;
      }
      stripped_query_params.Add(name, value_piece);
    }
  }

  GoogleString stripped_url;
  if (rewrite_query_params) {
    // TODO(jmarantz): It would be nice to use GoogleUrl to do this but
    // it's not clear how it would help.  Instead just hack the string.
    GoogleString::size_type question_mark = uri.find('?');
    CHECK(question_mark != GoogleString::npos);
    stripped_url.append(uri.data(), question_mark);  // does not include "?" yet
    if (stripped_query_params.size() != 0) {
      stripped_url += StrCat("?", stripped_query_params.ToString());
    }
  } else {
    stripped_url = uri;
  }
  return stripped_url;
}

// Some of the sites we are trying to slurp have mod-pagespeed enabled already.
// We actually want to start with the non-pagespeed-enabled site.  But we'd
// rather not send ModPagespeed=off to servers that are not expecting it.
class StrippingFetch : public UrlAsyncFetcher::Callback {
 public:
  StrippingFetch(const DomainLawyer* lawyer,
                 const GoogleString& url_input,
                 const RequestHeaders& request_headers,
                 UrlAsyncFetcher* fetcher,
                 ResponseHeaders* response_headers,
                 GoogleString* dest_buffer,
                 ThreadSystem* thread_system,
                 MessageHandler* message_handler)
      : fetcher_(fetcher),
        lawyer_(lawyer),
        url_(url_input),
        response_headers_(response_headers),
        dest_buffer_(dest_buffer),
        writer_(dest_buffer_),
        message_handler_(message_handler),
        stripped_(false),
        success_(false),
        done_(false),
        mutex_(thread_system->NewMutex()),
        condvar_(mutex_->NewCondvar()) {
    request_headers_.CopyFrom(request_headers);  // we may mutate Host header
  }

  virtual bool EnableThreaded() const { return true; }

  bool Fetch() {
    // To test sharding domains from a slurp of a site that does not support
    // sharded domains, we apply mapping origin domain here.  Simply map all
    // the shards back into the origin domain in pagespeed.conf.
    GoogleString origin_url;
    if (lawyer_->MapOrigin(url_, &origin_url)) {
      url_ = origin_url;
      GoogleUrl gurl(url_);
      request_headers_.Replace(HttpAttributes::kHost, gurl.Host());
    }

    fetcher_->StreamingFetch(url_, request_headers_, response_headers_,
                             &writer_, message_handler_, this);
    {
      ScopedMutex lock(mutex_.get());
      while (!done_) {
        condvar_->TimedWait(Timer::kSecondMs);
      }
    }
    return success_;
  }

  virtual void Done(bool success) {
    bool done = true;
    if (!success) {
      success_ = false;
    } else if (stripped_) {
      // Second pass -- declare completion.
      success_ = true;
    } else if (response_headers_->Lookup1(kModPagespeedHeader) != NULL) {
      // First pass -- the slurped site evidently had mod_pagespeed already
      // enabled.  Turn it off and re-fetch.
      LOG(ERROR) << "URL " << url_ << " already has mod_pagespeed.  Stripping.";
      response_headers_->Clear();
      GoogleString::size_type question = url_.find('?');
      if (question == GoogleString::npos) {
        url_ += "?ModPagespeed=off";
      } else {
        url_ += "&ModPagespeed=off";
      }
      stripped_ = true;
      dest_buffer_->clear();
      fetcher_->StreamingFetch(url_, request_headers_, response_headers_,
                               &writer_, message_handler_, this);
      done = false;
    } else {
      // First-pass -- the origin site did not have mod_pagespeed so no need
      // for a second pass.
      success_ = true;
    }
    if (done) {
      ScopedMutex lock(mutex_.get());
      done_ = true;
      condvar_->Signal();
      // Don't "delete this" -- this is allocated on the stack in ApacheSlurp.
    }
  }

 private:
  UrlAsyncFetcher* fetcher_;
  const DomainLawyer* lawyer_;
  GoogleString url_;
  RequestHeaders request_headers_;
  ResponseHeaders* response_headers_;
  GoogleString* dest_buffer_;
  StringWriter writer_;
  MessageHandler* message_handler_;
  bool stripped_;
  bool success_;
  bool done_;
  scoped_ptr<ThreadSystem::CondvarCapableMutex> mutex_;
  scoped_ptr<ThreadSystem::Condvar> condvar_;

  DISALLOW_COPY_AND_ASSIGN(StrippingFetch);
};

}  // namespace

void SlurpUrl(ApacheResourceManager* manager, request_rec* r) {
  const char* uri = InstawebContext::MakeRequestUrl(r);
  RequestHeaders request_headers;
  ResponseHeaders response_headers;
  ApacheRequestToRequestHeaders(*r, &request_headers);
  ApacheWriter apache_writer(r, &response_headers);
  ChunkingWriter writer(&apache_writer, manager->config()->slurp_flush_limit());

  GoogleString stripped_url = RemoveModPageSpeedQueryParams(
      uri, r->parsed_uri.query);

  UrlAsyncFetcher* fetcher = manager->DefaultSystemFetcher();
  GoogleString contents;
  MessageHandler* handler = manager->message_handler();
  StrippingFetch fetch(manager->config()->domain_lawyer(),
                       stripped_url, request_headers,
                       fetcher, &response_headers, &contents,
                       manager->thread_system(), handler);
  if (fetch.Fetch()) {
    apache_writer.OutputHeaders();
    writer.Write(contents, handler);
  } else {
    handler->Message(kInfo, "mod_pagespeed: slurp of url %s failed.\n"
                     "Request Headers: %s\n\nResponse Headers: %s",
                     stripped_url.c_str(),
                     request_headers.ToString().c_str(),
                     response_headers.ToString().c_str());
    SlurpDefaultHandler(r);
  }
}

}  // namespace net_instaweb
