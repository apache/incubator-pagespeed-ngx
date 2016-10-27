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

#ifndef PAGESPEED_KERNEL_CACHE_WRITE_THROUGH_CACHE_H_
#define PAGESPEED_KERNEL_CACHE_WRITE_THROUGH_CACHE_H_

#include <cstddef>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/cache/cache_interface.h"

namespace net_instaweb {

// Composes two caches to form a write-through cache.
class WriteThroughCache : public CacheInterface {
 public:
  static const size_t kUnlimited;

  // Does not take ownership of caches passed in.
  WriteThroughCache(CacheInterface* cache1, CacheInterface* cache2)
      : cache1_(cache1),
        cache2_(cache2),
        cache1_size_limit_(kUnlimited) {
  }

  virtual ~WriteThroughCache();

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, const SharedString& value);
  virtual void Delete(const GoogleString& key);

  // By default, all data goes into both cache1 and cache2.  But
  // if you only want to put small items in cache1, you can set the
  // size limit.  Note that both the key and value will count
  // torward the size.
  void set_cache1_limit(size_t limit) { cache1_size_limit_ = limit; }
  size_t cache1_limit() const { return cache1_size_limit_; }

  CacheInterface* cache1() { return cache1_; }
  CacheInterface* cache2() { return cache2_; }
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

  virtual GoogleString Name() const {
    return FormatName(cache1_->Name(), cache2_->Name());
  }
  static GoogleString FormatName(StringPiece l1, StringPiece l2);

 private:
  void PutInCache1(const GoogleString& key, const SharedString& value);
  friend class WriteThroughCallback;

  CacheInterface* cache1_;
  CacheInterface* cache2_;
  size_t cache1_size_limit_;

  DISALLOW_COPY_AND_ASSIGN(WriteThroughCache);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_CACHE_WRITE_THROUGH_CACHE_H_
