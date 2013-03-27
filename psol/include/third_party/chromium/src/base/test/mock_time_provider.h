// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A helper class used to mock out calls to the static method base::Time::Now.
//
// Example usage:
//
// typedef base::Time(TimeProvider)();
// class StopWatch {
//  public:
//   StopWatch(TimeProvider* time_provider);
//   void Start();
//   base::TimeDelta Stop();
//  private:
//   TimeProvider* time_provider_;
//   ...
// }
//
// Normally, you would instantiate a StopWatch with the real Now function:
//
// StopWatch watch(&base::Time::Now);
//
// But when testing, you want to instantiate it with
// MockTimeProvider::StaticNow, which calls an internally mocked out member.
// This allows you to set expectations on the Now method. For example:
//
// TEST_F(StopWatchTest, BasicTest) {
//   InSequence s;
//   StrictMock<MockTimeProvider> mock_time;
//   EXPECT_CALL(mock_time, Now())
//       .WillOnce(Return(Time::FromDoubleT(4)));
//   EXPECT_CALL(mock_time, Now())
//       .WillOnce(Return(Time::FromDoubleT(10)));
//
//   StopWatch sw(&MockTimeProvider::StaticNow);
//   sw.Start();  // First call to Now.
//   TimeDelta elapsed = sw.stop();  // Second call to Now.
//   ASSERT_EQ(elapsed, TimeDelta::FromSeconds(6));
// }

#ifndef BASE_TEST_MOCK_TIME_PROVIDER_H_
#define BASE_TEST_MOCK_TIME_PROVIDER_H_

#include "base/time.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {

class MockTimeProvider {
 public:
  MockTimeProvider();
  ~MockTimeProvider();

  MOCK_METHOD0(Now, Time());

  static Time StaticNow();

 private:
  static MockTimeProvider* instance_;
  DISALLOW_COPY_AND_ASSIGN(MockTimeProvider);
};

}  // namespace base

#endif  // BASE_TEST_MOCK_TIME_PROVIDER_H_
