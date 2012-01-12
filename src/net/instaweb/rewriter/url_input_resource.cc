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
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

bool IsValidAndCacheableImpl(HTTPCache* http_cache,
                             int64 min_cache_time_to_rewrite_ms,
                             bool respect_vary,
                             const ResponseHeaders* headers) {
  if (headers->status_code() != HttpStatus::kOK) {
    return false;
  }

  bool cacheable = true;
  if (respect_vary) {
    cacheable = headers->VaryCacheable();
  } else {
    cacheable = headers->IsCacheable();
  }
  // If we are setting a TTL for HTML, we cannot rewrite any resource
  // with a shorter TTL.
  cacheable &= (headers->cache_ttl_ms() >= min_cache_time_to_rewrite_ms);

  if (!cacheable && !http_cache->force_caching()) {
    return false;
  }

  return !http_cache->IsAlreadyExpired(*headers);
}

}  // namespace

UrlInputResource::~UrlInputResource() {
}

// Shared fetch callback, used by both Load and LoadAndCallback
class UrlResourceFetchCallback : public AsyncFetch {
 public:
  UrlResourceFetchCallback(ResourceManager* resource_manager,
                           const RewriteOptions* rewrite_options,
                           HTTPValue* fallback_value) :
      resource_manager_(resource_manager),
      rewrite_options_(rewrite_options),
      message_handler_(NULL),
      fallback_value_(fallback_value),
      success_(false),
      fetcher_(NULL),
      respect_vary_(rewrite_options->respect_vary()),
      resource_cutoff_ms_(
          rewrite_options->min_resource_cache_time_to_rewrite_ms()),
      fallback_fetch_(NULL) {
    // We intentionally do *not* keep a pointer to rewrite_options because
    // the pointer may not be valid at callback time.
  }
  virtual ~UrlResourceFetchCallback() {}

  bool Fetch(UrlAsyncFetcher* fetcher, MessageHandler* handler) {
    message_handler_ = handler;
    GoogleString lock_name =
        StrCat(resource_manager_->lock_hasher()->Hash(url()), ".lock");
    lock_.reset(resource_manager_->lock_manager()->CreateNamedLock(lock_name));
    int64 lock_timeout = fetcher->timeout_ms();
    if (lock_timeout == UrlAsyncFetcher::kUnspecifiedTimeout) {
      // Even if the fetcher never explicitly times out requests, they probably
      // won't succeed after more than 2 minutes.
      lock_timeout = 2 * Timer::kMinuteMs;
    } else {
      // Give a little slack for polling, writing the file, freeing the lock.
      lock_timeout *= 2;
    }
    if (!lock_->TryLockStealOld(lock_timeout)) {
      lock_.reset(NULL);
      // TODO(abliss): a per-unit-time statistic would be useful here.
      if (should_yield()) {
        message_handler_->Message(
            kInfo, "%s is already being fetched (lock %s)",
            url().c_str(), lock_name.c_str());
        DoneInternal(false);
        delete this;
        return false;
      }
      message_handler_->Message(
          kInfo, "%s is being re-fetched asynchronously "
          "(lock %s held elsewhere)", url().c_str(), lock_name.c_str());
    } else {
      message_handler_->Message(kInfo, "%s: Locking (lock %s)",
                                url().c_str(), lock_name.c_str());
    }

    fetch_url_ = url();

    UrlNamer* url_namer = resource_manager_->url_namer();

    fetcher_ = fetcher;

    url_namer->PrepareRequest(rewrite_options_, &fetch_url_, request_headers(),
        &success_,
        MakeFunction(this, &UrlResourceFetchCallback::StartFetchInternal),
        message_handler_);
    return true;
  }

