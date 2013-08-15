/*
 * Copyright 2013 Google Inc.
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

// Author: pulkitg@google.com (Pulkit Goyal)

// Unit test for PropertyStore.

#include "net/instaweb/util/public/property_store.h"

#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/platform.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/thread_system.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/scoped_ptr.h"

namespace net_instaweb {

class PropertyStoreTest : public testing::Test {
 public:
  PropertyStoreTest()
      : thread_system_(Platform::CreateThreadSystem()),
        num_callback_with_false_called_(0),
        num_callback_with_true_called_(0),
        timer_(MockTimer::kApr_5_2010_ms) {
    PropertyStoreGetCallback::InitStats(&stats_);
  }

  void ExpectCallback(bool result) {
    if (result) {
      ++num_callback_with_true_called_;
    } else {
      ++num_callback_with_false_called_;
    }
  }

  PropertyStoreGetCallback* GetCallback(bool is_cancellable) {
    return new PropertyStoreGetCallback(
        thread_system_->NewMutex(),
        NULL,
        is_cancellable,
        NewCallback(this, &PropertyStoreTest::ExpectCallback),
        &timer_);
  }

 protected:
  scoped_ptr<ThreadSystem> thread_system_;
  int num_callback_with_false_called_;
  int num_callback_with_true_called_;

 private:
  SimpleStats stats_;
  MockTimer timer_;
  DISALLOW_COPY_AND_ASSIGN(PropertyStoreTest);
};

TEST_F(PropertyStoreTest, TestNonCancellableNoFastFinishLookupDoneWithTrue) {
  PropertyStoreGetCallback* callback = GetCallback(false);
  callback->Done(true);
  callback->DeleteWhenDone();
  EXPECT_EQ(0, num_callback_with_false_called_);
  EXPECT_EQ(1, num_callback_with_true_called_);
}

TEST_F(PropertyStoreTest, TestNonCancellableNoFastFinishLookupDoneWithFalse) {
  PropertyStoreGetCallback* callback = GetCallback(false);
  callback->Done(false);
  callback->DeleteWhenDone();
  EXPECT_EQ(1, num_callback_with_false_called_);
  EXPECT_EQ(0, num_callback_with_true_called_);
}

TEST_F(PropertyStoreTest, TestNonCancellableFastFinishLookupAfterDoneWithTrue) {
  PropertyStoreGetCallback* callback = GetCallback(false);
  callback->Done(true);
  callback->FastFinishLookup();
  callback->DeleteWhenDone();
  EXPECT_EQ(0, num_callback_with_false_called_);
  EXPECT_EQ(1, num_callback_with_true_called_);
}

TEST_F(PropertyStoreTest,
       TestNonCancellableFastFinishLookupAfterDoneWithFalse) {
  PropertyStoreGetCallback* callback = GetCallback(false);
  callback->Done(false);
  callback->FastFinishLookup();
  callback->DeleteWhenDone();
  EXPECT_EQ(1, num_callback_with_false_called_);
  EXPECT_EQ(0, num_callback_with_true_called_);
}

TEST_F(PropertyStoreTest,
       TestNonCancellableFastFinishLookupBeforeDoneWithTrue) {
  PropertyStoreGetCallback* callback = GetCallback(false);
  callback->FastFinishLookup();
  callback->Done(true);
  callback->DeleteWhenDone();
  EXPECT_EQ(0, num_callback_with_false_called_);
  EXPECT_EQ(1, num_callback_with_true_called_);
}

TEST_F(PropertyStoreTest,
       TestNonCancellableFastFinishLookupBeforeDoneWithFalse) {
  PropertyStoreGetCallback* callback = GetCallback(false);
  callback->FastFinishLookup();
  callback->Done(false);
  callback->DeleteWhenDone();
  EXPECT_EQ(1, num_callback_with_false_called_);
  EXPECT_EQ(0, num_callback_with_true_called_);
}

TEST_F(PropertyStoreTest, TestCancellableNoFastFinishLookupDoneWithTrue) {
  PropertyStoreGetCallback* callback = GetCallback(true);
  callback->Done(true);
  callback->DeleteWhenDone();
  EXPECT_EQ(0, num_callback_with_false_called_);
  EXPECT_EQ(1, num_callback_with_true_called_);
}

TEST_F(PropertyStoreTest, TestCancellableNoFastFinishLookupDoneWithFalse) {
  PropertyStoreGetCallback* callback = GetCallback(true);
  callback->Done(false);
  callback->DeleteWhenDone();
  EXPECT_EQ(1, num_callback_with_false_called_);
  EXPECT_EQ(0, num_callback_with_true_called_);
}

TEST_F(PropertyStoreTest, TestCancellableFastFinishLookupAfterDoneWithTrue) {
  PropertyStoreGetCallback* callback = GetCallback(true);
  callback->Done(true);
  callback->FastFinishLookup();
  callback->DeleteWhenDone();
  EXPECT_EQ(0, num_callback_with_false_called_);
  EXPECT_EQ(1, num_callback_with_true_called_);
}

TEST_F(PropertyStoreTest,
       TestCancellableFastFinishLookupAfterDoneWithFalse) {
  PropertyStoreGetCallback* callback = GetCallback(true);
  callback->Done(false);
  callback->FastFinishLookup();
  callback->DeleteWhenDone();
  EXPECT_EQ(1, num_callback_with_false_called_);
  EXPECT_EQ(0, num_callback_with_true_called_);
}

TEST_F(PropertyStoreTest,
       TestCancellableFastFinishLookupBeforeDoneWithTrue) {
  PropertyStoreGetCallback* callback = GetCallback(true);
  callback->FastFinishLookup();
  callback->Done(true);
  callback->DeleteWhenDone();
  EXPECT_EQ(1, num_callback_with_false_called_);
  EXPECT_EQ(0, num_callback_with_true_called_);
}

TEST_F(PropertyStoreTest,
       TestCancellableFastFinishLookupBeforeDoneWithFalse) {
  PropertyStoreGetCallback* callback = GetCallback(true);
  callback->FastFinishLookup();
  callback->Done(false);
  callback->DeleteWhenDone();
  EXPECT_EQ(1, num_callback_with_false_called_);
  EXPECT_EQ(0, num_callback_with_true_called_);
}

TEST_F(PropertyStoreTest,
       TestDeleteWhenDoneBeforeDoneWithFalse) {
  PropertyStoreGetCallback* callback = GetCallback(true);
  callback->DeleteWhenDone();
  callback->Done(false);
  EXPECT_EQ(1, num_callback_with_false_called_);
  EXPECT_EQ(0, num_callback_with_true_called_);
}

}  // namespace net_instaweb
