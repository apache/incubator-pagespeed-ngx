/*
 * Copyright 2012 Google Inc.
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
//
// Contains DelayCache, which lets one inject progammer-controlled
// delays before lookup callback invocation.
//
// See also: MockTimeCache.

#include "net/instaweb/util/public/delay_cache.h"

#include <utility>  // for pair.
#include "base/logging.h"
#include "net/instaweb/util/public/shared_string.h"

namespace net_instaweb {

// This calls a passed-in callback with a mock time delay, forwarding
// on lookup results.
class DelayCache::DelayCallback : public CacheInterface::Callback {
 public:
  DelayCallback(const GoogleString& key, DelayCache* delay_cache,
                Callback* orig_callback)
      : delay_cache_(delay_cache),
        orig_callback_(orig_callback),
        key_(key),
        state_(kNotFound) {
  }

  virtual ~DelayCallback() {}

  // Implements CacheInterface::Callback so underlying cache implementation
  // can notify the DelayCache that an time is available.
  virtual void Done(KeyState state) {
    *orig_callback_->value() = *value();
    state_ = state;
    delay_cache_->LookupComplete(this);
  };

  // Helper method so that DelayCache can call the callback for keys
  // that are not being delayed, or for keys that have been released.
  void Run() {
    orig_callback_->Done(state_);
    delete this;
  }

  const GoogleString& key() const { return key_; }

 private:
  DelayCache* delay_cache_;
  Callback* orig_callback_;
  GoogleString key_;
  KeyState state_;

  DISALLOW_COPY_AND_ASSIGN(DelayCallback);
};

DelayCache::DelayCache(CacheInterface* cache) : cache_(cache) {}

DelayCache::~DelayCache() {
  CHECK(delay_requests_.empty());
  CHECK(delay_map_.empty());
}

void DelayCache::LookupComplete(DelayCallback* callback) {
  StringSet::iterator p = delay_requests_.find(callback->key());
  if (p == delay_requests_.end()) {
    callback->Run();
  } else {
    CHECK(delay_map_.find(callback->key()) == delay_map_.end());
    delay_map_[callback->key()] = callback;
  }
}

void DelayCache::DelayKey(const GoogleString& key) {
  delay_requests_.insert(key);
}

void DelayCache::ReleaseKey(const GoogleString& key) {
  int erased = delay_requests_.erase(key);
  CHECK_EQ(1, erased);
  DelayMap::iterator p = delay_map_.find(key);
  CHECK(p != delay_map_.end());
  DelayCallback* callback = p->second;
  delay_map_.erase(p);
  callback->Run();
}

void DelayCache::Get(const GoogleString& key, Callback* callback) {
  cache_->Get(key, new DelayCallback(key, this, callback));
}

void DelayCache::Put(const GoogleString& key, SharedString* value) {
  cache_->Put(key, value);
}

void DelayCache::Delete(const GoogleString& key) {
  cache_->Delete(key);
}

}  // namespace net_instaweb
