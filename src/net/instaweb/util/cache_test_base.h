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

#include <vector>

#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class CacheTestBase : public testing::Test {
 public:
  // Helper class for calling Get on cache implementations
  // that are blocking in nature (e.g. in-memory LRU or blocking file-system).
  class Callback : public CacheInterface::Callback {
   public:
    Callback() { Reset(); }
    virtual ~Callback() {}
    Callback* Reset() {
      called_ = false;
      validate_called_ = false;
      state_ = CacheInterface::kNotFound;
      SharedString empty;
      *value() = empty;
      invalid_value_ = NULL;
      return this;
    }

    virtual bool ValidateCandidate(const GoogleString& key,
                                   CacheInterface::KeyState state) {
      validate_called_ = true;
      if ((invalid_value_ != NULL) && (*value()->get() == invalid_value_)) {
        return false;
      }
      return true;
    }

    virtual void Done(CacheInterface::KeyState state) {
      EXPECT_TRUE(validate_called_);
      called_ = true;
      state_ = state;
    }

    void set_invalid_value(const char* v) { invalid_value_ = v; }
    CacheInterface::KeyState state() const { return state_; }
    bool called() const { return called_; }
    const GoogleString& value_str() { return **value(); }

    bool called_;
    bool validate_called_;
    CacheInterface::KeyState state_;

   private:
    const char* invalid_value_;

    DISALLOW_COPY_AND_ASSIGN(Callback);
  };

  CacheTestBase() : invalid_value_(NULL) {
  }
  ~CacheTestBase() {
    STLDeleteElements(&callbacks_);
  }

  void CheckGet(const char* key, const GoogleString& expected_value) {
    CheckGet(Cache(), key, expected_value);
  }

  void CheckGet(CacheInterface* cache, const char* key,
                const GoogleString& expected_value) {
    cache->Get(key, ResetCallback());
    ASSERT_TRUE(callback_.called_);
    ASSERT_EQ(CacheInterface::kAvailable, callback_.state_)
        << "For key: " << key;
    EXPECT_EQ(expected_value, **callback_.value()) << "For key: " << key;
    SanityCheck();
  }

  void CheckPut(const char* key, const char* value) {
    CheckPut(Cache(), key, value);
  }

  void CheckPut(const GoogleString& key, const GoogleString& value) {
    CheckPut(Cache(), key.c_str(), value.c_str());
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
    cache->Get(key, ResetCallback());
    ASSERT_TRUE(callback_.called_);
    EXPECT_NE(CacheInterface::kAvailable, callback_.state_)
        << "For key: " << key;
    SanityCheck();
  }

  Callback* NewCallback() {
    Callback* callback = new Callback();
    callbacks_.push_back(callback);
    return callback;
  }

  void CheckMultiGetValue(int index, const GoogleString& value) {
    Callback* callback = callbacks_[index];
    ASSERT_TRUE(callback->called());
    EXPECT_EQ(value, callback->value_str());
    EXPECT_EQ(CacheInterface::kAvailable, callback->state());
  }

  void CheckMultiGetNotFound(int index) {
    Callback* callback = callbacks_[index];
    ASSERT_TRUE(callback->called());
    EXPECT_EQ(CacheInterface::kNotFound, callback->state());
  }

  void TestMultiGet() {
    CheckPut("n1", "v1");
    CheckPut("n2", "v2");
    CacheInterface::MultiGetRequest* request =
        new CacheInterface::MultiGetRequest;
    request->push_back(CacheInterface::KeyCallback("n1", NewCallback()));
    request->push_back(CacheInterface::KeyCallback("not found", NewCallback()));
    request->push_back(CacheInterface::KeyCallback("n2", NewCallback()));
    Cache()->MultiGet(request);
    CheckMultiGetValue(0, "v1");
    CheckMultiGetNotFound(1);
    CheckMultiGetValue(2, "v2");
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
    callback_.set_invalid_value(invalid_value_);
    return &callback_;
  }

  void set_invalid_value(const char* v) { invalid_value_ = v; }

 private:
  const char* invalid_value_;  // may be NULL.
  Callback callback_;
  std::vector<Callback*> callbacks_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_CACHE_TEST_BASE_H_
