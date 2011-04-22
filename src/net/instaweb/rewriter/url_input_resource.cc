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

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/http/public/url_async_fetcher.h"

namespace net_instaweb {

UrlInputResource::~UrlInputResource() {
}

// Shared fetch callback, used by both Load and LoadAndCallback
class UrlResourceFetchCallback : public UrlAsyncFetcher::Callback {
 public:
  UrlResourceFetchCallback(ResourceManager* resource_manager,
                           const RewriteOptions* rewrite_options) :
      resource_manager_(resource_manager),
      rewrite_options_(rewrite_options),
      message_handler_(NULL) { }
  virtual ~UrlResourceFetchCallback() {}

  bool Fetch(UrlAsyncFetcher* fetcher, MessageHandler* handler) {
    // TODO(jmarantz): consider request_headers.  E.g. will we ever
    // get different resources depending on user-agent?
    RequestHeaders request_headers;
    message_handler_ = handler;
    GoogleString lock_name =
        StrCat(resource_manager_->hasher()->Hash(url()), ".lock");
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

    GoogleString origin_url;
    bool ret = false;
    const DomainLawyer* lawyer = rewrite_options_->domain_lawyer();
    if (lawyer->MapOrigin(url(), &origin_url)) {
      if (origin_url != url()) {
        // If mapping the URL changes its host, then add a 'Host' header
        // pointing to the origin URL's hostname.
        GoogleUrl gurl(url());
        if (gurl.is_valid()) {
          request_headers.Add(HttpAttributes::kHost, gurl.Host());
        }
      }
      // TODO(sligocki): Allow ConditionalFetch here
      ret = fetcher->StreamingFetch(
          origin_url, request_headers, response_headers(), http_value(),
          handler, this);
    } else {
      delete this;
    }
    return ret;
  }

  void AddToCache(bool success) {
    ResponseHeaders* headers = response_headers();
    if (success && !headers->IsErrorStatus()
        && !http_cache()->IsAlreadyExpired(*headers)) {
      HTTPValue* value = http_value();
      value->SetHeaders(headers);
      http_cache()->Put(url(), value, message_handler_);
    } else {
      http_cache()->RememberNotCacheable(url(), message_handler_);
    }
  }

  virtual void Done(bool success) {
    AddToCache(success);
    if (lock_.get() != NULL) {
      message_handler_->Message(
          kInfo, "%s: Unlocking lock %s with success=%s",
          url().c_str(), lock_->name().c_str(), success ? "true" : "false");
      lock_->Unlock();
      lock_.reset(NULL);
    }
    DoneInternal(success);
    delete this;
  }

  // The two derived classes differ in how they provide the
  // fields below.  The Async callback gets them from the resource,
  // which must be live at the time it is called.  The ReadIfCached
  // cannot rely on the resource still being alive when the callback
  // is called, so it must keep them locally in the class.
  virtual ResponseHeaders* response_headers() = 0;
  virtual HTTPValue* http_value() = 0;
  virtual GoogleString url() const = 0;
  virtual HTTPCache* http_cache() = 0;
  // If someone is already fetching this resource, should we yield to them and
  // try again later?  If so, return true.  Otherwise, if we must fetch the
  // resource regardless, return false.
  // TODO(abliss): unit test this
  virtual bool should_yield() = 0;

 protected:
  virtual void DoneInternal(bool success) {
  }

  ResourceManager* resource_manager_;
  const RewriteOptions* rewrite_options_;
  MessageHandler* message_handler_;

 private:
  scoped_ptr<AbstractLock> lock_;
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
      : UrlResourceFetchCallback(resource_manager, rewrite_options),
        url_(url),
        http_cache_(http_cache) {
  }

  // Indicate that it's OK for the callback to be executed on a different
  // thread, as it only populates the cache, which is thread-safe.
  virtual bool EnableThreaded() const { return true; }

  virtual ResponseHeaders* response_headers() { return &response_headers_; }
  virtual HTTPValue* http_value() { return &http_value_; }
  virtual GoogleString url() const { return url_; }
  virtual HTTPCache* http_cache() { return http_cache_; }
  virtual bool should_yield() { return true; }

 private:
  GoogleString url_;
  HTTPCache* http_cache_;
  HTTPValue http_value_;
  ResponseHeaders response_headers_;

  DISALLOW_COPY_AND_ASSIGN(UrlReadIfCachedCallback);
};

bool UrlInputResource::Load(MessageHandler* handler) {
  meta_data_.Clear();
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
       (http_cache->Find(url_, &value_, &meta_data_, handler) ==
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
                                 resource->rewrite_options()),
        resource_(resource),
        callback_(callback) {
  }

  virtual void DoneInternal(bool success) {
    callback_->Done(success);
  }

  virtual ResponseHeaders* response_headers() { return &resource_->meta_data_; }
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
