// Copyright 2011 Google Inc.
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
// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class MockTimerTest : public testing::Test {
 protected:
  MockTimerTest() : timer_(0) {}

  MockTimer timer_;
};

TEST_F(MockTimerTest, Set) {
  timer_.SetTimeUs(5012);
  EXPECT_EQ(5012, timer_.NowUs());
  EXPECT_EQ(5, timer_.NowMs());
  timer_.SetTimeMs(6);
  EXPECT_EQ(6000, timer_.NowUs());
  EXPECT_EQ(6, timer_.NowMs());
}

TEST_F(MockTimerTest, Advance) {
  timer_.AdvanceUs(1001);
  EXPECT_EQ(1001, timer_.NowUs());
  EXPECT_EQ(1, timer_.NowMs());
  timer_.AdvanceMs(6);
  EXPECT_EQ(7001, timer_.NowUs());
  EXPECT_EQ(7, timer_.NowMs());
}

TEST_F(MockTimerTest, SetTimeDelta) {
  timer_.SetTimeDeltaUs(2001);
  timer_.SetTimeDeltaUs(43);
  timer_.SetTimeDeltaMs(2);
  timer_.SetTimeDeltaUs(57);
  EXPECT_EQ(2001, timer_.NowUs());
  EXPECT_EQ(2044, timer_.NowUs());
  EXPECT_EQ(4044, timer_.NowUs());
  EXPECT_EQ(4101, timer_.NowUs());
}

}  // namespace net_instaweb
