/*
 * Copyright 2010 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: sligocki@google.com (Shawn Ligocki)
//         jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/url_input_resource.h"

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/async_fetch_with_lock.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/http_value_writer.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

namespace {

bool IsValidAndCacheableImpl(HTTPCache* http_cache,
                             int64 min_cache_time_to_rewrite_ms,
                             bool respect_vary,
                             const ResponseHeaders& headers) {
  if (headers.status_code() != HttpStatus::kOK) {
    return false;
  }

  bool cacheable = true;
  if (respect_vary) {
    // Conservatively assume that the request has cookies, since the site may
    // want to serve different content based on the cookie. If we consider the
    // response to be cacheable here, we will serve the optimized version
    // without contacting the origin which would be against the webmaster's
    // intent. We also don't have cookies available at lookup time, so we
    // cannot try to use this response only when the request doesn't have a
    // cookie.
    cacheable = headers.VaryCacheable(true);
  } else {
    cacheable = headers.IsProxyCacheable();
  }
  // If we are setting a TTL for HTML, we cannot rewrite any resource
  // with a shorter TTL.
  cacheable &= (headers.cache_ttl_ms() >= min_cache_time_to_rewrite_ms);

  if (!cacheable && !http_cache->force_caching()) {
    return false;
  }

  // NULL is OK here since we make the request_headers ourselves.
  return !http_cache->IsAlreadyExpired(NULL, headers);
}

// Returns true if the input didn't change and we could successfully update
// input_info() in the callback.
bool CheckAndUpdateInputInfo(const ResponseHeaders& headers,
                             const HTTPValue& value,
                             const RewriteOptions& options,
                             const ServerContext& server_context,
                             Resource::FreshenCallback* callback) {
  InputInfo* input_info = callback->input_info();
  if (input_info != NULL && input_info->has_input_content_hash() &&
      IsValidAndCacheableImpl(server_context.http_cache(),
                              options.min_resource_cache_time_to_rewrite_ms(),
                              options.respect_vary(),
                              headers)) {
    StringPiece content;
    if (value.ExtractContents(&content)) {
      GoogleString new_hash = server_context.contents_hasher()->Hash(content);
      // TODO(nikhilmadan): Consider using the Etag / Last-Modified header to
      // validate if the resource has changed instead of computing the hash.
      if (new_hash == input_info->input_content_hash()) {
        callback->resource()->FillInPartitionInputInfoFromResponseHeaders(
            headers, input_info);
        return true;
      }
    }
  }
  return false;
}

}  // namespace

UrlInputResource::UrlInputResource(RewriteDriver* rewrite_driver,
                                   const RewriteOptions* options,
                                   const ContentType* type,
                                   const StringPiece& url)
    : CacheableResourceBase((rewrite_driver == NULL ? NULL :
                            rewrite_driver->server_context()), type),
      url_(url.data(), url.size()),
      rewrite_driver_(rewrite_driver),
      rewrite_options_(options),
      respect_vary_(rewrite_options_->respect_vary()) {
  response_headers()->set_implicit_cache_ttl_ms(
      options->implicit_cache_ttl_ms());
  set_enable_cache_purge(options->enable_cache_purge());
  set_disable_rewrite_on_no_transform(
      options->disable_rewrite_on_no_transform());
}

UrlInputResource::~UrlInputResource() {
}

// Shared fetch callback, used by both LoadAndCallback and Freshen.
class UrlResourceFetchCallback : public AsyncFetchWithLock {
 public:
  UrlResourceFetchCallback(ServerContext* server_context,
                           const RewriteOptions* rewrite_options,
                           const GoogleString& url,
                           HTTPValue* fallback_value,
                           const RequestContextPtr& request_context,
                           MessageHandler* handler,
                           RewriteDriver* driver)
      : AsyncFetchWithLock(server_context->lock_hasher(),
                           request_context,
                           url,
                           server_context->lock_manager(),
                           handler),
        server_context_(server_context),
        rewrite_options_(rewrite_options),
        message_handler_(NULL),
        no_cache_ok_(false),
        fetcher_(NULL),
        driver_(driver),
        respect_vary_(rewrite_options->respect_vary()),
        resource_cutoff_ms_(
            rewrite_options->min_resource_cache_time_to_rewrite_ms()),
        fallback_fetch_(NULL) {
    if (fallback_value != NULL) {
      fallback_value_.Link(fallback_value);
    }
  }
  virtual ~UrlResourceFetchCallback() {}

  bool AddToCache(bool success) {
    ResponseHeaders* headers = response_headers();
    // Merge in any extra response headers.
    headers->UpdateFrom(*extra_response_headers());
    headers->ComputeCaching();
    headers->FixDateHeaders(http_cache()->timer()->NowMs());
    if (success && !headers->IsErrorStatus()) {
      if (rewrite_options_->IsCacheTtlOverridden(url())) {
        headers->ForceCaching(rewrite_options_->override_caching_ttl_ms());
      }
      if (IsValidAndCacheableImpl(http_cache(), resource_cutoff_ms_,
                                  respect_vary_, *headers)) {
        HTTPValue* value = http_value();
        value->SetHeaders(headers);
        http_cache()->Put(url(), value, message_handler_);
        return true;
      } else {
        http_cache()->RememberNotCacheable(
            url(), headers->status_code() == HttpStatus::kOK,
            message_handler_);
      }
    } else {
      if (headers->Has(HttpAttributes::kXPsaLoadShed)) {
        http_cache()->RememberFetchDropped(url(), message_handler_);
      } else {
        http_cache()->RememberFetchFailed(url(), message_handler_);
      }
    }
    return false;
  }

  void StartFetchInternal(bool success) {
    if (!success) {
      // TODO(gee): Will this hang the state machine?
      return;
    }

    AsyncFetch* fetch = this;
    if (rewrite_options_->serve_stale_if_fetch_error() &&
        !fallback_value_.Empty()) {
      // Use a stale value if the fetch from the backend fails.
      fallback_fetch_ = new FallbackSharedAsyncFetch(
          this, &fallback_value_, message_handler_);
      fallback_fetch_->set_fallback_responses_served(
          server_context_->rewrite_stats()->fallback_responses_served());
      fetch = fallback_fetch_;
    }
    if (!fallback_value_.Empty()) {
      // Use the conditional headers in a stale response in cache while
      // triggering the outgoing fetch.
      ConditionalSharedAsyncFetch* conditional_fetch =
          new ConditionalSharedAsyncFetch(
              fetch, &fallback_value_, message_handler_);
      conditional_fetch->set_num_conditional_refreshes(
          server_context_->rewrite_stats()->num_conditional_refreshes());
      fetch = conditional_fetch;
    }
    fetcher_->Fetch(fetch_url_, message_handler_, fetch);
  }

  virtual void HandleDone(bool success) {
    VLOG(2) << response_headers()->ToString();
    bool cached = false;
    // Do not store the response in cache if we are using the fallback.
    if (fallback_fetch_ != NULL && fallback_fetch_->serving_fallback()) {
      success = true;
    } else {
      cached = AddToCache(success && http_value_writer()->has_buffered());
      // Unless the client code explicitly opted into dealing with potentially
      // uncacheable content (by passing in kLoadEvenIfNotCacheable to
      // LoadAsync) we turn it into a fetch failure so we do not
      // end up inadvertently rewriting something that's private or highly
      // volatile.
      if ((!cached && !no_cache_ok_) || !http_value_writer()->has_buffered()) {
        success = false;
      }
    }
    if (http_value()->Empty()) {
      // If there have been no writes so far, write an empty string to the
      // HTTPValue. Note that this is required since empty writes aren't
      // propagated while fetching and we need to write something to the
      // HTTPValue so that we can successfully extract empty content from it.
      http_value()->Write("", message_handler_);
    }
    AsyncFetchWithLock::HandleDone(success);
  }

  virtual void HandleHeadersComplete() {
    if (fallback_fetch_ != NULL && fallback_fetch_->serving_fallback()) {
      response_headers()->ComputeCaching();
    }
    http_value_writer()->CheckCanCacheElseClear(response_headers());
    AsyncFetchWithLock::HandleHeadersComplete();
  }
  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    bool success = http_value_writer()->Write(content, handler);
    return success && AsyncFetchWithLock::HandleWrite(content, handler);
  }

  // The two derived classes differ in how they provide the
  // fields below.  The Async callback gets them from the resource,
  // which must be live at the time it is called.  The ReadIfCached
  // cannot rely on the resource still being alive when the callback
  // is called, so it must keep them locally in the class.
  virtual HTTPValue* http_value() = 0;
  virtual HTTPCache* http_cache() = 0;
  virtual HTTPValueWriter* http_value_writer() = 0;

  void set_no_cache_ok(bool x) { no_cache_ok_ = x; }

 protected:
  ServerContext* server_context_;
  const RewriteOptions* rewrite_options_;
  MessageHandler* message_handler_;

 private:
  virtual bool StartFetch(UrlAsyncFetcher* fetcher, MessageHandler* handler) {
    message_handler_ = handler;
    fetch_url_ = url();
    UrlNamer* url_namer = server_context_->url_namer();
    fetcher_ = fetcher;
    if (!request_headers()->Has(HttpAttributes::kReferer)) {
      if (IsBackgroundFetch()) {
        // Set referer for background fetching, if the referer is missing.
        request_headers()->Add(HttpAttributes::kReferer,
                               driver_->base_url().Spec());
      } else if (driver_->request_headers() != NULL) {
        const char* referer_str = driver_->request_headers()->Lookup1(
            HttpAttributes::kReferer);
        if (referer_str != NULL) {
          request_headers()->Add(HttpAttributes::kReferer, referer_str);
        }
      }
    }

    url_namer->PrepareRequest(
        rewrite_options_,
        &fetch_url_,
        request_headers(),
        NewCallback(this, &UrlResourceFetchCallback::StartFetchInternal),
        message_handler_);
    return true;
  }
  // TODO(jmarantz): consider request_headers.  E.g. will we ever
  // get different resources depending on user-agent?
  HTTPValue fallback_value_;

  // If this is true, loading of non-cacheable resources will succeed.
  bool no_cache_ok_;
  UrlAsyncFetcher* fetcher_;
  RewriteDriver* driver_;
  GoogleString fetch_url_;

  scoped_ptr<NamedLock> lock_;
  const bool respect_vary_;
  const int64 resource_cutoff_ms_;

  FallbackSharedAsyncFetch* fallback_fetch_;

  DISALLOW_COPY_AND_ASSIGN(UrlResourceFetchCallback);
};

// Writes result into cache. Use this when you do not need to wait for the
// response, you just want it to be asynchronously placed in the HttpCache.
//
// For example, this is used for fetches and refreshes of resources
// discovered while rewriting HTML. Note that this uses the Last-Modified and
// If-None-Match headers of the stale value in cache to conditionally refresh
// the resource.
class FreshenFetchCallback : public UrlResourceFetchCallback {
 public:
  FreshenFetchCallback(const GoogleString& url, HTTPCache* http_cache,
                       ServerContext* server_context,
                       RewriteDriver* rewrite_driver,
                       const RewriteOptions* rewrite_options,
                       HTTPValue* fallback_value,
                       Resource::FreshenCallback* callback)
      : UrlResourceFetchCallback(server_context,
                                 rewrite_options,
                                 url,
                                 fallback_value,
                                 rewrite_driver->request_context(),
                                 server_context->message_handler(),
                                 rewrite_driver),
        url_(url),
        http_cache_(http_cache),
        rewrite_driver_(rewrite_driver),
        callback_(callback),
        http_value_writer_(&http_value_, http_cache_) {
    response_headers()->set_implicit_cache_ttl_ms(
        rewrite_options->implicit_cache_ttl_ms());
  }

  virtual void Finalize(bool lock_failure, bool resource_ok) {
    if (callback_ != NULL) {
      if (!lock_failure) {
        resource_ok &= CheckAndUpdateInputInfo(
            *response_headers(), http_value_, *rewrite_options_,
            *rewrite_driver_->server_context(), callback_);
      }
      callback_->Done(lock_failure, resource_ok);
    }
    rewrite_driver_->decrement_async_events_count();
  }

  virtual HTTPValue* http_value() { return &http_value_; }
  virtual HTTPCache* http_cache() { return http_cache_; }
  virtual HTTPValueWriter* http_value_writer() { return &http_value_writer_; }
  virtual bool ShouldYieldToRedundantFetchInProgress() { return true; }
  virtual bool IsBackgroundFetch() const { return true; }

 private:
  GoogleString url_;
  HTTPCache* http_cache_;
  RewriteDriver* rewrite_driver_;
  Resource::FreshenCallback* callback_;
  HTTPValue http_value_;
  HTTPValueWriter http_value_writer_;

  DISALLOW_COPY_AND_ASSIGN(FreshenFetchCallback);
};

// HTTPCache::Callback which checks if we have a fresh response in the cache.
// Note that we don't really care about what the response in cache is. We just
// check whether it is fresh enough to avoid having to trigger an external
// fetch.
class FreshenHttpCacheCallback : public OptionsAwareHTTPCacheCallback {
 public:
  FreshenHttpCacheCallback(const GoogleString& url,
                           ServerContext* server_context,
                           RewriteDriver* driver,
                           const RewriteOptions* options,
                           Resource::FreshenCallback* callback)
      : OptionsAwareHTTPCacheCallback(options, driver->request_context()),
        url_(url),
        server_context_(server_context),
        driver_(driver),
        options_(options),
        callback_(callback) {}

  virtual ~FreshenHttpCacheCallback() {}

  virtual void Done(HTTPCache::FindResult find_result) {
    if (find_result == HTTPCache::kNotFound) {
      // Not found in cache. Invoke the fetcher.
      FreshenFetchCallback* cb = new FreshenFetchCallback(
          url_, server_context_->http_cache(), server_context_, driver_,
          options_, fallback_http_value(), callback_);
      AsyncFetchWithLock::Start(
          driver_->async_fetcher(), cb, server_context_->message_handler());
    } else {
      if (callback_ != NULL) {
        bool success = (find_result == HTTPCache::kFound) &&
            CheckAndUpdateInputInfo(*response_headers(), *http_value(),
                                    *options_, *server_context_, callback_);
        callback_->Done(true, success);
      }
      driver_->decrement_async_events_count();
    }
    delete this;
  }

  // Checks if the response is fresh enough. We may have an imminently
  // expiring resource in the L1 cache, but a fresh response in the L2 cache and
  // regular cache lookups will return the response in the L1.
  virtual bool IsFresh(const ResponseHeaders& headers) {
    int64 date_ms = headers.date_ms();
    int64 expiry_ms = headers.CacheExpirationTimeMs();
    return !ResponseHeaders::IsImminentlyExpiring(
        date_ms, expiry_ms, server_context_->timer()->NowMs());
  }

 private:
  GoogleString url_;
  ServerContext* server_context_;
  RewriteDriver* driver_;
  const RewriteOptions* options_;
  Resource::FreshenCallback* callback_;
  DISALLOW_COPY_AND_ASSIGN(FreshenHttpCacheCallback);
};

bool UrlInputResource::IsValidAndCacheable() const {
  return IsValidAndCacheableImpl(
      server_context()->http_cache(),
      rewrite_options_->min_resource_cache_time_to_rewrite_ms(),
      respect_vary_, response_headers_);
}

void UrlInputResource::Freshen(Resource::FreshenCallback* callback,
                               MessageHandler* handler) {
  // TODO(jmarantz): use if-modified-since
  // For now this is much like Load(), except we do not
  // touch our value, but just the cache
  HTTPCache* http_cache = server_context()->http_cache();
  if (rewrite_driver_ != NULL) {
    // Ensure that the rewrite driver is alive until the freshen is completed.
    rewrite_driver_->increment_async_events_count();
  } else {
    LOG(DFATAL) << "rewrite_driver_ must be non-NULL while freshening";
    return;
  }

  FreshenHttpCacheCallback* freshen_callback = new FreshenHttpCacheCallback(
      url_, server_context(), rewrite_driver_, rewrite_options_, callback);
  // Lookup the cache before doing the fetch since the response may have already
  // been fetched elsewhere.
  http_cache->Find(url_, handler, freshen_callback);
}

// Writes result into a resource. Use this when you need to load a resource
// object and do something specific with the resource once its loaded.
//
// For example, this is used for fetches of output_resources where we don't
// have the input_resource in cache.
class UrlReadAsyncFetchCallback : public UrlResourceFetchCallback {
 public:
  explicit UrlReadAsyncFetchCallback(Resource::AsyncCallback* callback,
                                     UrlInputResource* resource,
                                     const RequestContextPtr& request_context)
      : UrlResourceFetchCallback(resource->server_context(),
                                 resource->rewrite_options(),
                                 resource->url(),
                                 &resource->fallback_value_,
                                 request_context,
                                 resource->server_context()->message_handler(),
                                 resource->rewrite_driver()),
        resource_(resource),
        callback_(callback),
        http_value_writer_(http_value(), http_cache()) {
    set_response_headers(&resource_->response_headers_);
    response_headers()->set_implicit_cache_ttl_ms(
        resource->rewrite_options()->implicit_cache_ttl_ms());
  }

  virtual void Finalize(bool lock_failure, bool resource_ok) {
    if (!lock_failure && resource_ok) {
      resource_->set_fetch_response_status(Resource::kFetchStatusOK);
      // Because we've authorized the Fetcher to directly populate the
      // ResponseHeaders in resource_->response_headers_, we must explicitly
      // propagate the content-type to the resource_->type_.
      resource_->DetermineContentType();
    } else {
      // Record the type of the fetched response before clearing the response
      // headers.
      int status_code = response_headers()->status_code();
      if (status_code >= 400 && status_code < 500) {
        resource_->set_fetch_response_status(Resource::kFetchStatus4xxError);
      } else if (status_code == HttpStatus::kOK &&
                 !response_headers()->IsProxyCacheable()) {
        resource_->set_fetch_response_status(Resource::kFetchStatusUncacheable);
      } else {
        resource_->set_fetch_response_status(Resource::kFetchStatusOther);
      }

      // It's possible that the fetcher has read some of the headers into
      // our response_headers (perhaps even a 200) before it called Done(false)
      // or before we decided inside AddToCache() that we don't want to deal
      // with this particular resource. In that case, make sure to clear the
      // response_headers() so the various validity bits in Resource are
      // accurate.
      response_headers()->Clear();
    }

    Statistics* stats = resource_->server_context()->statistics();
    if (resource_ok) {
      stats->GetVariable(RewriteStats::kNumResourceFetchSuccesses)->Add(1);
    } else {
      stats->GetVariable(RewriteStats::kNumResourceFetchFailures)->Add(1);
    }
    callback_->Done(lock_failure, resource_ok);
  }

  virtual bool IsBackgroundFetch() const {
    return resource_->is_background_fetch();
  }

  virtual HTTPValue* http_value() { return &resource_->value_; }
  virtual HTTPCache* http_cache() {
    return resource_->server_context()->http_cache();
  }
  virtual HTTPValueWriter* http_value_writer() { return &http_value_writer_; }
  virtual bool ShouldYieldToRedundantFetchInProgress() { return false; }

 private:
  UrlInputResource* resource_;
  Resource::AsyncCallback* callback_;
  HTTPValueWriter http_value_writer_;

  DISALLOW_COPY_AND_ASSIGN(UrlReadAsyncFetchCallback);
};

void UrlInputResource::LoadAndSaveToCache(NotCacheablePolicy no_cache_policy,
                                          AsyncCallback* callback,
                                          MessageHandler* message_handler) {
  CHECK(callback != NULL) << "A callback must be supplied, or else it will "
      "not be possible to determine when it's safe to delete the resource.";
  CHECK(this == callback->resource().get())
      << "The callback must keep a reference to the resource";
  CHECK(rewrite_driver_ != NULL)
      << "Must provide a RewriteDriver for resources that will get fetched.";
  DCHECK(!loaded()) << "Shouldn't get this far if already loaded.";
  UrlReadAsyncFetchCallback* cb =
      new UrlReadAsyncFetchCallback(callback,
                                    this,
                                    rewrite_driver_->request_context());
  if (no_cache_policy == Resource::kLoadEvenIfNotCacheable) {
    cb->set_no_cache_ok(true);
  }
  AsyncFetchWithLock::Start(
      rewrite_driver_->async_fetcher(),
      cb,
      server_context_->message_handler());
}

}  // namespace net_instaweb
