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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/cacheable_resource_base.h"

#include "base/logging.h"               // for operator<<, etc
#include "net/instaweb/config/rewrite_options_manager.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/async_fetch_with_lock.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/http_value_writer.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/util/public/basictypes.h"        // for int64
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/timer.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

namespace {

const char kHitSuffix[] = "_hit";
const char kRecentFetchFailureSuffix[] = "_recent_fetch_failure";
const char kRecentUncacheableTreatedAsMiss[] =
    "_recent_uncacheable_treated_as_miss";
const char kRecentUncacheableTreatedAsFailure[] =
    "_recent_uncacheable_treated_as_failure";
const char kMissSuffix[] = "_miss";

}  // namespace

// Shared base class for fetch callbacks, used by both LoadAndCallback and
// Freshen.
class CacheableResourceBase::FetchCallbackBase : public AsyncFetchWithLock {
 public:
  FetchCallbackBase(ServerContext* server_context,
                    const RewriteOptions* rewrite_options,
                    const GoogleString& url,
                    const GoogleString& cache_key,
                    HTTPValue* fallback_value,
                    const RequestContextPtr& request_context,
                    MessageHandler* handler,
                    RewriteDriver* driver,
                    CacheableResourceBase* resource)
      : AsyncFetchWithLock(server_context->lock_hasher(),
                           request_context,
                           url,
                           cache_key,
                           server_context->lock_manager(),
                           handler),
        resource_(resource),
        server_context_(server_context),
        driver_(driver),
        rewrite_options_(rewrite_options),
        message_handler_(handler),
        no_cache_ok_(false),
        fetcher_(NULL),
        fallback_fetch_(NULL) {
    if (fallback_value != NULL) {
      fallback_value_.Link(fallback_value);
    }
  }

  virtual ~FetchCallbackBase() {}

  // Set this to true if implementing a kLoadEvenIfNotCacheable policy.
  void set_no_cache_ok(bool x) { no_cache_ok_ = x; }

 protected:
  // The two derived classes differ in how they provide the
  // fields below. LoadAndCallback updates the resource directly, while
  // Freshen does not actually change the resource object.
  virtual HTTPValue* http_value() = 0;
  virtual HTTPCache* http_cache() = 0;
  virtual HTTPValueWriter* http_value_writer() = 0;

  // Subclasses will also want to override AsyncFetchWithLock's Finalize
  // to get all the cases.

  // Overridden from AsyncFetch.
  virtual void HandleDone(bool success) {
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

  // Overridden from AsyncFetch.
  virtual void HandleHeadersComplete() {
    if (fallback_fetch_ != NULL && fallback_fetch_->serving_fallback()) {
      response_headers()->ComputeCaching();
    }
    http_value_writer()->CheckCanCacheElseClear(response_headers());
    AsyncFetchWithLock::HandleHeadersComplete();
  }

  // Overridden from AsyncFetch.
  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    bool success = http_value_writer()->Write(content, handler);
    return success && AsyncFetchWithLock::HandleWrite(content, handler);
  }

  // Overridden from AsyncFetchWithLock.
  virtual bool StartFetch(UrlAsyncFetcher* fetcher, MessageHandler* handler) {
    fetch_url_ = url();
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

    server_context_->rewrite_options_manager()->PrepareRequest(
        rewrite_options_,
        &fetch_url_,
        request_headers(),
        NewCallback(this, &FetchCallbackBase::PrepareRequestDone));
    return true;
  }

  void PrepareRequestDone(bool success) {
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
    resource_->PrepareRequest(fetch->request_context(),
                              fetch->request_headers());
    fetcher_->Fetch(fetch_url_, message_handler_, fetch);
  }

