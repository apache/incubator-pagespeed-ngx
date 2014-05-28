// Copyright 2010 and onwards Google Inc.
// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test the simple statistics implementation.

#include "pagespeed/kernel/util/simple_stats.h"

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

class SimpleStatsTest : public testing::Test {
 public:
  SimpleStatsTest()
      : thread_system_(Platform::CreateThreadSystem()),
        stats_(thread_system_.get()) {
  }

 protected:
  scoped_ptr<ThreadSystem> thread_system_;
  SimpleStats stats_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleStatsTest);
};

TEST_F(SimpleStatsTest, TestSimpleUpDownCounters) {
  UpDownCounter* c0 = stats_.AddUpDownCounter("c0");
  UpDownCounter* c1 = stats_.AddUpDownCounter("c1");
  UpDownCounter* c2 = stats_.AddUpDownCounter("c2");
  EXPECT_EQ(c0, stats_.FindUpDownCounter("c0"));
  EXPECT_EQ(c1, stats_.AddUpDownCounter("c1"));
  EXPECT_TRUE(stats_.FindUpDownCounter("not_defined") == NULL);
  c0->Set(0);
  c1->Set(1);
  c2->Set(2);
  EXPECT_EQ(0, c0->Get());
  EXPECT_EQ(1, c1->Get());
  EXPECT_EQ(2, c2->Get());
}

TEST_F(SimpleStatsTest, TestSimpleVariables) {
  Variable* c0 = stats_.AddVariable("c0");
  Variable* c1 = stats_.AddVariable("c1");
  Variable* c2 = stats_.AddVariable("c2");
  EXPECT_EQ(c0, stats_.FindVariable("c0"));
  EXPECT_EQ(c1, stats_.AddVariable("c1"));
  EXPECT_TRUE(stats_.FindVariable("not_defined") == NULL);
  c0->Add(0);
  c1->Add(1);
  c2->Add(2);
  EXPECT_EQ(0, c0->Get());
  EXPECT_EQ(1, c1->Get());
  EXPECT_EQ(2, c2->Get());
}

TEST_F(SimpleStatsTest, TestTimedVariable) {
  TimedVariable* tv = stats_.AddTimedVariable("name", "group");
  tv->IncBy(1);
  EXPECT_EQ(1, tv->Get(TimedVariable::START));
}

TEST_F(SimpleStatsTest, TestSetReturningPrevious) {
  UpDownCounter* var = stats_.AddUpDownCounter("c0");
  EXPECT_EQ(0, var->SetReturningPreviousValue(5));
  EXPECT_EQ(5, var->SetReturningPreviousValue(-3));
  EXPECT_EQ(-3, var->SetReturningPreviousValue(10));
  EXPECT_EQ(10, var->Get());
}

}  // namespace net_instaweb
