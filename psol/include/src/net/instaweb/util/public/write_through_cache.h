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
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class SharedString;

// Composes two caches to form a write-through cache.
class WriteThroughCache : public CacheInterface {
 public:
  static const size_t kUnlimited;

  // Does not take ownership of caches passed in.
  WriteThroughCache(CacheInterface* cache1, CacheInterface* cache2)
      : cache1_(cache1),
        cache2_(cache2),
        cache1_size_limit_(kUnlimited),
        name_(StrCat("WriteThroughCache using backend 1 : ", cache1->Name(),
                     " and backend 2 : ", cache2->Name())) {}

  virtual ~WriteThroughCache();

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, SharedString* value);
  virtual void Delete(const GoogleString& key);

  // By default, all data goes into both cache1 and cache2.  But
  // if you only want to put small items in cache1, you can set the
  // size limit.  Note that both the key and value will count
  // torward the size.
  void set_cache1_limit(size_t limit) { cache1_size_limit_ = limit; }

  CacheInterface* cache1() { return cache1_; }
  CacheInterface* cache2() { return cache2_; }
  virtual const char* Name() const { return name_.c_str(); }
  virtual bool IsBlocking() const {
    // We can fulfill our guarantee only if both caches block.
    return cache1_->IsBlocking() && cache2_->IsBlocking();
  }

  virtual bool IsHealthy() const {
    return cache1_->IsHealthy() && cache2_->IsHealthy();
  }

  virtual void ShutDown() {
    cache1_->ShutDown();
    cache2_->ShutDown();
  }

 private:
  void PutInCache1(const GoogleString& key, SharedString* value);
  friend class WriteThroughCallback;

  CacheInterface* cache1_;
  CacheInterface* cache2_;
  size_t cache1_size_limit_;
  GoogleString name_;

  DISALLOW_COPY_AND_ASSIGN(WriteThroughCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_WRITE_THROUGH_CACHE_H_
