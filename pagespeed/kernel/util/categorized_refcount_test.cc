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

// Author: morlovich@google.com (Maksim Orlovich)
//
// Unit tests for CategorizedRefcount

#include "pagespeed/kernel/util/categorized_refcount.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {
namespace {

class Client {
 public:
  enum RefCategory {
    kRewrites = 0,
    kFetches,
    kNumRefCategories
  };

  Client() : last_ref_removed_called_(false) {}

  bool last_ref_removed_called() const { return last_ref_removed_called_; }

 private:
  friend class CategorizedRefcount<Client, Client::RefCategory>;

  void LastRefRemoved() { last_ref_removed_called_ = true; }

  StringPiece RefCategoryName(RefCategory cat) {
    switch (cat) {
      case kRewrites:
        return "Rewrites";
      case kFetches:
        return "Fetches";
      case kNumRefCategories:
      default:
        CHECK(false);
        return "???";
    }
  }

  bool last_ref_removed_called_;
};

class CategorizedRefcountTest : public testing::Test {
 protected:
  CategorizedRefcountTest()
      : thread_system_(Platform::CreateThreadSystem()),
        mutex_(thread_system_->NewMutex()),
        count_(&client_) {
    count_.set_mutex(mutex_.get());
  }

  scoped_ptr<ThreadSystem> thread_system_;
  scoped_ptr<AbstractMutex> mutex_;
  Client client_;
  CategorizedRefcount<
      Client, Client::RefCategory> count_;
};

TEST_F(CategorizedRefcountTest, Basic) {
  count_.DCheckAllCountsZero();

  count_.AddRef(Client::kRewrites);
  EXPECT_FALSE(client_.last_ref_removed_called());
  EXPECT_EQ("\tRewrites: 1\tFetches: 0", count_.DebugString());

  count_.AddRef(Client::kFetches);
  EXPECT_FALSE(client_.last_ref_removed_called());
  EXPECT_EQ("\tRewrites: 1\tFetches: 1", count_.DebugString());

  count_.ReleaseRef(Client::kRewrites);
  EXPECT_FALSE(client_.last_ref_removed_called());
  EXPECT_EQ("\tRewrites: 0\tFetches: 1", count_.DebugString());

  count_.ReleaseRef(Client::kFetches);
  EXPECT_TRUE(client_.last_ref_removed_called());
  EXPECT_EQ("\tRewrites: 0\tFetches: 0", count_.DebugString());

  count_.DCheckAllCountsZero();
}

TEST_F(CategorizedRefcountTest, ExternalLock) {
  ScopedMutex lock(mutex_.get());
  count_.DCheckAllCountsZeroMutexHeld();

  count_.AddRefMutexHeld(Client::kRewrites);
  EXPECT_FALSE(client_.last_ref_removed_called());
  EXPECT_EQ("\tRewrites: 1\tFetches: 0", count_.DebugStringMutexHeld());

  count_.AddRefMutexHeld(Client::kFetches);
  EXPECT_FALSE(client_.last_ref_removed_called());
  EXPECT_EQ("\tRewrites: 1\tFetches: 1", count_.DebugStringMutexHeld());

  count_.ReleaseRefMutexHeld(Client::kRewrites);
  EXPECT_FALSE(client_.last_ref_removed_called());
  EXPECT_EQ("\tRewrites: 0\tFetches: 1", count_.DebugStringMutexHeld());

  count_.ReleaseRefMutexHeld(Client::kFetches);
  EXPECT_TRUE(client_.last_ref_removed_called());
  EXPECT_EQ("\tRewrites: 0\tFetches: 0", count_.DebugStringMutexHeld());

  count_.DCheckAllCountsZeroMutexHeld();
}

}  // namespace
}  // namespace net_instaweb
