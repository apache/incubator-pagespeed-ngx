// Copyright 2010 and onwards Google Inc.
// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test the simple statistics implementation.

#include "pagespeed/kernel/base/simple_stats.h"

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/statistics.h"

namespace net_instaweb {

class SimpleStatsTest : public testing::Test {
 public:
  SimpleStatsTest() { }

 protected:
  SimpleStats stats_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleStatsTest);
};

TEST_F(SimpleStatsTest, TestSimpleStats) {
  SimpleStats ss;
  Variable* c0 = stats_.AddVariable("c0");
  Variable* c1 = stats_.AddVariable("c1");
  Variable* c2 = stats_.AddVariable("c2");
  EXPECT_EQ(c0, stats_.FindVariable("c0"));
  EXPECT_EQ(c1, stats_.AddVariable("c1"));
  EXPECT_TRUE(stats_.FindVariable("not_defined") == NULL);
  c0->Set(0);
  c1->Set(1);
  c2->Set(2);
  EXPECT_EQ(0, c0->Get());
  EXPECT_EQ(1, c1->Get());
  EXPECT_EQ(2, c2->Get());
}

TEST_F(SimpleStatsTest, TestTimedVariable) {
  SimpleStats ss;
  TimedVariable* tv = stats_.AddTimedVariable("name", "group");
  tv->IncBy(1);
  EXPECT_EQ(1, tv->Get(TimedVariable::START));
}

TEST_F(SimpleStatsTest, TestSetReturningPrevious) {
  SimpleStats ss;
  Variable* var = ss.AddVariable("c0");
  EXPECT_EQ(0, var->SetReturningPreviousValue(5));
  EXPECT_EQ(5, var->SetReturningPreviousValue(-3));
  EXPECT_EQ(-3, var->SetReturningPreviousValue(10));
  EXPECT_EQ(10, var->Get());
}

}  // namespace net_instaweb