 private:
  // Returns true if the result was successfully cached.
  bool AddToCache(bool success) {
    ResponseHeaders* headers = response_headers();
    // Merge in any extra response headers.
    headers->UpdateFrom(*extra_response_headers());
    resource_->PrepareResponseHeaders(headers);
    headers->ComputeCaching();
    headers->FixDateHeaders(http_cache()->timer()->NowMs());
    if (success && !headers->IsErrorStatus()) {
      if (rewrite_options_->IsCacheTtlOverridden(url())) {
        headers->ForceCaching(rewrite_options_->override_caching_ttl_ms());
      }
      if (resource_->IsValidAndCacheableImpl(*headers)) {
        HTTPValue* value = http_value();
        value->SetHeaders(headers);

        // Note that we could potentially store Vary:Cookie responses
        // here, as we will have fetched the resource without cookies.
        // But we must be careful in the mod_pagespeed ipro flow,
        // where we must avoid storing any resource obtained with a
        // Cookie.  For now we don't implement this.
        http_cache()->Put(resource_->cache_key(), driver_->CacheFragment(),
                          RequestHeaders::Properties(),
                          resource_->respect_vary(), value, message_handler_);
        return true;
      } else {
        http_cache()->RememberNotCacheable(
            resource_->cache_key(), driver_->CacheFragment(),
            headers->status_code() == HttpStatus::kOK,
            message_handler_);
      }
    } else {
      if (headers->Has(HttpAttributes::kXPsaLoadShed)) {
        http_cache()->RememberFetchDropped(resource_->cache_key(),
                                           driver_->CacheFragment(),
                                           message_handler_);
      } else {
        http_cache()->RememberFetchFailed(resource_->cache_key(),
                                          driver_->CacheFragment(),
                                          message_handler_);
      }
    }
    return false;
  }

  CacheableResourceBase* resource_;  // owned by the callback.
  ServerContext* server_context_;
  RewriteDriver* driver_;
  const RewriteOptions* rewrite_options_;
  MessageHandler* message_handler_;

  // TODO(jmarantz): consider request_headers.  E.g. will we ever
  // get different resources depending on user-agent?
  HTTPValue fallback_value_;

  // If this is true, loading of non-cacheable resources will succeed.
  // Used to implement kLoadEvenIfNotCacheable.
  bool no_cache_ok_;

  // These 2 are set only once we get to StartFetch
  UrlAsyncFetcher* fetcher_;
  GoogleString fetch_url_;

  FallbackSharedAsyncFetch* fallback_fetch_;

  DISALLOW_COPY_AND_ASSIGN(FetchCallbackBase);
};

// Writes result into cache. Use this when you do not need to wait for the
// response, you just want it to be asynchronously placed in the HttpCache.
//
// For example, this is used for fetches and refreshes of resources
// discovered while rewriting HTML. Note that this uses the Last-Modified and
// If-None-Match headers of the stale value in cache to conditionally refresh
// the resource.
class CacheableResourceBase::FreshenFetchCallback : public FetchCallbackBase {
 public:
  FreshenFetchCallback(const GoogleString& url,
                       const GoogleString& cache_key,
                       HTTPCache* http_cache,
                       ServerContext* server_context,
                       RewriteDriver* rewrite_driver,
                       const RewriteOptions* rewrite_options,
                       HTTPValue* fallback_value,
                       CacheableResourceBase* resource,
                       Resource::FreshenCallback* callback)
      : FetchCallbackBase(server_context,
                          rewrite_options,
                          url,
                          cache_key,
                          fallback_value,
                          rewrite_driver->request_context(),
                          server_context->message_handler(),
                          rewrite_driver,
                          resource),
        url_(url),
        http_cache_(http_cache),
        rewrite_driver_(rewrite_driver),
        callback_(callback),
        http_value_writer_(&http_value_, http_cache_),
        resource_(resource),
        own_resource_(resource) {
    // TODO(morlovich): This is duplicated a few times, clean this up.
    response_headers()->set_implicit_cache_ttl_ms(
        rewrite_options->implicit_cache_ttl_ms());
  }

