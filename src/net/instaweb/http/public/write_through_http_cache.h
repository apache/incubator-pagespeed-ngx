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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_WRITE_THROUGH_HTTP_CACHE_H_
#define NET_INSTAWEB_HTTP_PUBLIC_WRITE_THROUGH_HTTP_CACHE_H_

#include <cstddef>
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class CacheInterface;
class Hasher;
class HTTPValue;
class MessageHandler;
class Statistics;
class Timer;

// Composes two cache interfaces to form a two level http cache.
class WriteThroughHTTPCache : public HTTPCache {
 public:
  static const size_t kUnlimited;

  // Takes ownership of both caches passed in.
  WriteThroughHTTPCache(CacheInterface* cache1, CacheInterface* cache2,
                        Timer* timer, Hasher* hasher, Statistics* statistics);

  virtual ~WriteThroughHTTPCache();

  // Implements HTTPCache::SetIgnoreFailurePuts().
  virtual void SetIgnoreFailurePuts();

  // Implements HTTPCache::Find().
  virtual void Find(const GoogleString& key, MessageHandler* handler,
                    Callback* callback);

  // Implements HTTPCache::Delete().
  virtual void Delete(const GoogleString& key);

  // Implements HTTPCache::set_force_caching().
  virtual void set_force_caching(bool force);

  // Implements HTTPCache::set_remember_not_cacheable_ttl_seconds().
  virtual void set_remember_not_cacheable_ttl_seconds(int64 value);

  // Implements HTTPCache::set_remember_fetch_failed_ttl_seconds().
  virtual void set_remember_fetch_failed_ttl_seconds(int64 value);

  // Implements HTTPCache::set_remember_fetch_dropped_ttl_seconds();
  virtual void set_remember_fetch_dropped_ttl_seconds(int64 value);

  // Implements HTTPCache::set_max_cacheable_response_content_length().
  virtual void set_max_cacheable_response_content_length(int64 value);

  // Implements HTTPCache::RememberNotCacheable().
  virtual void RememberNotCacheable(const GoogleString& key,
                                    MessageHandler * handler);

  // Implements HTTPCache::RememberFetchFailed().
  virtual void RememberFetchFailed(const GoogleString& key,
                                   MessageHandler * handler);

  // Implements HTTPCache::RememberFetchDropped().
  virtual void RememberFetchDropped(const GoogleString& key,
                                    MessageHandler * handler);

  // By default, all data goes into both cache1 and cache2.  But
  // if you only want to put small items in cache1, you can set the
  // size limit.  Note that both the key and value will count
  // torward the size.
  void set_cache1_limit(size_t limit) { cache1_size_limit_ = limit; }

  virtual const char* Name() const { return name_.c_str(); }

 protected:
  // Implements HTTPCache::PutInternal().
  virtual void PutInternal(const GoogleString& key, int64 start_us,
                           HTTPValue* value);

 private:
  void PutInCache1(const GoogleString& key, HTTPValue* value);

  scoped_ptr<HTTPCache> cache1_;
  scoped_ptr<HTTPCache> cache2_;
  size_t cache1_size_limit_;
  GoogleString name_;

  DISALLOW_COPY_AND_ASSIGN(WriteThroughHTTPCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_WRITE_THROUGH_HTTP_CACHE_H_
