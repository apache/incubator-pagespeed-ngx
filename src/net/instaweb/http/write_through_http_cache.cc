/*
 * Copyright 2011 Google Inc.
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

// Author: nikhilmadan@google.com (Nikhil Madan)

#include "net/instaweb/http/public/write_through_http_cache.h"
#include <cstddef>
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

const size_t WriteThroughHTTPCache::kUnlimited = static_cast<size_t>(-1);

// TODO(nikhilmadan): Fix the stats computation of cache expirations which are
// currently double counted.

class WriteThroughHTTPCache::WriteThroughHTTPCallback
    : public HTTPCache::Callback {
 public:
  WriteThroughHTTPCallback(const GoogleString& key,
                           WriteThroughHTTPCache* cache,
                           MessageHandler* handler,
                           Callback* callback)
      : key_(key),
        cache_(cache),
        handler_(handler),
        callback_(callback),
        trying_cache2_(false) { }

  virtual ~WriteThroughHTTPCallback() { }

  virtual void Done(HTTPCache::FindResult find_result) {
    if (!trying_cache2_ && find_result == HTTPCache::kNotFound) {
      // This is not a cache miss yet. Check the other cache.
      // TODO(nikhilmadan): Put in a hook into HTTPCache to allow the derived
      // class to take control of statistics updating.
      cache_->cache_misses()->Add(-1);
      trying_cache2_ = true;
      cache_->cache2_->Find(key_, handler_, this);
    } else {
      callback_->http_value()->Link(http_value());
      callback_->response_headers()->CopyFrom(*response_headers());
      if (trying_cache2_ && find_result != HTTPCache::kNotFound) {
        cache_->PutInCache1(key_, http_value());
      }
      callback_->Done(find_result);
      delete this;
    }
  }

  virtual bool IsCacheValid(const ResponseHeaders& headers) {
    return callback_->IsCacheValid(headers);
  }

 private:
  GoogleString key_;
  WriteThroughHTTPCache* cache_;
  MessageHandler* handler_;
  Callback* callback_;
  bool trying_cache2_;

  DISALLOW_COPY_AND_ASSIGN(WriteThroughHTTPCallback);
};

WriteThroughHTTPCache::~WriteThroughHTTPCache() {
}

void WriteThroughHTTPCache::PutInCache1(const GoogleString& key,
                                        HTTPValue* value) {
  if ((cache1_size_limit_ == kUnlimited) ||
      (key.size() + value->size() < cache1_size_limit_)) {
    cache1_->PutInternal(key, timer()->NowUs(), value);
    // Avoid double counting the put.
    cache_inserts()->Add(-1);
  }
}

void WriteThroughHTTPCache::SetReadOnly() {
  cache1_->SetReadOnly();
  cache2_->SetReadOnly();
}

void WriteThroughHTTPCache::Find(const GoogleString& key,
                                 MessageHandler* handler,
                                 Callback* callback) {
  WriteThroughHTTPCallback* write_through_callback =
      new WriteThroughHTTPCallback(key, this, handler, callback);
  cache1_->Find(key, handler, write_through_callback);
}

HTTPCache::FindResult WriteThroughHTTPCache::Find(
    const GoogleString& key,
    HTTPValue* value,
    ResponseHeaders* headers,
    MessageHandler* handler) {
  HTTPCache::FindResult result = cache1_->Find(key, value, headers, handler);
  if (result == kNotFound) {
    // This is not a cache miss yet. Check the other cache.
    cache_misses()->Add(-1);
    result = cache2_->Find(key, value, headers, handler);
  }
  return result;
}

void WriteThroughHTTPCache::PutInternal(const GoogleString& key, int64 start_us,
                                        HTTPValue* value) {
  // Put into cache2_'s underlying cache.
  cache2_->PutInternal(key, start_us, value);
  // Put into cache1_'s underlying cache if required.
  PutInCache1(key, value);
}

CacheInterface::KeyState WriteThroughHTTPCache::Query(const GoogleString& key) {
  CacheInterface::KeyState state = cache1_->Query(key);
  if (state != CacheInterface::kAvailable) {
    state = cache2_->Query(key);
  }
  return state;
}

void WriteThroughHTTPCache::Delete(const GoogleString& key) {
  cache1_->Delete(key);
  cache2_->Delete(key);
}

void WriteThroughHTTPCache::set_force_caching(bool force) {
  HTTPCache::set_force_caching(force);
  cache1_->set_force_caching(force);
  cache2_->set_force_caching(force);
}

void WriteThroughHTTPCache::set_remember_not_cacheable_ttl_seconds(
    int64 value) {
  HTTPCache::set_remember_not_cacheable_ttl_seconds(value);
  cache1_->set_remember_not_cacheable_ttl_seconds(value);
  cache2_->set_remember_not_cacheable_ttl_seconds(value);
}

void WriteThroughHTTPCache::set_remember_fetch_failed_ttl_seconds(
    int64 value) {
  HTTPCache::set_remember_fetch_failed_ttl_seconds(value);
  cache1_->set_remember_fetch_failed_ttl_seconds(value);
  cache2_->set_remember_fetch_failed_ttl_seconds(value);
}

void WriteThroughHTTPCache::RememberNotCacheable(
    const GoogleString& key,
    MessageHandler * handler) {
  cache1_->RememberNotCacheable(key, handler);
  cache2_->RememberNotCacheable(key, handler);
}

void WriteThroughHTTPCache::RememberFetchFailed(
    const GoogleString& key,
    MessageHandler * handler) {
  cache1_->RememberFetchFailed(key, handler);
  cache2_->RememberFetchFailed(key, handler);
}

}  // namespace net_instaweb