  virtual void Finalize(bool lock_failure, bool resource_ok) {
    if (callback_ != NULL) {
      if (!lock_failure) {
        resource_ok &= resource_->UpdateInputInfoForFreshen(
            *response_headers(), http_value_, callback_);
      }
      callback_->Done(lock_failure, resource_ok);
    }
    rewrite_driver_->decrement_async_events_count();
    // AsyncFetchWithLock::HandleDone (which calls this method)
    // will take care of deleting 'this'.
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
  CacheableResourceBase* resource_;
  ResourcePtr own_resource_;  // keep alive resource since callback may be NULL

  DISALLOW_COPY_AND_ASSIGN(FreshenFetchCallback);
};

// Fetch callback that writes result directly into a resource.
class CacheableResourceBase::LoadFetchCallback
    : public FetchCallbackBase {
 public:
  explicit LoadFetchCallback(Resource::AsyncCallback* callback,
                             CacheableResourceBase* resource,
                             const RequestContextPtr& request_context)
      : FetchCallbackBase(resource->server_context(),
                          resource->rewrite_options(),
                          resource->url(),
                          resource->cache_key(),
                          &resource->fallback_value_,
                          request_context,
                          resource->server_context()->message_handler(),
                          resource->rewrite_driver(),
                          resource),
        resource_(resource),
        callback_(callback),
        http_value_writer_(http_value(), http_cache()),
        respect_vary_(resource->respect_vary()) {
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
      ResponseHeaders* headers = response_headers();
      int status_code = headers->status_code();
      if (headers->Has(HttpAttributes::kXPsaLoadShed)) {
        resource_->set_fetch_response_status(Resource::kFetchStatusDropped);
      } else if (status_code >= 400 && status_code < 500) {
        resource_->set_fetch_response_status(Resource::kFetchStatus4xxError);
      } else if (status_code == HttpStatus::kOK &&
                 !headers->IsProxyCacheable(RequestHeaders::Properties(),
                                            respect_vary_,
                                            ResponseHeaders::kNoValidator)) {
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
      headers->Clear();
    }

    Statistics* stats = resource_->server_context()->statistics();
    if (resource_ok) {
      stats->GetVariable(RewriteStats::kNumResourceFetchSuccesses)->Add(1);
    } else {
      stats->GetVariable(RewriteStats::kNumResourceFetchFailures)->Add(1);
    }
    callback_->Done(lock_failure, resource_ok);
    // AsyncFetchWithLock will delete 'this' eventually.
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
  CacheableResourceBase* resource_;
  Resource::AsyncCallback* callback_;
  HTTPValueWriter http_value_writer_;
  ResponseHeaders::VaryOption respect_vary_;

  DISALLOW_COPY_AND_ASSIGN(LoadFetchCallback);
};

class CacheableResourceBase::LoadHttpCacheCallback
    : public OptionsAwareHTTPCacheCallback {
 public:
  LoadHttpCacheCallback(
      const RequestContextPtr& request_context,
      NotCacheablePolicy not_cacheable_policy,
      AsyncCallback* resource_callback,
      CacheableResourceBase* resource);
  virtual ~LoadHttpCacheCallback();
  virtual void Done(HTTPCache::FindResult find_result);

 private:
  void LoadAndSaveToCache();

  // protected via resource_callback_->resource().
  CacheableResourceBase* resource_;
  Resource::AsyncCallback* resource_callback_;
  Resource::NotCacheablePolicy not_cacheable_policy_;
  DISALLOW_COPY_AND_ASSIGN(LoadHttpCacheCallback);
};

CacheableResourceBase::LoadHttpCacheCallback::LoadHttpCacheCallback(
    const RequestContextPtr& request_context,
    NotCacheablePolicy not_cacheable_policy,
    AsyncCallback* resource_callback,
    CacheableResourceBase* resource)
    : OptionsAwareHTTPCacheCallback(
          resource->rewrite_options(), request_context),
      resource_(resource),
      resource_callback_(resource_callback),
      not_cacheable_policy_(not_cacheable_policy) {
}

CacheableResourceBase::LoadHttpCacheCallback::~LoadHttpCacheCallback() {
}

void CacheableResourceBase::LoadHttpCacheCallback::Done(
    HTTPCache::FindResult find_result) {
  MessageHandler* handler = resource_->message_handler();

  // Note, we pass lock_failure==false to the resource callbacks
  // when we are taking action based on the cache.  We haven't locked,
  // but we didn't fail-to-lock.  Resource callbacks need to know if
  // the lock failed, because they will delete expired cache metadata
  // if they have the lock, or if the lock was not needed, but they
  // should not delete it if they fail to lock.
  switch (find_result) {
    case HTTPCache::kFound:
      resource_->hits_->Add(1);
      resource_->Link(http_value(), handler);
      resource_->response_headers()->CopyFrom(*response_headers());
      resource_->DetermineContentType();
      resource_->RefreshIfImminentlyExpiring();
      resource_callback_->Done(false /* lock_failure */,
                               true /* resource_ok */);
      break;
    case HTTPCache::kRecentFetchFailed:
      resource_->recent_fetch_failures_->Add(1);
      // TODO(jmarantz): in this path, should we try to fetch again
      // sooner than 5 minutes, especially if this is not a background
      // fetch, but rather one for serving the user? This could get
      // frustrating, even if the software is functioning as intended,
      // because a missing resource that is put in place by a site
      // admin will not be checked again for 5 minutes.
      //
      // The "good" news is that if the admin is willing to crank up
      // logging to 'info' then http_cache.cc will log the
      // 'remembered' failure.
      resource_callback_->Done(false /* lock_failure */,
                               false /* resource_ok */);
      break;
    case HTTPCache::kRecentFetchNotCacheable:
      switch (not_cacheable_policy_) {
        case Resource::kLoadEvenIfNotCacheable:
          resource_->recent_uncacheables_treated_as_miss_->Add(1);
          LoadAndSaveToCache();
          break;
        case Resource::kReportFailureIfNotCacheable:
          resource_->recent_uncacheables_treated_as_failure_->Add(1);
          resource_callback_->Done(false /* lock_failure */,
                                   false /* resource_ok */);
          break;
        default:
          LOG(DFATAL) << "Unexpected not_cacheable_policy_!";
          resource_callback_->Done(false /* lock_failure */,
                                   false /* resource_ok */);
          break;
      }
      break;
    case HTTPCache::kNotFound:
      resource_->misses_->Add(1);
      // If not, load it asynchronously.
      // Link the fallback value which can be used if the fetch fails.
      resource_->LinkFallbackValue(fallback_http_value());
      LoadAndSaveToCache();
      break;
  }
  delete this;
}

void CacheableResourceBase::LoadHttpCacheCallback::LoadAndSaveToCache() {
  if (resource_->ShouldSkipBackgroundFetch()) {
    // Note that this isn't really a lock failure, but we treat them the same
    // way.
    resource_callback_->Done(true /* lock_failure */,
                             false /* resource_ok */);
    return;
  }
  CHECK(resource_callback_ != NULL)
      << "A callback must be supplied, or else it will "
          "not be possible to determine when it's safe to delete the resource.";
  CHECK(resource_ == resource_callback_->resource().get())
      << "The callback must keep a reference to the resource";
  DCHECK(!resource_->loaded()) << "Shouldn't get this far if already loaded.";
  LoadFetchCallback* cb =
      new LoadFetchCallback(resource_callback_, resource_, request_context());
  if (not_cacheable_policy_ == Resource::kLoadEvenIfNotCacheable) {
    cb->set_no_cache_ok(true);
  }
  cb->Start(resource_->rewrite_driver()->async_fetcher());
}

// HTTPCache::Callback which checks if we have a fresh response in the cache.
// Note that we don't really care about what the response in cache is. We just
// check whether it is fresh enough to avoid having to trigger an external
// fetch. This keeps the RewriteDriver alive via the async event count.
class CacheableResourceBase::FreshenHttpCacheCallback
    : public OptionsAwareHTTPCacheCallback {
 public:
  FreshenHttpCacheCallback(const GoogleString& url,
                           const GoogleString& cache_key,
                           ServerContext* server_context,
                           RewriteDriver* driver,
                           const RewriteOptions* options,
                           CacheableResourceBase* resource,
                           Resource::FreshenCallback* callback)
      : OptionsAwareHTTPCacheCallback(options, driver->request_context()),
        url_(url),
        cache_key_(cache_key),
        server_context_(server_context),
        driver_(driver),
        options_(options),
        resource_(resource),
        own_resource_(resource),
        callback_(callback) {}

  virtual ~FreshenHttpCacheCallback() {}

  virtual void Done(HTTPCache::FindResult find_result) {
    if (find_result == HTTPCache::kNotFound &&
        !resource_->ShouldSkipBackgroundFetch()) {
      // Not found in cache. Invoke the fetcher.
      FreshenFetchCallback* cb = new FreshenFetchCallback(
          url_, cache_key_, server_context_->http_cache(), server_context_,
          driver_, options_, fallback_http_value(), resource_, callback_);
      cb->Start(driver_->async_fetcher());
    } else {
      if (callback_ != NULL) {
        bool success = (find_result == HTTPCache::kFound) &&
                       resource_->UpdateInputInfoForFreshen(
                           *response_headers(), *http_value(), callback_);
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
  GoogleString cache_key_;
  ServerContext* server_context_;
  RewriteDriver* driver_;
  const RewriteOptions* options_;
  CacheableResourceBase* resource_;
  // Note that we need to own the resource since callback_ might be NULL.
  ResourcePtr own_resource_;
  Resource::FreshenCallback* callback_;
  DISALLOW_COPY_AND_ASSIGN(FreshenHttpCacheCallback);
};

CacheableResourceBase::CacheableResourceBase(
    StringPiece stat_prefix,
    StringPiece url,
    StringPiece cache_key,
    const ContentType* type,
    RewriteDriver* rewrite_driver)
    : Resource(rewrite_driver->server_context(), type),
      url_(url.data(), url.size()),
      cache_key_(cache_key.data(), cache_key.size()),
      rewrite_driver_(rewrite_driver) {
  set_enable_cache_purge(rewrite_options()->enable_cache_purge());
  set_respect_vary(ResponseHeaders::GetVaryOption(
      rewrite_options()->respect_vary()));
  set_proactive_resource_freshening(
      rewrite_options()->proactive_resource_freshening());

  Statistics* stats = server_context()->statistics();
  hits_ = stats->GetVariable(StrCat(stat_prefix, kHitSuffix));
  recent_fetch_failures_ =
      stats->GetVariable(StrCat(stat_prefix, kRecentFetchFailureSuffix));
  recent_uncacheables_treated_as_miss_ =
      stats->GetVariable(StrCat(stat_prefix, kRecentUncacheableTreatedAsMiss));
  recent_uncacheables_treated_as_failure_ =
      stats->GetVariable(StrCat(stat_prefix,
                                kRecentUncacheableTreatedAsFailure));
  misses_ = stats->GetVariable(StrCat(stat_prefix, kMissSuffix));
}

CacheableResourceBase::~CacheableResourceBase() {
}

bool CacheableResourceBase::IsValidAndCacheable() const {
  return IsValidAndCacheableImpl(*response_headers());
}

bool CacheableResourceBase::IsValidAndCacheableImpl(
    const ResponseHeaders& headers) const {
  if (headers.status_code() != HttpStatus::kOK) {
    return false;
  }

  // Conservatively assume that the request has cookies, since the site may
  // want to serve different content based on the cookie. If we consider the
  // response to be cacheable here, we will serve the optimized version
  // without contacting the origin which would be against the webmaster's
  // intent. We also don't have cookies available at lookup time, so we
  // cannot try to use this response only when the request doesn't have a
  // cookie.
  RequestHeaders::Properties req_properties;
  bool cacheable = headers.IsProxyCacheable(req_properties, respect_vary(),
                                            ResponseHeaders::kNoValidator);

  // If we are setting a TTL for HTML, we cannot rewrite any resource
  // with a shorter TTL.
  cacheable &= (headers.cache_ttl_ms() >=
                rewrite_options()->min_resource_cache_time_to_rewrite_ms());

  if (!cacheable && !http_cache()->force_caching()) {
    return false;
  }

  return !http_cache()->IsExpired(headers);
}

void CacheableResourceBase::InitStats(StringPiece stat_prefix,
                                      Statistics* stats) {
  stats->AddVariable(StrCat(stat_prefix, kHitSuffix));
  stats->AddVariable(StrCat(stat_prefix, kRecentFetchFailureSuffix));
  stats->AddVariable(StrCat(stat_prefix, kRecentUncacheableTreatedAsMiss));
  stats->AddVariable(StrCat(stat_prefix,
                            kRecentUncacheableTreatedAsFailure));
  stats->AddVariable(StrCat(stat_prefix, kMissSuffix));
}

void CacheableResourceBase::RefreshIfImminentlyExpiring() {
  if (!http_cache()->force_caching()) {
    const ResponseHeaders* headers = response_headers();
    int64 start_date_ms = headers->date_ms();
    int64 expire_ms = headers->CacheExpirationTimeMs();
    if (ResponseHeaders::IsImminentlyExpiring(
        start_date_ms, expire_ms, timer()->NowMs())) {
      Freshen(NULL, server_context()->message_handler());
    }
  }
}

void CacheableResourceBase::LoadAndCallback(
    NotCacheablePolicy not_cacheable_policy,
    const RequestContextPtr& request_context,
    AsyncCallback* callback) {
  LoadHttpCacheCallback* cache_callback =
      new LoadHttpCacheCallback(request_context, not_cacheable_policy,
                                callback, this);

  cache_callback->set_is_background(is_background_fetch());
  http_cache()->Find(cache_key(), rewrite_driver()->CacheFragment(),
                     message_handler(), cache_callback);
}

void CacheableResourceBase::Freshen(Resource::FreshenCallback* callback,
                                    MessageHandler* handler) {
  // TODO(jmarantz): use if-modified-since
  // For now this is much like Load(), except we do not
  // touch our value, but just the cache
  HTTPCache* http_cache = server_context()->http_cache();
  // Ensure that the rewrite driver is alive until the freshen is completed.
  rewrite_driver_->increment_async_events_count();

  FreshenHttpCacheCallback* freshen_callback = new FreshenHttpCacheCallback(
      url(), cache_key(), server_context(), rewrite_driver_, rewrite_options(),
      this, callback);
  // Lookup the cache before doing the fetch since the response may have already
  // been fetched elsewhere.
  http_cache->Find(cache_key(), rewrite_driver()->CacheFragment(),
                   handler, freshen_callback);
}

bool CacheableResourceBase::UpdateInputInfoForFreshen(
    const ResponseHeaders& headers,
    const HTTPValue& value,
    Resource::FreshenCallback* callback) {
  InputInfo* input_info = callback->input_info();
  if (input_info != NULL && input_info->has_input_content_hash() &&
      IsValidAndCacheableImpl(headers)) {
    StringPiece content;
    if (value.ExtractContents(&content)) {
      GoogleString new_hash =
          server_context()->contents_hasher()->Hash(content);
      // TODO(nikhilmadan): Consider using the Etag / Last-Modified header to
      // validate if the resource has changed instead of computing the hash.
      if (new_hash == input_info->input_content_hash()) {
        FillInPartitionInputInfoFromResponseHeaders(headers, input_info);
        return true;
      }
    }
  }
  return false;
}

const RewriteOptions* CacheableResourceBase::rewrite_options() const {
  return rewrite_driver_->options();
}

void CacheableResourceBase::PrepareRequest(
    const RequestContextPtr& request_context, RequestHeaders* headers) {
}

void CacheableResourceBase::PrepareResponseHeaders(ResponseHeaders* headers) {
}

bool CacheableResourceBase::ShouldSkipBackgroundFetch() const {
  return is_background_fetch() &&
      rewrite_options()->disable_background_fetches_for_bots() &&
      rewrite_driver()->request_properties()->IsBot();
}

}  // namespace net_instaweb
