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

#include "net/instaweb/apache/apache_slurp.h"

// Must precede any Apache includes, for now, due a conflict in
// the use of 'OK" as an Instaweb enum and as an Apache #define.
#include "net/instaweb/apache/header_util.h"
#include "net/instaweb/apache/apache_config.h"
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

#include "base/logging.h"
#include "net/instaweb/apache/apache_server_context.h"
#include "net/instaweb/apache/apache_writer.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/http_dump_url_fetcher.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/chunking_writer.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"
#include "pagespeed/kernel/http/http_options.h"

// The Apache headers must be after instaweb headers.  Otherwise, the
// compiler will complain
//   "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "http_protocol.h"
#include "httpd.h"

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

// Remove any mod-pagespeed-specific modifiers before we go to our slurped
// fetcher.
//
// TODO(jmarantz): share the string constants from mod_instaweb.cc and
// formalize the prefix-matching assumed here.
GoogleString RemoveModPageSpeedQueryParams(
    const GoogleString& uri, const char* query_param_string) {
  QueryParams query_params, stripped_query_params;
  query_params.ParseFromUntrustedString(query_param_string);
  bool rewrite_query_params = false;

  for (int i = 0; i < query_params.size(); ++i) {
    StringPiece name = query_params.name(i);
    if (name.starts_with(RewriteQuery::kModPagespeed) ||
        name.starts_with(RewriteQuery::kPageSpeed)) {
      rewrite_query_params = true;
    } else {
      const GoogleString* value = query_params.EscapedValue(i);
      StringPiece value_piece;  // NULL data by default.
      if (value != NULL) {
        value_piece = *value;
      }
      stripped_query_params.AddEscaped(name, value_piece);
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
      StrAppend(&stripped_url, "?", stripped_query_params.ToEscapedString());
    }
  } else {
    stripped_url = uri;
  }
  return stripped_url;
}

// Some of the sites we are trying to slurp have pagespeed enabled already.
// We actually want to start with the non-pagespeed-enabled site.  But we'd
// rather not send ModPagespeed=off to servers that are not expecting it.
// TODO(sligocki): Perhaps we should just send the "ModPagespeed: off" header
// which seems less intrusive.
class StrippingFetch : public StringAsyncFetch {
 public:
  StrippingFetch(const GoogleString& url_input,
                 const DomainLawyer* lawyer,
                 UrlAsyncFetcher* fetcher,
                 ThreadSystem* thread_system,
                 const RequestContextPtr& ctx,
                 MessageHandler* message_handler)
      : StringAsyncFetch(ctx),
        fetcher_(fetcher),
        lawyer_(lawyer),
        url_(url_input),
        message_handler_(message_handler),
        stripped_(false),
        mutex_(thread_system->NewMutex()),
        condvar_(mutex_->NewCondvar()) {
  }

  // Blocking fetch.
  bool Fetch() {
    // To test sharding domains from a slurp of a site that does not support
    // sharded domains, we apply mapping origin domain here.  Simply map all
    // the shards back into the origin domain in pagespeed.conf.
    GoogleString origin_url;
    GoogleString host_header;
    bool is_proxy = false;
    if (lawyer_->MapOrigin(url_, &origin_url, &host_header, &is_proxy)) {
      url_ = origin_url;
      GoogleUrl gurl(url_);
      request_headers()->Replace(HttpAttributes::kHost, host_header);
    }

    fetcher_->Fetch(url_, message_handler_, this);
    {
      ScopedMutex lock(mutex_.get());
      while (!done()) {
        condvar_->TimedWait(Timer::kSecondMs);
      }
    }
    return success();
  }

