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

#include "pagespeed/kernel/base/mock_timer.h"

#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

class MockTimerTest : public testing::Test {
 protected:
  MockTimerTest() : timer_(new NullMutex, 0) {}
  virtual void SampleCallback(GoogleString* str) {
    *str = str->empty() ? kCallbackCalledOnce : kCallbackCalledTwice;
  }

  MockTimer timer_;
  static const char* kCallbackCalledOnce;
  static const char* kCallbackCalledTwice;
};

const char* MockTimerTest::kCallbackCalledOnce = "CALLED ONCE";
const char* MockTimerTest::kCallbackCalledTwice = "CALLED TWICE";

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


TEST_F(MockTimerTest, SetTimeDeltaWithCallback) {
  GoogleString str = "";
  timer_.SetTimeDeltaUs(2001);
  // We need to specify the template typenames explicitly below
  // because the compiler can't decide whether to use this test class
  // or its base class, MockTimerTest.
  timer_.SetTimeDeltaUsWithCallback(
      43,
      MakeFunction<
        MockTimerTest_SetTimeDeltaWithCallback_Test,
        GoogleString*>(
            this,
            &MockTimerTest_SetTimeDeltaWithCallback_Test::
            SampleCallback, &str));
  timer_.SetTimeDeltaMs(2);
  timer_.SetTimeDeltaUsWithCallback(
      57,
      MakeFunction<
      MockTimerTest_SetTimeDeltaWithCallback_Test,
      GoogleString*>(
          this,
          &MockTimerTest_SetTimeDeltaWithCallback_Test::
          SampleCallback, &str));
  // This callback never gets called but should get canceled:
  timer_.SetTimeDeltaUsWithCallback(
      103,
      MakeFunction<
      MockTimerTest_SetTimeDeltaWithCallback_Test,
      GoogleString*>(
          this,
          &MockTimerTest_SetTimeDeltaWithCallback_Test::
          SampleCallback, &str));

  EXPECT_EQ("", str);
  EXPECT_EQ(2001, timer_.NowUs());
  EXPECT_EQ("", str);
  EXPECT_EQ(2044, timer_.NowUs());
  EXPECT_EQ(kCallbackCalledOnce, str);
  EXPECT_EQ(4044, timer_.NowUs());
  EXPECT_EQ(kCallbackCalledOnce, str);
  EXPECT_EQ(4101, timer_.NowUs());
  EXPECT_EQ(kCallbackCalledTwice, str);
}

}  // namespace net_instaweb
