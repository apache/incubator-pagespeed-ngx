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
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

using testing::Eq;

namespace net_instaweb {

class PriorityQueueTest : public testing::Test {
 protected:
  void CheckTopIs(const GoogleString& expected_key, int expected_count) const {
    const std::pair<const GoogleString*, int>& top = queue_.Top();
    const GoogleString& actual_key = *top.first;
    int actual_count = top.second;
    EXPECT_THAT(actual_key, Eq(expected_key));
    EXPECT_THAT(actual_count, Eq(expected_count));
  }

  void CheckSize(size_t expected_size) const {
    if (expected_size == 0) {
      CheckEmpty();
    } else {
      EXPECT_THAT(queue_.Empty(), Eq(false));
      EXPECT_THAT(queue_.Size(), Eq(expected_size));
    }
  }

  void CheckEmpty() const {
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

  void Remove(const GoogleString& v) {
    queue_.Remove(v);
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

TEST_F(PriorityQueueTest, NegativeValues) {
  Increment("A");
  CheckSize(1);
  CheckTopIs("A", 1);

  IncreasePriority("B", -1);
  CheckSize(2);
  CheckTopIs("A", 1);

  IncreasePriority("C", -2);
  CheckSize(3);
  CheckTopIs("A", 1);

  IncreasePriority("C", 4);
  CheckSize(3);
  CheckTopIs("C", 2);

  IncreasePriority("A", -3);
  CheckSize(3);
  CheckTopIs("C", 2);

  Pop();
  CheckSize(2);
  CheckTopIs("B", -1);

  Pop();
  CheckSize(1);
  CheckTopIs("A", -2);

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

TEST_F(PriorityQueueTest, RemoveOnlyEntry) {
  Increment("A");
  CheckSize(1);
  CheckTopIs("A", 1);

  Remove("A");
  CheckEmpty();
}

TEST_F(PriorityQueueTest, RemoveLastEntry) {
  IncreasePriority("A", 2);
  IncreasePriority("B", 1);
  CheckSize(2);
  CheckTopIs("A", 2);

  Remove("B");
  CheckSize(1);
  CheckTopIs("A", 2);

  Pop();
  CheckEmpty();
}

TEST_F(PriorityQueueTest, BasicRemove) {
  IncreasePriority("A", 1);
  IncreasePriority("B", 2);
  IncreasePriority("C", 3);
  IncreasePriority("D", 4);

  CheckSize(4);
  CheckTopIs("D", 4);

  // Remove the top value.
  Remove("D");
  CheckSize(3);
  CheckTopIs("C", 3);

  // Remove a non-top value.
  Remove("B");
  CheckSize(2);
  CheckTopIs("C", 3);

  Pop();
  CheckSize(1);
  CheckTopIs("A", 1);

  Pop();
  CheckEmpty();
}

TEST_F(PriorityQueueTest, RemoveNonExistentEmpty) {
  Remove("F");
  CheckEmpty();
}

TEST_F(PriorityQueueTest, RemoveNonExistentNotEmpty) {
  Increment("J");
  CheckSize(1);
  CheckTopIs("J", 1);

  Remove("L");
  CheckSize(1);
  CheckTopIs("J", 1);

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

TEST_F(PriorityQueueTest, NegativeFlipFlop) {
  const int kNumValues = 100;
  // Fill the queue with values 0, 1, -2, 3, -4, etc.
  for (int i = 0; i < kNumValues; ++i) {
    int v = ((i % 2) == 1) ? i : -i;
    IncreasePriority(IntegerToString(v), v);
  }

  // Now check the values come out in order.
  for (int i = 0; i < kNumValues; ++i) {
    // Numbers come out in order: Odd numbers 99 -> 1, Even numbers 0 -> -98.
    int expected_value = 100 - 2 * i;
    if (expected_value > 0) {
      --expected_value;
    }
    CheckTopIs(IntegerToString(expected_value), expected_value);
    Pop();
  }
  CheckEmpty();
}

TEST_F(PriorityQueueTest, RemoveMany) {
  const int kNumValues = 100;
  // Add 100 values into the queue.
  for (int i = 1; i <= kNumValues; ++i) {
    IncreasePriority(IntegerToString(i), i);
  }

  // Remove values of i either side powers of 2 (3, 5, 7, 9, etc).
  for (int i = 2; i < kNumValues; i = (i << 1)) {
    Remove(IntegerToString(i - 1));
    Remove(IntegerToString(i + 1));
  }

  // Check the values come out in order, being sure to skip the values
  // either side of powers of 2.
  int next_power_2 = 64;
  for (int i = kNumValues; i > 0; --i) {
    bool expect_present = true;
    if (i + 1 == next_power_2) {
      expect_present = false;
      next_power_2 >>= 1;
    } else if (i - 1 == next_power_2) {
      expect_present = false;
    }
    if (expect_present) {
      CheckTopIs(IntegerToString(i), i);
      Pop();
    }
  }
}

}  // namespace

}  // namespace net_instaweb
