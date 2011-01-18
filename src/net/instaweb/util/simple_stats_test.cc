// Copyright 2010 and onwards Google Inc.
// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test the simple statistics implementation.

#include "net/instaweb/util/public/simple_stats.h"

#include "base/basictypes.h"
#include "net/instaweb/util/public/gtest.h"

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

}  // namespace net_instaweb
