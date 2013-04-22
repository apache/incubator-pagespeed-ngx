/*
 * Copyright 2013 Google Inc.
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

// Author: gee@google.com (Adam Gee)

#include "testing/base/public/gunit.h"

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/util/public/mock_timer.h"

namespace net_instaweb {

namespace {

TEST(RequestContext_TimingInfo, Noop) {
  RequestContext::TimingInfo timing_info;
  EXPECT_EQ(-1, timing_info.start_ts_ms());
  EXPECT_EQ(0, timing_info.fetch_elapsed_ms());
  EXPECT_EQ(0, timing_info.processing_elapsed_ms());

  MockTimer timer(101);
  timing_info.Init(&timer);
  EXPECT_EQ(-1, timing_info.start_ts_ms());
  EXPECT_EQ(0, timing_info.fetch_elapsed_ms());
  EXPECT_EQ(0, timing_info.processing_elapsed_ms());
}

TEST(RequestContext_TimingInfo, StartTime) {
  RequestContext::TimingInfo timing_info;

  MockTimer timer(101);
  timing_info.Init(&timer);
  timer.AdvanceMs(1);
  timing_info.RequestStarted();
  EXPECT_EQ(102, timing_info.start_ts_ms());
}

TEST(RequestContext_TimingInfo, FetchTiming) {
  RequestContext::TimingInfo timing_info;

  MockTimer timer(100);
  timing_info.Init(&timer);
  timing_info.RequestStarted();

  timer.AdvanceMs(1);
  timing_info.FetchStarted();

  timer.AdvanceMs(2);
  timing_info.FetchHeaderReceived();

  timer.AdvanceMs(3);
  timing_info.FetchFinished();

  EXPECT_EQ(1, timing_info.fetch_start_ms());
  EXPECT_EQ(2, timing_info.fetch_header_ms());
  EXPECT_EQ(5, timing_info.fetch_elapsed_ms());
}

TEST(RequestContext_TimingInfo, ProcessingTime) {
  RequestContext::TimingInfo timing_info;

  MockTimer timer(100);
  timing_info.Init(&timer);
  timing_info.RequestStarted();

  timer.AdvanceMs(1);
  timing_info.FetchStarted();
  timer.AdvanceMs(5);
  timing_info.FetchFinished();
  timer.AdvanceMs(10);

  // RequestFinished not yet called.
  EXPECT_EQ(0, timing_info.processing_elapsed_ms());

  timing_info.RequestFinished();

  EXPECT_EQ(5, timing_info.fetch_elapsed_ms());
  EXPECT_EQ(11, timing_info.processing_elapsed_ms());
  EXPECT_EQ(16, timing_info.GetElapsedMs());
}

TEST(RequestContext_TimingInfo, ProcessingTimeNoFetch) {
  RequestContext::TimingInfo timing_info;

  MockTimer timer(100);
  timing_info.Init(&timer);
  timing_info.RequestStarted();

  timer.AdvanceMs(1);
  // RequestFinished not yet called.
  EXPECT_EQ(0, timing_info.processing_elapsed_ms());

  timing_info.RequestFinished();

  EXPECT_EQ(0, timing_info.fetch_elapsed_ms());
  EXPECT_EQ(1, timing_info.processing_elapsed_ms());
  EXPECT_EQ(1, timing_info.GetElapsedMs());
}

}  // namespace

}  // namespace net_instaweb