  bool AddToCache(bool success) {
    ResponseHeaders* headers = response_headers();
    headers->FixDateHeaders(http_cache()->timer()->NowMs());
    if (success && !headers->IsErrorStatus()) {
      if (IsValidAndCacheableImpl(http_cache(), resource_cutoff_ms_,
                                  respect_vary_, headers)) {
        HTTPValue* value = http_value();
        value->SetHeaders(headers);
        http_cache()->Put(url(), value, message_handler_);
        return true;
      } else {
        http_cache()->RememberNotCacheable(url(), message_handler_);
      }
    } else {
      http_cache()->RememberFetchFailed(url(), message_handler_);
    }
    return false;
  }

  void StartFetchInternal() {
    if (!success_) {
      return;
    }
    // TODO(sligocki): Allow ConditionalFetch here
    AsyncFetch* fetch = this;
    if (rewrite_options_->serve_stale_if_fetch_error() &&
        fallback_value_ != NULL && !fallback_value_->Empty()) {
      fallback_fetch_ = new FallbackSharedAsyncFetch(
          this, fallback_value_, message_handler_);
      fallback_fetch_->set_fallback_responses_served(
          resource_manager_->rewrite_stats()->fallback_responses_served());
      fetch = fallback_fetch_;
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
      cached = AddToCache(success);
    }
    if (lock_.get() != NULL) {
      message_handler_->Message(
          kInfo, "%s: Unlocking lock %s with cached=%s, success=%s",
          url().c_str(), lock_->name().c_str(),
          cached ? "true" : "false",
          success ? "true" : "false");
      lock_->Unlock();
      lock_.reset(NULL);
    }
    DoneInternal(success);
    delete this;
  }

  virtual void HandleHeadersComplete() {
    if (fallback_fetch_ != NULL && fallback_fetch_->serving_fallback()) {
      response_headers()->ComputeCaching();
    }
  }
  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    return http_value()->Write(content, handler);
  }
  virtual bool HandleFlush(MessageHandler* handler) {
    return true;
  }

  // The two derived classes differ in how they provide the
  // fields below.  The Async callback gets them from the resource,
  // which must be live at the time it is called.  The ReadIfCached
  // cannot rely on the resource still being alive when the callback
  // is called, so it must keep them locally in the class.
  virtual HTTPValue* http_value() = 0;
  virtual GoogleString url() const = 0;
  virtual HTTPCache* http_cache() = 0;
  // If someone is already fetching this resource, should we yield to them and
  // try again later?  If so, return true.  Otherwise, if we must fetch the
  // resource regardless, return false.
  // TODO(abliss): unit test this
  virtual bool should_yield() = 0;

  // Indicate that it's OK for the callback to be executed on a different
  // thread, as it only populates the cache, which is thread-safe.
  virtual bool EnableThreaded() const { return true; }

 protected:
  virtual void DoneInternal(bool success) {
  }

  ResourceManager* resource_manager_;
  const RewriteOptions* rewrite_options_;
  MessageHandler* message_handler_;

 private:
  // TODO(jmarantz): consider request_headers.  E.g. will we ever
  // get different resources depending on user-agent?
  HTTPValue* fallback_value_;
  bool success_;
  UrlAsyncFetcher* fetcher_;
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
// discovered while rewriting HTML.
class UrlReadIfCachedCallback : public UrlResourceFetchCallback {
 public:
  UrlReadIfCachedCallback(const GoogleString& url, HTTPCache* http_cache,
                          ResourceManager* resource_manager,
                          const RewriteOptions* rewrite_options)
      : UrlResourceFetchCallback(resource_manager, rewrite_options, NULL),
        url_(url),
        http_cache_(http_cache) {
  }

  virtual HTTPValue* http_value() { return &http_value_; }
  virtual GoogleString url() const { return url_; }
  virtual HTTPCache* http_cache() { return http_cache_; }
  virtual bool should_yield() { return true; }

 private:
  GoogleString url_;
  HTTPCache* http_cache_;
  HTTPValue http_value_;

  DISALLOW_COPY_AND_ASSIGN(UrlReadIfCachedCallback);
};

