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

namespace net_instaweb {

WriteThroughCache::~WriteThroughCache() {
}

bool WriteThroughCache::Get(const std::string& key, SharedString* value) {
  return (cache1_->Get(key, value) || cache2_->Get(key, value));
}

void WriteThroughCache::Put(const std::string& key, SharedString* value) {
  cache1_->Put(key, value);
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
