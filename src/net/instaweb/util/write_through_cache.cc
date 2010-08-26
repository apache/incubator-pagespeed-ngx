/**
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
#include "net/instaweb/util/public/shared_string.h"

namespace net_instaweb {

const size_t WriteThroughCache::kUnlimited = static_cast<size_t>(-1);

WriteThroughCache::~WriteThroughCache() {
}

void WriteThroughCache::PutInCache1(const std::string& key,
                                    SharedString* value) {
  if ((cache1_size_limit_ == kUnlimited) ||
      (key.size() + value->size() < cache1_size_limit_)) {
    cache1_->Put(key, value);
  }
}

bool WriteThroughCache::Get(const std::string& key, SharedString* value) {
  bool ret = cache1_->Get(key, value);
  if (!ret) {
    ret = cache2_->Get(key, value);
    if (ret) {
      PutInCache1(key, value);
    }
  }
  return ret;
}

void WriteThroughCache::Put(const std::string& key, SharedString* value) {
  PutInCache1(key, value);
  cache2_->Put(key, value);
}

void WriteThroughCache::Delete(const std::string& key) {
  cache1_->Delete(key);
  cache2_->Delete(key);
}

CacheInterface::KeyState WriteThroughCache::Query(const std::string& key) {
  // There are 3 possible states, kAvailable, kInTransit, and kNotFound.
  // We want to 'promote' the state obtained from cache1.  But we don't want
  // to demote.  Specifically, if cache1 says kInTransit, and cache2 says
  // kNotFound, we'd want to return kInTransit.
  CacheInterface::KeyState state = cache1_->Query(key);
  if (state != kAvailable) {
    CacheInterface::KeyState state2 = cache2_->Query(key);
    if (state2 != kNotFound) {
      state = state2;
    }
  }
  return state;
}

}  // namespace net_instaweb
