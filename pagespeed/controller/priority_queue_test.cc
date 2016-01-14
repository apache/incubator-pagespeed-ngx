// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: cheesy@google.com (Steve Hill)

#include "pagespeed/controller/priority_queue.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

using testing::Eq;

namespace net_instaweb {

class PriorityQueueTest : public testing::Test {
 protected:
  void CheckTopIs(const GoogleString& expected_key, int expected_count) {
    const std::pair<const GoogleString*, int>& top = queue_.Top();
    const GoogleString& actual_key = *top.first;
    int actual_count = top.second;
    EXPECT_THAT(actual_key, Eq(expected_key));
    EXPECT_THAT(actual_count, Eq(expected_count));
  }

  void CheckSize(size_t expected_size) {
    if (expected_size == 0) {
      CheckEmpty();
    } else {
      EXPECT_THAT(queue_.Empty(), Eq(false));
      EXPECT_THAT(queue_.Size(), Eq(expected_size));
    }
  }

  void CheckEmpty() {
    EXPECT_THAT(queue_.Empty(), Eq(true));
    EXPECT_THAT(queue_.Size(), Eq(0));
  }

  void Increment(const GoogleString& v) {
    queue_.Increment(v);
    queue_.SanityCheckForTesting();
  }

  void IncreasePriority(const GoogleString& v, int howmuch) {
    queue_.IncreasePriority(v, howmuch);
    queue_.SanityCheckForTesting();
  }

  void Pop() {
    queue_.Pop();
    queue_.SanityCheckForTesting();
  }

 private:
  PriorityQueue<GoogleString> queue_;
};

namespace {

TEST_F(PriorityQueueTest, EmptyCase) {
  PriorityQueue<GoogleString> q;
  CheckEmpty();
}

TEST_F(PriorityQueueTest, SingleElement) {
  Increment("A");
  CheckSize(1);
  CheckTopIs("A", 1);

  Increment("A");
  CheckSize(1);
  CheckTopIs("A", 2);

  Pop();
  CheckEmpty();
}

TEST_F(PriorityQueueTest, ZeroValues) {
  IncreasePriority("A", 0);
  CheckSize(1);
  CheckTopIs("A", 0);

  Increment("B");
  CheckSize(2);
  CheckTopIs("B", 1);

  IncreasePriority("A", 2);
  CheckSize(2);
  CheckTopIs("A", 2);

  IncreasePriority("A", 0);
  CheckSize(2);
  CheckTopIs("A", 2);

  Pop();
  CheckSize(1);
  CheckTopIs("B", 1);

  Pop();
  CheckEmpty();
}

TEST_F(PriorityQueueTest, TwoElements) {
  Increment("A");
  Increment("B");
  CheckSize(2);
  CheckTopIs("A", 1);

  Increment("B");
  CheckTopIs("B", 2);
  CheckSize(2);

  Pop();
  CheckTopIs("A", 1);
  CheckSize(1);

  Pop();
  CheckEmpty();
}

TEST_F(PriorityQueueTest, TwoElementsWithIncreasePriority) {
  Increment("A");
  Increment("B");
  CheckSize(2);
  CheckTopIs("A", 1);

  IncreasePriority("B", 2);
  CheckTopIs("B", 3);
  CheckSize(2);

  Pop();
  CheckTopIs("A", 1);
  CheckSize(1);

  Pop();
  CheckEmpty();
}


TEST_F(PriorityQueueTest, InterleavedIncrementAndPop) {
  // A => 3, B => 2, C => 1 (in random-ish order).
  Increment("A");
  Increment("B");
  Increment("C");
  Increment("A");
  Increment("A");
  Increment("B");
  CheckTopIs("A", 3);
  Pop();  // Now B => 2, C => 1.
  CheckTopIs("B", 2);
  CheckSize(2);

  Increment("C");  // B => 2, C => 2.
  CheckTopIs("B", 2);
  Increment("C");  // B => 2, C => 3.
  CheckTopIs("C", 3);

  Pop();  // Now just B => 2.
  CheckTopIs("B", 2);

  Pop();
  CheckEmpty();
}

TEST_F(PriorityQueueTest, InterleavedIncrementAndPopWithIncrease) {
  // A => 3, B => 2, C => 1.
  IncreasePriority("C", 1);
  IncreasePriority("B", 2);
  IncreasePriority("A", 3);
  CheckTopIs("A", 3);
  Pop();  // Now B => 2, C => 1.
  CheckTopIs("B", 2);
  CheckSize(2);

  IncreasePriority("C", 2);  // B => 2, C => 3.
  CheckTopIs("C", 3);

  Pop();  // Now just B => 2.
  CheckTopIs("B", 2);

  Pop();
  CheckEmpty();
}


TEST_F(PriorityQueueTest, TortureTest) {
  // Populate the queue with values that have an ever increasing priority
  // (1 => 1, 2 => 2, etc). This will (eventually) force the newly inserted
  // value to be swapped all the way to the root, giving the "PushUp" code path
  // a good work-out.
  const int kNumValues = 100;
  for (int i = 1; i <= kNumValues; ++i) {
    std::string k = IntegerToString(i);
    std::string prev_k = IntegerToString(i - 1);
    for (int j = 1; j <= i; ++j) {
      Increment(k);
      if (j < i) {
        CheckTopIs(prev_k, i - 1);
      } else {
        CheckTopIs(k, j);
      }
    }
  }
  // Now pop the keys out. This thorougly exercises the "PushDown" swap path.
  for (int i = kNumValues; i > 0; --i) {
    CheckTopIs(IntegerToString(i), i);
    Pop();
  }
  CheckEmpty();
}

TEST_F(PriorityQueueTest, TortureTestWithIncrease) {
  // This is the same as TortureTest, except:
  // * Uses IncreasePriority instead of repeated calls to Increment.
  // * Priorities are 0-based instead of 1-based.
  // Populate the queue with values that have an ever increasing priority
  // (1 => 1, 2 => 2, etc). This will force the newly inserted value to be
  // swapped all the way to the root, giving the "PushUp" code path a good
  // work-out.
  const int kNumValues = 100;
  for (int i = 0; i < kNumValues; ++i) {
    std::string k = IntegerToString(i);
    IncreasePriority(k, i);
    CheckTopIs(k, i);
  }
  // Now pop the keys out. This thorougly exercises the "PushDown" swap path.
  for (int i = kNumValues - 1; i >= 0; --i) {
    CheckTopIs(IntegerToString(i), i);
    Pop();
  }
  CheckEmpty();
}

TEST_F(PriorityQueueTest, Destructor) {
  // Populate the queue but then don't remove anything. This ensures the
  // destructor actually deletes the items properly.
  for (int i = 0; i < 100; ++i) {
    Increment(IntegerToString(i));
  }
  CheckSize(100);
}

}  // namespace

}  // namespace net_instaweb
