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
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/http_value_writer.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

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
    cacheable = headers.VaryCacheable();
  } else {
    cacheable = headers.IsCacheable();
  }
  // If we are setting a TTL for HTML, we cannot rewrite any resource
  // with a shorter TTL.
  cacheable &= (headers.cache_ttl_ms() >= min_cache_time_to_rewrite_ms);

  if (!cacheable && !http_cache->force_caching()) {
    return false;
  }

  return !http_cache->IsAlreadyExpired(headers);
}

// Returns true if the input didn't change and we could successfully update
// input_info() in the callback.
bool CheckAndUpdateInputInfo(const ResponseHeaders& headers,
                             const HTTPValue& value,
                             const RewriteOptions& options,
                             const ResourceManager& manager,
                             Resource::FreshenCallback* callback) {
  InputInfo* input_info = callback->input_info();
  if (input_info != NULL && input_info->has_input_content_hash() &&
      IsValidAndCacheableImpl(manager.http_cache(),
                              options.min_resource_cache_time_to_rewrite_ms(),
                              options.respect_vary(),
                              headers)) {
    StringPiece content;
    if (value.ExtractContents(&content)) {
      InputInfo* input_info = callback->input_info();
      GoogleString new_hash = manager.contents_hasher()->Hash(content);
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
    : Resource((rewrite_driver == NULL ? NULL :
                rewrite_driver->resource_manager()), type),
      url_(url.data(), url.size()),
      rewrite_driver_(rewrite_driver),
      rewrite_options_(options),
      respect_vary_(rewrite_options_->respect_vary()) {
  response_headers()->set_implicit_cache_ttl_ms(
      options->implicit_cache_ttl_ms());
}

UrlInputResource::~UrlInputResource() {
}

// Shared fetch callback, used by both Load and LoadAndCallback
class UrlResourceFetchCallback : public AsyncFetch {
 public:
  UrlResourceFetchCallback(ResourceManager* resource_manager,
                           const RewriteOptions* rewrite_options,
                           HTTPValue* fallback_value)
      : resource_manager_(resource_manager),
        rewrite_options_(rewrite_options),
        message_handler_(NULL),
        success_(false),
        no_cache_ok_(false),
        fetcher_(NULL),
        respect_vary_(rewrite_options->respect_vary()),
        resource_cutoff_ms_(
            rewrite_options->min_resource_cache_time_to_rewrite_ms()),
        fallback_fetch_(NULL) {
    if (fallback_value != NULL) {
      fallback_value_.Link(fallback_value);
    }
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
                                  respect_vary_, *headers)) {
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
    AsyncFetch* fetch = this;
    if (rewrite_options_->serve_stale_if_fetch_error() &&
        !fallback_value_.Empty()) {
      // Use a stale value if the fetch from the backend fails.
      fallback_fetch_ = new FallbackSharedAsyncFetch(
          this, &fallback_value_, message_handler_);
      fallback_fetch_->set_fallback_responses_served(
          resource_manager_->rewrite_stats()->fallback_responses_served());
      fetch = fallback_fetch_;
    }
    if (!fallback_value_.Empty()) {
      // Use the conditional headers in a stale response in cache while
      // triggering the outgoing fetch.
      ConditionalSharedAsyncFetch* conditional_fetch =
          new ConditionalSharedAsyncFetch(
              fetch, &fallback_value_, message_handler_);
      conditional_fetch->set_num_conditional_refreshes(
          resource_manager_->rewrite_stats()->num_conditional_refreshes());
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
      // LoadAndCallback) we turn it into a fetch failure so we do not
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
    http_value_writer()->CheckCanCacheElseClear(response_headers());
  }
  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    return http_value_writer()->Write(content, handler);
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
  virtual HTTPValueWriter* http_value_writer() = 0;
  // If someone is already fetching this resource, should we yield to them and
  // try again later?  If so, return true.  Otherwise, if we must fetch the
  // resource regardless, return false.
  // TODO(abliss): unit test this
  virtual bool should_yield() = 0;

  // Indicate that it's OK for the callback to be executed on a different
  // thread, as it only populates the cache, which is thread-safe.
  virtual bool EnableThreaded() const { return true; }

  void set_no_cache_ok(bool x) { no_cache_ok_ = x; }

 protected:
  virtual void DoneInternal(bool success) {
  }

  ResourceManager* resource_manager_;
  const RewriteOptions* rewrite_options_;
  MessageHandler* message_handler_;

 private:
  // TODO(jmarantz): consider request_headers.  E.g. will we ever
  // get different resources depending on user-agent?
  HTTPValue fallback_value_;
  bool success_;

  // If this is true, loading of non-cacheable resources will succeed.
  bool no_cache_ok_;
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
// discovered while rewriting HTML. Note that this uses the Last-Modified and
// If-None-Match headers of the stale value in cache to conditionally refresh
// the resource.
class FreshenFetchCallback : public UrlResourceFetchCallback {
 public:
  FreshenFetchCallback(const GoogleString& url, HTTPCache* http_cache,
                       ResourceManager* resource_manager,
                       RewriteDriver* rewrite_driver,
                       const RewriteOptions* rewrite_options,
                       HTTPValue* fallback_value,
                       Resource::FreshenCallback* callback)
      : UrlResourceFetchCallback(resource_manager, rewrite_options,
                                 fallback_value),
        url_(url),
        http_cache_(http_cache),
        rewrite_driver_(rewrite_driver),
        callback_(callback),
        http_value_writer_(&http_value_, http_cache_) {
    response_headers()->set_implicit_cache_ttl_ms(
        rewrite_options->implicit_cache_ttl_ms());
  }

  virtual void DoneInternal(bool success) {
    if (callback_ != NULL) {
      success &= CheckAndUpdateInputInfo(
          *response_headers(), http_value_, *rewrite_options_,
          *rewrite_driver_->resource_manager(), callback_);
      callback_->Done(success);
    }
    rewrite_driver_->decrement_async_events_count();
  }

  virtual HTTPValue* http_value() { return &http_value_; }
  virtual GoogleString url() const { return url_; }
  virtual HTTPCache* http_cache() { return http_cache_; }
  virtual HTTPValueWriter* http_value_writer() { return &http_value_writer_; }
  virtual bool should_yield() { return true; }
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
                           ResourceManager* manager,
                           RewriteDriver* driver,
                           const RewriteOptions* options,
                           Resource::FreshenCallback* callback)
      : OptionsAwareHTTPCacheCallback(options),
        url_(url),
        manager_(manager),
        driver_(driver),
        options_(options),
        callback_(callback) {}

  virtual ~FreshenHttpCacheCallback() {}

  virtual void Done(HTTPCache::FindResult find_result) {
    if (find_result == HTTPCache::kNotFound) {
      // Not found in cache. Invoke the fetcher.
      FreshenFetchCallback* cb = new FreshenFetchCallback(
          url_, manager_->http_cache(), manager_, driver_, options_,
          fallback_http_value(), callback_);
      cb->Fetch(driver_->async_fetcher(), manager_->message_handler());
    } else {
      if (callback_ != NULL) {
        bool success = (find_result == HTTPCache::kFound) &&
            CheckAndUpdateInputInfo(*response_headers(), *http_value(),
                                    *options_, *manager_, callback_);
        callback_->Done(success);
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
    return !manager_->IsImminentlyExpiring(date_ms, expiry_ms);
  }

 private:
  GoogleString url_;
  ResourceManager* manager_;
  RewriteDriver* driver_;
  const RewriteOptions* options_;
  Resource::FreshenCallback* callback_;
  DISALLOW_COPY_AND_ASSIGN(FreshenHttpCacheCallback);
};

bool UrlInputResource::IsValidAndCacheable() const {
  return IsValidAndCacheableImpl(
      resource_manager()->http_cache(),
      rewrite_options_->min_resource_cache_time_to_rewrite_ms(),
      respect_vary_, response_headers_);
}

bool UrlInputResource::Load(MessageHandler* handler) {
  // I believe a deep static analysis would reveal this function
  // cannot be reached.  However, the typing rules of C++ do not allow
  // us to omit the definition.
  //
  // Resource::Load is a protected abstract method.  It's called in
  // the default implementation of Resource::LoadAndCallback.
  // UrlInputResource::LoadAndCallback overrides that, however, and
  // does not call Load(), so this function cannot be reached.
  LOG(DFATAL) << "Blocking Load should never be called for UrlInputResource";
  return false;
}

void UrlInputResource::Freshen(Resource::FreshenCallback* callback,
                               MessageHandler* handler) {
  // TODO(jmarantz): use if-modified-since
  // For now this is much like Load(), except we do not
  // touch our value, but just the cache
  HTTPCache* http_cache = resource_manager()->http_cache();
  if (rewrite_driver_ != NULL) {
    // Ensure that the rewrite driver is alive until the freshen is completed.
    rewrite_driver_->increment_async_events_count();
  } else {
    LOG(DFATAL) << "rewrite_driver_ must be non-NULL while freshening";
    return;
  }

  FreshenHttpCacheCallback* freshen_callback = new FreshenHttpCacheCallback(
      url_, resource_manager(), rewrite_driver_, rewrite_options_, callback);
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
                                     UrlInputResource* resource)
      : UrlResourceFetchCallback(resource->resource_manager(),
                                 resource->rewrite_options(),
                                 &resource->fallback_value_),
        resource_(resource),
        callback_(callback),
        http_value_writer_(http_value(), http_cache()) {
    set_response_headers(&resource_->response_headers_);
    response_headers()->set_implicit_cache_ttl_ms(
        resource->rewrite_options()->implicit_cache_ttl_ms());
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
  virtual bool IsBackgroundFetch() const {
    return resource_->is_background_fetch();
  }

  virtual HTTPValue* http_value() { return &resource_->value_; }
  virtual GoogleString url() const { return resource_->url(); }
  virtual HTTPCache* http_cache() {
    return resource_->resource_manager()->http_cache();
  }
  virtual HTTPValueWriter* http_value_writer() { return &http_value_writer_; }
  virtual bool should_yield() { return false; }

 private:
  UrlInputResource* resource_;
  Resource::AsyncCallback* callback_;
  HTTPValueWriter http_value_writer_;

  DISALLOW_COPY_AND_ASSIGN(UrlReadAsyncFetchCallback);
};

void UrlInputResource::LoadAndCallback(NotCacheablePolicy no_cache_policy,
                                       AsyncCallback* callback,
                                       MessageHandler* message_handler) {
  CHECK(callback != NULL) << "A callback must be supplied, or else it will "
      "not be possible to determine when it's safe to delete the resource.";
  CHECK(this == callback->resource().get())
      << "The callback must keep a reference to the resource";
  CHECK(rewrite_driver_ != NULL)
      << "Must provide a RewriteDriver for resources that will get fetched.";
  if (loaded()) {
    callback->Done(true);
  } else {
    UrlReadAsyncFetchCallback* cb =
        new UrlReadAsyncFetchCallback(callback, this);
    if (no_cache_policy == Resource::kLoadEvenIfNotCacheable) {
      cb->set_no_cache_ok(true);
    }
    cb->Fetch(rewrite_driver_->async_fetcher(), message_handler);
  }
}

}  // namespace net_instaweb
