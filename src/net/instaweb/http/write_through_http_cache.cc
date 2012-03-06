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
#include "net/instaweb/http/timing.pb.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class CacheInterface;

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
      // Clear the fallback_http_value() in client_callback_ since we found a
      // fresh response.
      client_callback_->fallback_http_value()->Clear();
      // Insert the response into cache1.
      (write_through_http_cache_->*function_)(key_, http_value());
    } else if (!fallback_http_value()->Empty()) {
      // We assume that the fallback value in the L2 cache is always fresher
      // than or as fresh as the fallback value in the L1 cache.
      HTTPValue* client_fallback = client_callback_->fallback_http_value();
      client_fallback->Clear();
      client_fallback->Link(fallback_http_value());
    }
    client_callback_->Done(find_result);
    delete this;
  }

  virtual bool IsCacheValid(const ResponseHeaders& headers) {
    return client_callback_->IsCacheValid(headers);
  }

  virtual bool IsFresh(const ResponseHeaders& headers) {
    return client_callback_->IsFresh(headers);
  }

  virtual void SetTimingMs(int64 timing_value_ms) {
    client_callback_->timing_info()->set_cache2_ms(timing_value_ms);
  }

  virtual TimingInfo* timing_info() {
    return client_callback_->timing_info();
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
      if (!fallback_http_value()->Empty()) {
        // If we have a stale value in the L1 cache, use it unless we find a
        // fresher value in the L2 cache.
        client_callback_->fallback_http_value()->Link(fallback_http_value());
      }
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

  virtual bool IsFresh(const ResponseHeaders& headers) {
    return client_callback_->IsFresh(headers);
  }

  virtual void SetTimingMs(int64 timing_value_ms) {
    client_callback_->timing_info()->set_cache1_ms(timing_value_ms);
  }

  virtual TimingInfo* timing_info() {
    return client_callback_->timing_info();
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

void WriteThroughHTTPCache::PutInternal(const GoogleString& key, int64 start_us,
                                        HTTPValue* value) {
  // Put into cache2_'s underlying cache.
  cache2_->PutInternal(key, start_us, value);
  // Put into cache1_'s underlying cache if required.
  PutInCache1(key, value);
}

void WriteThroughHTTPCache::Delete(const GoogleString& key) {
  cache1_->Delete(key);
  cache2_->Delete(key);
  cache_deletes()->Add(-1);  // To avoid double counting.
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
    MessageHandler* handler) {
  cache1_->RememberNotCacheable(key, handler);
  cache2_->RememberNotCacheable(key, handler);
}

void WriteThroughHTTPCache::RememberFetchFailed(
    const GoogleString& key,
    MessageHandler* handler) {
  cache1_->RememberFetchFailed(key, handler);
  cache2_->RememberFetchFailed(key, handler);
}

}  // namespace net_instaweb
