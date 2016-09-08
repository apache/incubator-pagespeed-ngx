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

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/cache_url_async_fetcher.h"
#include "net/instaweb/http/public/http_dump_url_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/domain_rewrite_filter.h"
#include "pagespeed/apache/apache_config.h"
#include "pagespeed/apache/apache_server_context.h"
#include "pagespeed/apache/apache_writer.h"
#include "pagespeed/apache/instaweb_handler.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/chunking_writer.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/html/html_keywords.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"

// The Apache headers must be after instaweb headers.  Otherwise, the
// compiler will complain
//   "strtoul_is_not_a_portable_function_use_strtol_instead".
// Also note the use of 'OK" as an Instaweb enum and as an Apache #define.
#include "http_protocol.h"
#include "httpd.h"

namespace net_instaweb {

namespace {

// Some of the sites we are trying to slurp have pagespeed enabled already.
// We actually want to start with the non-pagespeed-enabled site.  But we'd
// rather not send ModPagespeed=off to servers that are not expecting it.
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
    request_headers()->Add(kPageSpeedHeader, "off");
    request_headers()->Add(kModPagespeedHeader, "off");
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

      // Note that the request headers might be sanitized as we are entering
      // the origin network, so there's no guarantees.  We might need to
      // send in the query-params after all.
      LOG(ERROR) << "URL " << url_ << " already has mod_pagespeed.  Stripping.";
      Reset();  // Clears output buffer and response_headers.
      GoogleString::size_type question = url_.find('?');
      if (question == GoogleString::npos) {
        url_ += "?ModPagespeed=off";
      } else {
        url_ += "&ModPagespeed=off";
      }
      stripped_ = true;
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

bool InstawebHandler::ProxyUrl() {
  GoogleString origin_host;
  GoogleString stripped_url = stripped_gurl_.Spec().as_string();
  const DomainLawyer* lawyer = options()->domain_lawyer();
  const GoogleString& proxy_suffix = lawyer->proxy_suffix();
  if (!proxy_suffix.empty() &&
      !lawyer->StripProxySuffix(stripped_gurl_, &stripped_url, &origin_host)) {
    // This is simply a request for a URL whose host does not end in
    // proxy_suffix.  Let another Apache handler handle it; it might
    // be a static asset, mod_pagespeed_example, etc.  Note that this
    // typically only happens when testing via forward proxy setting
    // in the browser.
    return false;
  }

  if (!AuthenticateProxy()) {
    return true;
  }

  // Figure out if we should be using a slurp fetcher rather than the default
  // system fetcher.
  UrlAsyncFetcher* fetcher = server_context_->DefaultSystemFetcher();
  scoped_ptr<UrlAsyncFetcher> fetcher_storage;

  if (options()->test_proxy() && !options()->test_proxy_slurp().empty()) {
    fetcher_storage.reset(new HttpDumpUrlFetcher(
        options()->test_proxy_slurp(), server_context_->file_system(),
        server_context_->timer()));
    fetcher = fetcher_storage.get();
  } else if (!proxy_suffix.empty()) {
    // Do some extra caching when using proxy_suffix (but we don't want it in
    // other modes since they are used for things like loadtesting)

    // Passing the 'fetcher' explicitly here rather than calling
    // CreateCacheFetcher() avoids getting the driver's loopback
    // fetcher.  We don't want the loopback fetcher because we
    // are proxying an external site.

    const GoogleString& fragment = options()->cache_fragment().empty()
        ? request_context_->minimal_private_suffix()
        : options()->cache_fragment();
    // Note that the cache fetcher is aware of request methods, so it won't
    // cache POSTs improperly.
    CacheUrlAsyncFetcher* cache_url_async_fetcher =
        server_context_->CreateCustomCacheFetcher(options(), fragment,
                                                  NULL, fetcher);
    cache_url_async_fetcher->set_ignore_recent_fetch_failed(true);
    fetcher_storage.reset(cache_url_async_fetcher);
    fetcher = fetcher_storage.get();
  }

  MessageHandler* handler = server_context_->message_handler();
  RequestContextPtr request_context(
      // TODO(sligocki): Do we want custom options here?
      new RequestContext(options()->ComputeHttpOptions(),
                         server_context_->thread_system()->NewMutex(),
                         server_context_->timer()));
  StrippingFetch fetch(stripped_url, options()->domain_lawyer(),
                       fetcher, server_context_->thread_system(),
                       request_context, handler);
  fetch.set_request_headers(request_headers_.get());

  // Handle a POST if needed.
  if (request_->method_number == M_POST) {
    apr_status_t ret;
    GoogleString payload;
    if (!parse_body_from_post(request_, &payload, &ret)) {
      handler->Message(kInfo, "Trouble parsing POST of %s.",
                       stripped_url.c_str());
      request_->status = HttpStatus::kBadRequest;
      ap_send_error_response(request_, 0);
      return true;
    } else {
      fetch.request_headers()->set_method(RequestHeaders::kPost);
      fetch.request_headers()->set_message_body(payload);
    }
  }

  if (!origin_host.empty()) {
    // origin_host has proxy_suffix (if any) stripped out, allowing us
    // to fetch the origin content.
    fetch.request_headers()->Replace(HttpAttributes::kHost, origin_host);
  }

  bool fetch_succeeded = fetch.Fetch();
  if (fetch_succeeded) {
    if (fetch.response_headers()->status_code() != HttpStatus::kOK) {
      // For redirects, we will need to update the Location: header.
      // We have to do it here rather than relying on normal rewriting
      // via DomainRewriteFilter since Apache 2.4's implementation of
      // AddOutputFilterByType doesn't apply to non-200s, and the check
      // doesn't appear to be possible to disable just for us.
      //
      // Similarly other non-200s may have cookies, so may also need patching.
      // (200s will get handled by DomainRewriteFilter via normal rewriting).
      DomainRewriteFilter::UpdateDomainHeaders(
          stripped_gurl_, server_context_, server_context_->global_options(),
          fetch.response_headers());
    }

    // We always disable downstream header filters when sending out
    // slurped resources, since we've captured them from the origin
    // in the fetch we did to write the slurp.
    ApacheWriter apache_writer(request_, server_context_->thread_system());

    ChunkingWriter chunking_writer(
        &apache_writer, options()->slurp_flush_limit());
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
    server_context_->ReportSlurpNotFound(stripped_url, request_);
  }
  return true;
}

bool InstawebHandler::AuthenticateProxy() {
  StringPiece cookie_name, cookie_value, redirect;
  const ApacheConfig* config = ApacheConfig::DynamicCast(options());
  if (config->GetProxyAuth(&cookie_name, &cookie_value, &redirect)) {
    bool ok = cookie_value.empty()
        ? request_headers_->HasCookie(cookie_name)
        : request_headers_->HasCookieValue(cookie_name, cookie_value);
    if (!ok) {
      ResponseHeaders response_headers;
      if (redirect.empty()) {
        response_headers.SetStatusAndReason(HttpStatus::kForbidden);
      } else {
        response_headers.SetStatusAndReason(HttpStatus::kTemporaryRedirect);
        response_headers.Add(HttpAttributes::kContentType, "text/html");
        response_headers.Add(HttpAttributes::kLocation, redirect);
        GoogleString redirect_escaped;
        HtmlKeywords::Escape(redirect, &redirect_escaped);
        send_out_headers_and_body(request_, response_headers, StrCat(
            "Redirecting to ", redirect_escaped));
      }
      return false;
    }
  }
  return true;
}

}  // namespace net_instaweb
