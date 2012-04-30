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
// delays before callback invocations of a cache object.
//
// Note: DelayCache also supports delayed callbacks, but each key's
// delivery is controlled by API.
//
// TODO(jmarantz): consider refactoring this as a subclass of DelayCache.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_MOCK_TIME_CACHE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_MOCK_TIME_CACHE_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class SharedString;
class MockTimer;

// See file comment
class MockTimeCache : public CacheInterface {
 public:
  // Note: takes ownership of nothing.
  MockTimeCache(MockTimer* timer, CacheInterface* cache);
  virtual ~MockTimeCache();

  // Reimplementations of CacheInterface methods.
  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, SharedString* value);
  virtual void Delete(const GoogleString& key);

  // Sets the delay the cache will inject before invoking the callbacks.
  // Note that this only affects the 'Done' callback; 'ValidateCandidate'
  // happens immediately.
  void set_delay_us(int64 delay_us) { delay_us_ = delay_us; }
  int64 delay_us() const { return delay_us_; }

  MockTimer* timer() { return timer_; }

  virtual const char* Name() const { return name_.c_str(); }

 private:
  class DelayCallback;

  MockTimer* timer_;
  CacheInterface* cache_;
  int64 delay_us_;
  GoogleString name_;

  DISALLOW_COPY_AND_ASSIGN(MockTimeCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_MOCK_TIME_CACHE_H_