  virtual void HandleDone(bool success) {
    bool done = true;
    if (!success) {
      set_success(false);
    } else if (stripped_) {
      // Second pass -- declare completion.
      set_success(true);
    } else if ((response_headers()->Lookup1(kModPagespeedHeader) != NULL) ||
               (response_headers()->Lookup1(kPageSpeedHeader) != NULL)) {
      // First pass -- the slurped site evidently had mod_pagespeed already
      // enabled.  Turn it off and re-fetch.
      LOG(ERROR) << "URL " << url_ << " already has mod_pagespeed.  Stripping.";
      Reset();  // Clears output buffer and response_headers.
      GoogleString::size_type question = url_.find('?');
      if (question == GoogleString::npos) {
        url_ += "?ModPagespeed=off";
      } else {
        url_ += "&ModPagespeed=off";
      }
      stripped_ = true;
      // TODO(sligocki): This currently allows infinite looping behavior if
      // someone returns X-Mod-Pagespeed headers unexpectedly.
      fetcher_->Fetch(url_, message_handler_, this);
      done = false;
    } else {
      // First-pass -- the origin site did not have mod_pagespeed so no need
      // for a second pass.
      set_success(true);
    }
    if (done) {
      ScopedMutex lock(mutex_.get());
      set_done(true);
      condvar_->Signal();
      // Don't "delete this" -- this is allocated on the stack in ApacheSlurp.
    }
  }

 private:
  UrlAsyncFetcher* fetcher_;
  const DomainLawyer* lawyer_;

  GoogleString url_;
  MessageHandler* message_handler_;

  bool stripped_;

  scoped_ptr<ThreadSystem::CondvarCapableMutex> mutex_;
  scoped_ptr<ThreadSystem::Condvar> condvar_;

  DISALLOW_COPY_AND_ASSIGN(StrippingFetch);
};

}  // namespace

void SlurpUrl(ApacheServerContext* server_context, request_rec* r) {
  const char* url =
      InstawebContext::MakeRequestUrl(*server_context->global_options(), r);
  GoogleString stripped_url = RemoveModPageSpeedQueryParams(
      url, r->parsed_uri.query);

  // Figure out if we should be using a slurp fetcher rather than the default
  // system fetcher.
  UrlAsyncFetcher* fetcher = server_context->DefaultSystemFetcher();
  scoped_ptr<HttpDumpUrlFetcher> slurp_fetcher;

  ApacheConfig* global_config = server_context->global_config();
  if (global_config->test_proxy() &&
      !global_config->test_proxy_slurp().empty()) {
    slurp_fetcher.reset(new HttpDumpUrlFetcher(
        global_config->test_proxy_slurp(), server_context->file_system(),
        server_context->timer()));
    fetcher = slurp_fetcher.get();
  }

  MessageHandler* handler = server_context->message_handler();
  RequestContextPtr request_context(
      // TODO(sligocki): Do we want custom options here?
      new RequestContext(global_config->ComputeHttpOptions(),
                         server_context->thread_system()->NewMutex(),
                         server_context->timer()));
  StrippingFetch fetch(stripped_url, global_config->domain_lawyer(),
                       fetcher, server_context->thread_system(),
                       request_context, handler);
  ApacheRequestToRequestHeaders(*r, fetch.request_headers());

  bool fetch_succeeded = fetch.Fetch();
  if (fetch_succeeded) {
    // We always disable downstream header filters when sending out
    // slurped resources, since we've captured them from the origin
    // in the fetch we did to write the slurp.
    ApacheWriter apache_writer(r);
    apache_writer.set_disable_downstream_header_filters(true);
    ChunkingWriter chunking_writer(
        &apache_writer, global_config->slurp_flush_limit());
    apache_writer.OutputHeaders(fetch.response_headers());
    chunking_writer.Write(fetch.buffer(), handler);
  } else {
    handler->Message(kInfo, "mod_pagespeed: slurp of url %s failed.\n"
                     "Request Headers: %s\n\nResponse Headers: %s",
                     stripped_url.c_str(),
                     fetch.request_headers()->ToString().c_str(),
                     fetch.response_headers()->ToString().c_str());
  }

  if (!fetch_succeeded || fetch.response_headers()->IsErrorStatus()) {
    server_context->ReportSlurpNotFound(stripped_url, r);
  }
}

}  // namespace net_instaweb
