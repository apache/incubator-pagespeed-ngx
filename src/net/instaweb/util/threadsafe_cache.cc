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

#include "net/instaweb/util/public/threadsafe_cache.h"
#include "net/instaweb/util/public/abstract_mutex.h"

namespace net_instaweb {

ThreadsafeCache::~ThreadsafeCache() {
}

bool ThreadsafeCache::Get(const std::string& key, SharedString* value) {
  ScopedMutex mutex(mutex_);
  return cache_->Get(key, value);
}

void ThreadsafeCache::Put(const std::string& key, SharedString* value) {
  ScopedMutex mutex(mutex_);
  cache_->Put(key, value);
}

void ThreadsafeCache::Delete(const std::string& key) {
  ScopedMutex mutex(mutex_);
  cache_->Delete(key);
}

CacheInterface::KeyState ThreadsafeCache::Query(const std::string& key) {
  ScopedMutex mutex(mutex_);
  return cache_->Query(key);
}

}  // namespace net_instaweb
