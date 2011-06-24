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

// Shared infrastructure for testing cache implementations

#ifndef NET_INSTAWEB_UTIL_CACHE_TEST_BASE_H_
#define NET_INSTAWEB_UTIL_CACHE_TEST_BASE_H_

#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class CacheTestBase : public testing::Test {
 public:
  // Helper class for calling Get on cache implementations
  // that are blocking in nature (e.g. in-memory LRU or blocking file-system).
  class Callback : public CacheInterface::Callback {
   public:
    Callback() { Reset(); }
    Callback* Reset() {
      called_ = false;
      state_ = CacheInterface::kNotFound;
      SharedString empty;
      *value() = empty;
      return this;
    }
    virtual void Done(CacheInterface::KeyState state) {
      called_ = true;
      state_ = state;
    }
    bool called_;
    CacheInterface::KeyState state_;
  };

  void CheckGet(const char* key, const GoogleString& expected_value) {
    CheckGet(Cache(), key, expected_value);
  }

  void CheckGet(CacheInterface* cache, const char* key,
                const GoogleString& expected_value) {
    cache->Get(key, callback_.Reset());
    ASSERT_TRUE(callback_.called_);
    ASSERT_EQ(CacheInterface::kAvailable, callback_.state_)
        << "For key: " << key;
    EXPECT_EQ(expected_value, **callback_.value()) << "For key: " << key;
    SanityCheck();
  }

  void CheckPut(const char* key, const char* value) {
    CheckPut(Cache(), key, value);
  }

  void CheckPut(CacheInterface* cache, const char* key, const char* value) {
    SharedString put_buffer(value);
    cache->Put(key, &put_buffer);
    SanityCheck();
  }

  void CheckNotFound(const char* key) {
    CheckNotFound(Cache(), key);
  }

  void CheckNotFound(CacheInterface* cache, const char* key) {
    cache->Get(key, callback_.Reset());
    ASSERT_TRUE(callback_.called_);
    EXPECT_NE(CacheInterface::kAvailable, callback_.state_)
        << "For key: " << key;
    SanityCheck();
  }

 protected:
  virtual CacheInterface* Cache() = 0;
  virtual void SanityCheck() {
  }

  // Returns a read-only view of the callback for helping determine whether
  // it was called, and with what state.
  const Callback& callback() const { return callback_; }

  // Clears out the callback to its initial state so it can be
  // passed into a CacheInterface method.
  Callback* ResetCallback() {
    callback_.Reset();
    return &callback_;
  }

 private:
  Callback callback_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_CACHE_TEST_BASE_H_
