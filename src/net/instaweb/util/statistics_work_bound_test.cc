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

// Author: jmaessen@google.com (Jan Maessen)

// Unit-test the statistics work bound.

#include "net/instaweb/util/public/statistics_work_bound.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/simple_stats.h"

namespace net_instaweb {
class Variable;

namespace {

class StatisticsWorkBoundTest : public testing::Test {
 public:
  StatisticsWorkBoundTest()
      : stats_(),
        var1_(stats_.AddVariable("var1")),
        var2_(stats_.AddVariable("var2")) { }

 protected:
  SimpleStats stats_;
  Variable* var1_;
  Variable* var2_;

  StatisticsWorkBound* MakeBound(Variable* var, int bound) {
    StatisticsWorkBound* result = new StatisticsWorkBound(var, bound);
    CHECK(NULL != result);
    return result;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StatisticsWorkBoundTest);
};

// Test with a bound of two.
TEST_F(StatisticsWorkBoundTest, TestTwoBound) {
  // We allocate two objects backed by the same statistic,
  // to ensure that they share a common count.  This is what
  // happens in a multi-process setting.
  scoped_ptr<StatisticsWorkBound> bound1(MakeBound(var1_, 2));
  scoped_ptr<StatisticsWorkBound> bound2(MakeBound(var1_, 2));
  // Repeat twice to ensure that we actually made it back to 0.
  for (int i = 0; i < 2; i++) {
    // Start with no workers.
    EXPECT_TRUE(bound1->TryToWork());
    // One worker here.
    EXPECT_TRUE(bound1->TryToWork());
    EXPECT_FALSE(bound1->TryToWork());
    EXPECT_FALSE(bound2->TryToWork());
    bound1->WorkComplete();
    // One worker here.
    EXPECT_TRUE(bound2->TryToWork());
    EXPECT_FALSE(bound1->TryToWork());
    EXPECT_FALSE(bound2->TryToWork());
    bound1->WorkComplete();
    // Back to one worker.
    EXPECT_TRUE(bound2->TryToWork());
    EXPECT_FALSE(bound1->TryToWork());
    EXPECT_FALSE(bound2->TryToWork());
    bound2->WorkComplete();
    // Back to one worker.
    bound2->WorkComplete();
    // Back to none.
  }
}

// Test that a bound of 0 allows large # of tries.
TEST_F(StatisticsWorkBoundTest, TestZeroBound) {
  scoped_ptr<StatisticsWorkBound> bound1(MakeBound(var1_, 0));
  scoped_ptr<StatisticsWorkBound> bound2(MakeBound(var1_, 0));
  for (int i = 0; i < 1000; ++i) {
    EXPECT_TRUE(bound1->TryToWork());
    EXPECT_TRUE(bound2->TryToWork());
  }
}

// Test that absent variable allows large # of tries.
TEST_F(StatisticsWorkBoundTest, TestNullVar) {
  scoped_ptr<StatisticsWorkBound> bound1(MakeBound(NULL, 2));
  scoped_ptr<StatisticsWorkBound> bound2(MakeBound(NULL, 2));
  for (int i = 0; i < 1000; ++i) {
    EXPECT_TRUE(bound1->TryToWork());
    EXPECT_TRUE(bound2->TryToWork());
  }
}

// Test that differently-named bounds are distinct
TEST_F(StatisticsWorkBoundTest, TestDistinctVar) {
  scoped_ptr<StatisticsWorkBound> bound1(MakeBound(var1_, 2));
  scoped_ptr<StatisticsWorkBound> bound2(MakeBound(var2_, 2));
  EXPECT_TRUE(bound1->TryToWork());
  EXPECT_TRUE(bound1->TryToWork());
  EXPECT_FALSE(bound1->TryToWork());
  EXPECT_TRUE(bound2->TryToWork());
  EXPECT_TRUE(bound2->TryToWork());
  EXPECT_FALSE(bound2->TryToWork());
  bound1->WorkComplete();
  EXPECT_FALSE(bound2->TryToWork());
  EXPECT_TRUE(bound1->TryToWork());
}

}  // namespace

}  // namespace net_instaweb