bool UrlInputResource::IsValidAndCacheable() const {
  return IsValidAndCacheableImpl(
      resource_manager()->http_cache(),
      rewrite_options_->min_resource_cache_time_to_rewrite_ms(),
      respect_vary_, &response_headers_);
}

bool UrlInputResource::Load(MessageHandler* handler) {
  response_headers_.Clear();
  value_.Clear();

  HTTPCache* http_cache = resource_manager()->http_cache();
  UrlReadIfCachedCallback* cb = new UrlReadIfCachedCallback(
      url_, http_cache, resource_manager(), rewrite_options_);

  // If the fetcher can satisfy the request instantly, then we
  // can try to populate the resource from the cache.
  //
  // TODO(jmarantz): populate directly from Fetch callback rather
  // than having to deserialize from cache.
  bool data_available =
      (cb->Fetch(resource_manager_->url_async_fetcher(), handler) &&
       (http_cache->Find(url_, &value_, &response_headers_, handler) ==
        HTTPCache::kFound));
  return data_available;
}

void UrlInputResource::Freshen(MessageHandler* handler) {
  // TODO(jmarantz): use if-modified-since
  // For now this is much like Load(), except we do not
  // touch our value, but just the cache
  HTTPCache* http_cache = resource_manager()->http_cache();
  UrlReadIfCachedCallback* cb = new UrlReadIfCachedCallback(
      url_, http_cache, resource_manager(), rewrite_options_);
  // TODO(sligocki): Ask for Conditional fetch here.
  cb->Fetch(resource_manager_->url_async_fetcher(), handler);
}

// Writes result into a resource. Use this when you need to load a resource
// object and do something specific with the resource once its loaded.
//
// For example, this is used for fetches of output_resources where we don't
// have the input_resource in cache.
class UrlReadAsyncFetchCallback : public UrlResourceFetchCallback {
 public:
  explicit UrlReadAsyncFetchCallback(Resource::AsyncCallback* callback,
                                     UrlInputResource* resource)
      : UrlResourceFetchCallback(resource->resource_manager(),
                                 resource->rewrite_options(),
                                 &resource->fallback_value_),
        resource_(resource),
        callback_(callback) {
    set_response_headers(&resource_->response_headers_);
  }

  virtual void DoneInternal(bool success) {
    if (success) {
      // Because we've authorized the Fetcher to directly populate the
      // ResponseHeaders in resource_->response_headers_, we must explicitly
      // propagate the content-type to the resource_->type_.
      resource_->DetermineContentType();
    } else {
      // It's possible that the fetcher has read some of the headers into
      // our response_headers (perhaps even a 200) before it called Done(false)
      // or before we decided inside AddToCache() that we don't want to deal
      // with this particular resource. In that case, make sure to clear the
      // response_headers() so the various validity bits in Resource are
      // accurate.
      response_headers()->Clear();
    }

    callback_->Done(success);
  }

  virtual bool EnableThreaded() const { return callback_->EnableThreaded(); }

  virtual HTTPValue* http_value() { return &resource_->value_; }
  virtual GoogleString url() const { return resource_->url(); }
  virtual HTTPCache* http_cache() {
    return resource_->resource_manager()->http_cache();
  }
  virtual bool should_yield() { return false; }

 private:
  UrlInputResource* resource_;
  Resource::AsyncCallback* callback_;

  DISALLOW_COPY_AND_ASSIGN(UrlReadAsyncFetchCallback);
};

void UrlInputResource::LoadAndCallback(AsyncCallback* callback,
                                       MessageHandler* message_handler) {
  CHECK(callback != NULL) << "A callback must be supplied, or else it will "
      "not be possible to determine when it's safe to delete the resource.";
  CHECK(this == callback->resource().get())
      << "The callback must keep a reference to the resource";
  if (loaded()) {
    callback->Done(true);
  } else {
    UrlReadAsyncFetchCallback* cb =
        new UrlReadAsyncFetchCallback(callback, this);
    cb->Fetch(resource_manager_->url_async_fetcher(), message_handler);
  }
}

}  // namespace net_instaweb
