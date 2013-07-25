// Copyright 2013 Google Inc.
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
// Author: vchudnov@google.com (Victor Chudnovsky)

#include "pagespeed/kernel/base/countdown_timer.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_timer.h"

namespace net_instaweb {

class CountdownTimerTest : public testing::Test {
 protected:
  CountdownTimerTest() : timer_(0) {}

  MockTimer timer_;
};


TEST_F(CountdownTimerTest, SetTimeNegative) {
  const char* data = "Some fake data";
  CountdownTimer countdown_timer(&timer_, &data, -1);
  EXPECT_TRUE(countdown_timer.HaveTimeLeft());
  EXPECT_EQ(&data, countdown_timer.user_data());
  timer_.AdvanceMs(100);
  EXPECT_TRUE(countdown_timer.HaveTimeLeft());
  EXPECT_EQ(&data, countdown_timer.user_data());

  countdown_timer.Reset(-57);
  EXPECT_TRUE(countdown_timer.HaveTimeLeft());
  EXPECT_EQ(&data, countdown_timer.user_data());
  timer_.SetTimeMs(30);
  EXPECT_TRUE(countdown_timer.HaveTimeLeft());
  EXPECT_EQ(&data, countdown_timer.user_data());
}


TEST_F(CountdownTimerTest, SetTimeZero) {
  const char* data = "Nothing real";
  timer_.SetTimeUs(10);
  CountdownTimer countdown_timer(&timer_, &data, 0);
  EXPECT_FALSE(countdown_timer.HaveTimeLeft());
  EXPECT_EQ(&data, countdown_timer.user_data());
  timer_.AdvanceMs(100);
  EXPECT_FALSE(countdown_timer.HaveTimeLeft());
  EXPECT_EQ(&data, countdown_timer.user_data());

  countdown_timer.Reset(0);
  EXPECT_FALSE(countdown_timer.HaveTimeLeft());
  EXPECT_EQ(&data, countdown_timer.user_data());
}

TEST_F(CountdownTimerTest, SetTimePositive) {
  const char* data = "Something for later";
  CountdownTimer countdown_timer(&timer_, &data, 1);
  EXPECT_TRUE(countdown_timer.HaveTimeLeft());
  EXPECT_EQ(&data, countdown_timer.user_data());
  timer_.AdvanceMs(100);
  EXPECT_FALSE(countdown_timer.HaveTimeLeft());
  EXPECT_EQ(&data, countdown_timer.user_data());

  countdown_timer.Reset(10);
  EXPECT_TRUE(countdown_timer.HaveTimeLeft());
  EXPECT_EQ(&data, countdown_timer.user_data());
  timer_.AdvanceMs(9);
  EXPECT_TRUE(countdown_timer.HaveTimeLeft());
  EXPECT_EQ(&data, countdown_timer.user_data());
  timer_.AdvanceUs(999);
  EXPECT_TRUE(countdown_timer.HaveTimeLeft());
  EXPECT_EQ(&data, countdown_timer.user_data());
  timer_.AdvanceUs(1);
  EXPECT_FALSE(countdown_timer.HaveTimeLeft());
  EXPECT_EQ(&data, countdown_timer.user_data());
}

}  // namespace net_instaweb
