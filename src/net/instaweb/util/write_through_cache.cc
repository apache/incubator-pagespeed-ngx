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

#include "net/instaweb/util/public/write_through_cache.h"
#include <cstddef>
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

const size_t WriteThroughCache::kUnlimited = static_cast<size_t>(-1);

WriteThroughCache::~WriteThroughCache() {
}

void WriteThroughCache::PutInCache1(const GoogleString& key,
                                    SharedString* value) {
  if ((cache1_size_limit_ == kUnlimited) ||
      (key.size() + value->get()->size() < cache1_size_limit_)) {
    cache1_->Put(key, value);
  }
}

class WriteThroughCallback : public CacheInterface::Callback {
 public:
  WriteThroughCallback(WriteThroughCache* wtc,
                       const GoogleString& key,
                       CacheInterface::Callback* callback)
      : write_through_cache_(wtc),
        key_(key),
        callback_(callback),
        trying_cache2_(false) {
  }

  virtual void Done(CacheInterface::KeyState state) {
    if (state == CacheInterface::kAvailable) {
      if (trying_cache2_) {
        write_through_cache_->PutInCache1(key_, value());
      }
      *callback_->value() = *value();
      callback_->Done(state);
      delete this;
    } else if (trying_cache2_) {
      callback_->Done(state);
      delete this;
    } else {
      trying_cache2_ = true;
      write_through_cache_->cache2()->Get(key_, this);
    }
  }


  WriteThroughCache* write_through_cache_;
  GoogleString key_;
  CacheInterface::Callback* callback_;
  bool trying_cache2_;
};

void WriteThroughCache::Get(const GoogleString& key, Callback* callback) {
  cache1_->Get(key, new WriteThroughCallback(this, key, callback));
}

void WriteThroughCache::Put(const GoogleString& key, SharedString* value) {
  PutInCache1(key, value);
  cache2_->Put(key, value);
}

void WriteThroughCache::Delete(const GoogleString& key) {
  cache1_->Delete(key);
  cache2_->Delete(key);
}

}  // namespace net_instaweb
