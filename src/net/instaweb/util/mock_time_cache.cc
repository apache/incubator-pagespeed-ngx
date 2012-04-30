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

// Author: morlovich@google.com (Maksim Orlovich)
//
// Contains MockTimeCache, which lets one inject MockTimer-simulated
// delays before callback invocations of a cache object, as well as
// MockTimeCache::Callback, which chains to a passed-in callback to actually
// implement the delay.

#include "net/instaweb/util/public/mock_time_cache.h"

#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/shared_string.h"

namespace net_instaweb {

// This calls a passed-in callback with a mock time delay, forwarding
// on lookup results.
class MockTimeCache::DelayCallback : public CacheInterface::Callback {
 public:
  DelayCallback(MockTimeCache* parent, Callback* orig_callback)
      : parent_(parent), orig_callback_(orig_callback) {}

  virtual ~DelayCallback() {}

  virtual bool ValidateCandidate(const GoogleString& key, KeyState state) {
    *orig_callback_->value() = *value();
    return orig_callback_->DelegatedValidateCandidate(key, state);
  }

  virtual void Done(KeyState state) {
    parent_->timer()->AddAlarm(
        parent_->timer()->NowUs() + parent_->delay_us(),
        new MemberFunction1<CacheInterface::Callback, CacheInterface::KeyState>(
            &CacheInterface::Callback::DelegatedDone, orig_callback_, state));
    delete this;
  };

 private:
  MockTimeCache* parent_;
  Callback* orig_callback_;

  DISALLOW_COPY_AND_ASSIGN(DelayCallback);
};

MockTimeCache::MockTimeCache(MockTimer* timer, CacheInterface* cache)
    : timer_(timer),
      cache_(cache),
      delay_us_(0),
      name_(StrCat("MockTimeCache using ", cache_->Name())) {}

MockTimeCache::~MockTimeCache() {}

void MockTimeCache::Get(const GoogleString& key, Callback* callback) {
  if (delay_us_ == 0) {
    cache_->Get(key, callback);
  } else {
    cache_->Get(key, new DelayCallback(this, callback));
  }
}

void MockTimeCache::Put(const GoogleString& key, SharedString* value) {
  cache_->Put(key, value);
}

void MockTimeCache::Delete(const GoogleString& key) {
  cache_->Delete(key);
}

}  // namespace net_instaweb
