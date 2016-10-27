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

#ifndef PAGESPEED_KERNEL_CACHE_CACHE_TEST_BASE_H_
#define PAGESPEED_KERNEL_CACHE_CACHE_TEST_BASE_H_

#include <vector>

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/cache_interface.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class CacheTestBase : public testing::Test {
 public:
  // Helper class for calling Get on cache implementations that are blocking
  // in nature (e.g. in-memory LRU or blocking file-system). Also tests the
  // CacheInterface::SynchronousCallback class in the process.
  class Callback : public CacheInterface::SynchronousCallback {
   public:
    explicit Callback(CacheTestBase* test) : test_(test) { Reset(); }
    Callback() : test_(NULL) { Reset(); }
    virtual ~Callback() {}
    Callback* Reset() {
      SynchronousCallback::Reset();
      validate_called_ = false;
      noop_wait_called_ = false;
      value_of_called_when_wait_was_invoked_ = false;
      invalid_value_ = NULL;
      invalid_key_ = NULL;
      return this;
    }

    virtual bool ValidateCandidate(const GoogleString& key,
                                   CacheInterface::KeyState state) {
      validate_called_ = true;
      if ((invalid_value_ != NULL) && (value_str() == invalid_value_)) {
        return false;
      }
      if ((invalid_key_ != NULL) && (key == invalid_key_)) {
        return false;
      }
      return true;
    }

    virtual void Done(CacheInterface::KeyState state) {
      SynchronousCallback::Done(state);
      EXPECT_TRUE(validate_called_);
      if (test_ != NULL) {
        test_->GetDone();
      }
    }

    // The default implementation has an empty Wait implementation.
    // If you override this, be sure also to call set_mutex() from the
    // test subclass constructor or SetUp to protect outstanding_fetches_.
    virtual void Wait() {
      noop_wait_called_ = true;
      value_of_called_when_wait_was_invoked_ = called();
    }

    void set_invalid_value(const char* v) { invalid_value_ = v; }
    void set_invalid_key(const char* k) { invalid_key_ = k; }
    StringPiece value_str() { return value().Value(); }

    bool validate_called_;
    bool noop_wait_called_;
    bool value_of_called_when_wait_was_invoked_;

   private:
    CacheTestBase* test_;
    const char* invalid_value_;
    const char* invalid_key_;

    DISALLOW_COPY_AND_ASSIGN(Callback);
  };

 protected:
  CacheTestBase()
      : invalid_value_(NULL),
        invalid_key_(NULL),
        mutex_(new NullMutex),
        outstanding_fetches_(0) {
  }
  ~CacheTestBase() {
    STLDeleteElements(&callbacks_);
  }

  // Allocates a callback structure.  The default Callback structure
  // has an empty implementation of Wait().
  virtual Callback* NewCallback() { return new Callback(this); }
  virtual CacheInterface* Cache() = 0;

  // Optional method that can be specified by a test subclass to specify
  // an operation that should be performed after Get or Put.
  virtual void PostOpCleanup() {}

  // Performs a cache Get, waits for callback completion, and checks the
  // result is as expected.
  void CheckGet(const GoogleString& key, const GoogleString& expected_value) {
    CheckGet(Cache(), key, expected_value);
  }

  // As above, but specifies which cache to use.
  void CheckGet(CacheInterface* cache, const GoogleString& key,
                const GoogleString& expected_value) {
    Callback* callback = InitiateGet(cache, key);
    WaitAndCheck(callback, expected_value);
  }

  // Writes a value into the cache.
  void CheckPut(const GoogleString& key, const GoogleString& value) {
    CheckPut(Cache(), key, value);
  }

  void CheckPut(CacheInterface* cache, const GoogleString& key,
                const GoogleString& value) {
    cache->Put(key, SharedString(value));
    PostOpCleanup();
  }

  void CheckDelete(const GoogleString& key) {
    Cache()->Delete(key);
    PostOpCleanup();
  }

  // Performs a Get and verifies that the key is not found.
  void CheckNotFound(const char* key) {
    CheckNotFound(Cache(), key);
  }

  void CheckNotFound(CacheInterface* cache, const char* key) {
    Callback* callback = InitiateGet(cache, key);
    WaitAndCheckNotFound(callback);
  }

  // Adds a new callback to the callback-array, returning a Callback*
  // that can then be passed to WaitAndCheck and WaitAndCheckNotFound.
  Callback* AddCallback() {
    Callback* callback = NewCallback();
    callback->set_invalid_value(invalid_value_);
    callback->set_invalid_key(invalid_key_);
    callbacks_.push_back(callback);
    return callback;
  }

  void WaitAndCheck(Callback* callback, const GoogleString& expected_value) {
    // noop_wait_called_ means the callback hasn't overridden Wait() so
    // Callback's do-nothing method was called; if that's the case, then
    // we expect the cache to be blocking, and we expect the Get() call
    // to have invoked Done() before the do-nothing Wait(). Since we can't
    // know which version of Wait() was called before calling it, we use the
    // value of called() as noted by the do-nothing Wait().
    callback->Wait();
    if (callback->noop_wait_called_) {
      EXPECT_TRUE(callback->value_of_called_when_wait_was_invoked_);
      // We'd like to test this but it's valid for a CacheInterface to report
      // that it is not blocking even though it actually is (or can be).
      // DelayCache is an example.
      // EXPECT_TRUE(Cache()->IsBlocking());
    }
    ASSERT_TRUE(callback->called());
    EXPECT_STREQ(expected_value, callback->value_str());
    EXPECT_EQ(CacheInterface::kAvailable, callback->state());
    PostOpCleanup();
  }

  void WaitAndCheckNotFound(Callback* callback) {
    callback->Wait();
    ASSERT_TRUE(callback->called());
    EXPECT_EQ(CacheInterface::kNotFound, callback->state());
    PostOpCleanup();
  }

  void IssueMultiGet(Callback* c0, const GoogleString& key0,
                     Callback* c1, const GoogleString& key1,
                     Callback* c2, const GoogleString& key2) {
    CacheInterface::MultiGetRequest* request =
        new CacheInterface::MultiGetRequest;
    request->push_back(CacheInterface::KeyCallback(key0, c0));
    request->push_back(CacheInterface::KeyCallback(key1, c1));
    request->push_back(CacheInterface::KeyCallback(key2, c2));
    Cache()->MultiGet(request);
  }

  void TestMultiGet() {
    PopulateCache(2);
    Callback* n0 = AddCallback();
    Callback* not_found = AddCallback();
    Callback* n1 = AddCallback();
    IssueMultiGet(n0, "n0", not_found, "not_found", n1, "n1");
    WaitAndCheck(n0, "v0");
    WaitAndCheckNotFound(not_found);
    WaitAndCheck(n1, "v1");
  }

  // Populates the cache with keys in pattern n0 n1 n2 n3...
  // and values in pattern v0 v1 v2 v3...
  void PopulateCache(int num) {
    for (int i = 0; i < num; ++i) {
      CheckPut(StringPrintf("n%d", i), StringPrintf("v%d", i));
    }
  }

  void set_invalid_value(const char* v) { invalid_value_ = v; }
  void set_invalid_key(const char* k) { invalid_key_ = k; }

  // Initiate a cache Get, and return the Callback* which can be
  // passed to WaitAndCheck or WaitAndCheckNotFound.
  Callback* InitiateGet(const GoogleString& key) {
    return InitiateGet(Cache(), key);
  }

  Callback* InitiateGet(CacheInterface* cache, const GoogleString& key) {
    {
      ScopedMutex lock(mutex_.get());
      ++outstanding_fetches_;
    }
    Callback* callback = AddCallback();
    cache->Get(key, callback);
    return callback;
  }

  // Sets the mutex used to protect outstanding_fetches_.
  void set_mutex(AbstractMutex* mutex) { mutex_.reset(mutex); }

  // Returns the number of outstanding Get requests.  The return value
  // makes sense only if the cache system is quiescent.
  int outstanding_fetches() {
    ScopedMutex lock(mutex_.get());
    return outstanding_fetches_;
  }

 private:
  // Called from Callback::Done to track outstanding fetches -- for
  // callbacks created with NewCallback().
  //
  // TODO(jmarantz): eliminate the default ctor for Callback and change
  // all cache tests to use NewCallback() to remove that special case.
  void GetDone() {
    ScopedMutex lock(mutex_.get());
    --outstanding_fetches_;
  }

  const char* invalid_value_;  // may be NULL.
  const char* invalid_key_;  // may be NULL.
  std::vector<Callback*> callbacks_;
  scoped_ptr<AbstractMutex> mutex_;
  int outstanding_fetches_;  // protected by mutex_
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_CACHE_CACHE_TEST_BASE_H_
