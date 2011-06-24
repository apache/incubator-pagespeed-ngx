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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_WRITE_THROUGH_CACHE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_WRITE_THROUGH_CACHE_H_

#include <cstddef>
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class SharedString;

// Composes two caches to form a write-through cache.
class WriteThroughCache : public CacheInterface {
 public:
  static const size_t kUnlimited;

  // Takes ownership of both caches passed in.
  WriteThroughCache(CacheInterface* cache1, CacheInterface* cache2)
      : cache1_(cache1),
        cache2_(cache2),
        cache1_size_limit_(kUnlimited) {
  }
  virtual ~WriteThroughCache();

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, SharedString* value);
  virtual void Delete(const GoogleString& key);

  // By default, all data goes into both cache1 and cache2.  But
  // if you only want to put small items in cache1, you can set the
  // size limit.  Note that both the key and value will count
  // torward the size.
  void set_cache1_limit(size_t limit) { cache1_size_limit_ = limit; }

  CacheInterface* cache1() { return cache1_.get(); }
  CacheInterface* cache2() { return cache2_.get(); }

 private:
  void PutInCache1(const GoogleString& key, SharedString* value);
  friend class WriteThroughCallback;

  scoped_ptr<CacheInterface> cache1_;
  scoped_ptr<CacheInterface> cache2_;
  size_t cache1_size_limit_;

  DISALLOW_COPY_AND_ASSIGN(WriteThroughCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_WRITE_THROUGH_CACHE_H_
