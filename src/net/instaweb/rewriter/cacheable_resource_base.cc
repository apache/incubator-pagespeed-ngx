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
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/basictypes.h"        // for int64
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

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
      resource_->Link(http_value(), handler);
      resource_->response_headers()->CopyFrom(*response_headers());
      resource_->DetermineContentType();
      resource_->RefreshIfImminentlyExpiring();
      resource_callback_->Done(false /* lock_failure */,
                               true /* resource_ok */);
      break;
    case HTTPCache::kRecentFetchFailed:
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
          resource_->LoadAndSaveToCache(not_cacheable_policy_,
                                        resource_callback_, handler);
          break;
        case Resource::kReportFailureIfNotCacheable:
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
      // If not, load it asynchronously.
      // Link the fallback value which can be used if the fetch fails.
      resource_->LinkFallbackValue(fallback_http_value());
      resource_->LoadAndSaveToCache(not_cacheable_policy_,
                                    resource_callback_, handler);
      break;
  }
  delete this;
}

CacheableResourceBase::~CacheableResourceBase() {
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
  http_cache()->Find(url(), message_handler(), cache_callback);
}

}  // namespace net_instaweb
