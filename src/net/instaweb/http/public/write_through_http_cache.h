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
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class Hasher;
class HTTPValue;
class MessageHandler;
class ResponseHeaders;
class Statistics;
class Timer;

// Composes two cache interfaces to form a two level http cache.
class WriteThroughHTTPCache : public HTTPCache {
 public:
  static const size_t kUnlimited;

  // Takes ownership of both caches passed in.
  WriteThroughHTTPCache(CacheInterface* cache1, CacheInterface* cache2,
                        Timer* timer, Hasher* hasher, Statistics* statistics)
      : HTTPCache(NULL, timer, hasher, statistics),
        cache1_(new HTTPCache(cache1, timer, hasher, statistics)),
        cache2_(new HTTPCache(cache2, timer, hasher, statistics)),
        cache1_size_limit_(kUnlimited) { }
  virtual ~WriteThroughHTTPCache();

  void SetReadOnly();

  virtual void Find(const GoogleString& key, MessageHandler* handler,
                    Callback* callback);

  virtual HTTPCache::FindResult Find(const GoogleString& key, HTTPValue* value,
                                     ResponseHeaders* headers,
                                     MessageHandler* handler);

  virtual CacheInterface::KeyState Query(const GoogleString& key);

  virtual void Delete(const GoogleString& key);

  // By default, all data goes into both cache1 and cache2.  But
  // if you only want to put small items in cache1, you can set the
  // size limit.  Note that both the key and value will count
  // torward the size.
  void set_cache1_limit(size_t limit) { cache1_size_limit_ = limit; }

  virtual void set_force_caching(bool force);

  virtual void set_remember_not_cacheable_ttl_seconds(int64 value);

  virtual void set_remember_fetch_failed_ttl_seconds(int64 value);

  virtual void RememberNotCacheable(const GoogleString& key,
                                    MessageHandler * handler);

  virtual void RememberFetchFailed(const GoogleString& key,
                                   MessageHandler * handler);

 protected:
  virtual void PutInternal(const GoogleString& key, int64 start_us,
                           HTTPValue* value);

 private:
  class WriteThroughHTTPCallback;
  friend class WriteThroughHTTPCallback;

  void PutInCache1(const GoogleString& key, HTTPValue* value);

  scoped_ptr<HTTPCache> cache1_;
  scoped_ptr<HTTPCache> cache2_;
  size_t cache1_size_limit_;

  DISALLOW_COPY_AND_ASSIGN(WriteThroughHTTPCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_WRITE_THROUGH_HTTP_CACHE_H_
