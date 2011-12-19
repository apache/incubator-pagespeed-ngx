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
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

// Callback to look up cache2. Note that if the response is found in cache2, we
// insert it into cache1.
class FallbackCacheCallback: public HTTPCache::Callback {
 public:
  typedef void (WriteThroughHTTPCache::*UpdateCache1HandlerFunction) (
      const GoogleString& key, HTTPValue* http_value);

  FallbackCacheCallback(const GoogleString& key,
                        WriteThroughHTTPCache* write_through_http_cache,
                        HTTPCache::Callback* client_callback,
                        UpdateCache1HandlerFunction function)
      : key_(key),
        write_through_http_cache_(write_through_http_cache),
        client_callback_(client_callback),
        function_(function) {}

  virtual ~FallbackCacheCallback() {}

  virtual void Done(HTTPCache::FindResult find_result) {
    if (find_result != HTTPCache::kNotFound) {
      client_callback_->http_value()->Link(http_value());
      client_callback_->response_headers()->CopyFrom(*response_headers());
      // Insert the response into cache1.
      (write_through_http_cache_->*function_)(key_, http_value());
    }
    client_callback_->Done(find_result);
    delete this;
  }

  virtual bool IsCacheValid(const ResponseHeaders& headers) {
    return client_callback_->IsCacheValid(headers);
  }

 private:
  GoogleString key_;
  WriteThroughHTTPCache* write_through_http_cache_;
  HTTPCache::Callback* client_callback_;
  UpdateCache1HandlerFunction function_;
};

// Callback to look up cache1. Note that if the response is not found in cache1,
// we look up fallback_cache.
class Cache1Callback: public HTTPCache::Callback {
 public:
  Cache1Callback(const GoogleString& key,
                 HTTPCache* fallback_cache,
                 MessageHandler* handler,
                 HTTPCache::Callback* client_callback,
                 HTTPCache::Callback* fallback_cache_callback)
      : key_(key),
        fallback_cache_(fallback_cache),
        handler_(handler),
        client_callback_(client_callback),
        fallback_cache_callback_(fallback_cache_callback) {}

  virtual ~Cache1Callback() {}

  virtual void Done(HTTPCache::FindResult find_result) {
    if (find_result == HTTPCache::kNotFound) {
      fallback_cache_->cache_misses()->Add(-1);
      fallback_cache_->Find(key_, handler_, fallback_cache_callback_.release());
    } else {
      client_callback_->http_value()->Link(http_value());
      client_callback_->response_headers()->CopyFrom(*response_headers());
      client_callback_->Done(find_result);
    }
    delete this;
  }

  virtual bool IsCacheValid(const ResponseHeaders& headers) {
    return client_callback_->IsCacheValid(headers);
  }

 private:
  GoogleString key_;
  HTTPCache* fallback_cache_;
  MessageHandler* handler_;
  HTTPCache::Callback* client_callback_;
  scoped_ptr<HTTPCache::Callback> fallback_cache_callback_;
};

}  // namespace

const size_t WriteThroughHTTPCache::kUnlimited = static_cast<size_t>(-1);

// TODO(nikhilmadan): Fix the stats computation of cache expirations which are
// currently double counted.

WriteThroughHTTPCache::WriteThroughHTTPCache(CacheInterface* cache1,
                                             CacheInterface* cache2,
                                             Timer* timer,
                                             Hasher* hasher,
                                             Statistics* statistics)
    : HTTPCache(NULL, timer, hasher, statistics),
      cache1_(new HTTPCache(cache1, timer, hasher, statistics)),
      cache2_(new HTTPCache(cache2, timer, hasher, statistics)),
      cache1_size_limit_(kUnlimited) {}

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

void WriteThroughHTTPCache::SetIgnoreFailurePuts() {
  cache1_->SetIgnoreFailurePuts();
  cache2_->SetIgnoreFailurePuts();
}

void WriteThroughHTTPCache::Find(const GoogleString& key,
                                 MessageHandler* handler,
                                 Callback* callback) {
  FallbackCacheCallback* fallback_cache_callback = new FallbackCacheCallback(
      key, this, callback, &WriteThroughHTTPCache::PutInCache1);
  Cache1Callback* cache1_callback = new Cache1Callback(
      key, cache2_.get(), handler, callback, fallback_cache_callback);
  cache1_->Find(key, handler, cache1_callback);
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
