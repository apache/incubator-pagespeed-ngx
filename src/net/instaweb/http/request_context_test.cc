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
#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

namespace {

TEST(RequestContext_TimingInfo, Noop) {
  RequestContext::TimingInfo timing_info;
  EXPECT_EQ(-1, timing_info.start_ts_ms());
  int64 elapsed;
  ASSERT_FALSE(timing_info.GetProcessingElapsedMs(&elapsed));
  ASSERT_FALSE(timing_info.GetTimeToStartFetchMs(&elapsed));
  ASSERT_FALSE(timing_info.GetFetchElapsedMs(&elapsed));
  ASSERT_FALSE(timing_info.GetTimeToFetchHeaderMs(&elapsed));

  MockTimer timer(101);
  timing_info.Init(&timer);
  ASSERT_FALSE(timing_info.GetProcessingElapsedMs(&elapsed));
  ASSERT_FALSE(timing_info.GetTimeToStartFetchMs(&elapsed));
  ASSERT_FALSE(timing_info.GetFetchElapsedMs(&elapsed));
  ASSERT_FALSE(timing_info.GetTimeToFetchHeaderMs(&elapsed));
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
  int64 elapsed;
  ASSERT_TRUE(timing_info.GetTimeToStartFetchMs(&elapsed));
  EXPECT_EQ(1, elapsed);
  ASSERT_FALSE(timing_info.GetTimeToFetchHeaderMs(&elapsed));
  ASSERT_FALSE(timing_info.GetFetchElapsedMs(&elapsed));

  timer.AdvanceMs(2);
  timing_info.FetchHeaderReceived();
  ASSERT_TRUE(timing_info.GetTimeToFetchHeaderMs(&elapsed));
  EXPECT_EQ(2, elapsed);
  ASSERT_FALSE(timing_info.GetFetchElapsedMs(&elapsed));

  timer.AdvanceMs(3);
  timing_info.FetchFinished();
  ASSERT_TRUE(timing_info.GetFetchElapsedMs(&elapsed));
  EXPECT_EQ(5, elapsed);
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
  int64 elapsed;
  ASSERT_FALSE(timing_info.GetProcessingElapsedMs(&elapsed));

  timing_info.RequestFinished();


  ASSERT_TRUE(timing_info.GetFetchElapsedMs(&elapsed));
  EXPECT_EQ(5, elapsed);
  ASSERT_TRUE(timing_info.GetProcessingElapsedMs(&elapsed));
  EXPECT_EQ(11, elapsed);
  EXPECT_EQ(16, timing_info.GetElapsedMs());
}

TEST(RequestContext_TimingInfo, ProcessingTimeNoFetch) {
  RequestContext::TimingInfo timing_info;

  MockTimer timer(100);
  timing_info.Init(&timer);
  timing_info.RequestStarted();

  timer.AdvanceMs(1);
  // RequestFinished not yet called.
  int64 elapsed;
  ASSERT_FALSE(timing_info.GetProcessingElapsedMs(&elapsed));

  timing_info.RequestFinished();

  // No fetch.
  ASSERT_FALSE(timing_info.GetFetchElapsedMs(&elapsed));

  ASSERT_TRUE(timing_info.GetProcessingElapsedMs(&elapsed));
  EXPECT_EQ(1, elapsed);

  EXPECT_EQ(1, timing_info.GetElapsedMs());
}

TEST(RequestContext_TimingInfo, TimeToStartProcessing) {
  RequestContext::TimingInfo timing_info;

  MockTimer timer(100);
  timing_info.Init(&timer);

  int64 elapsed;
  ASSERT_FALSE(timing_info.GetTimeToStartProcessingMs(&elapsed));

  timer.AdvanceMs(1);
  timing_info.RequestStarted();
  ASSERT_FALSE(timing_info.GetTimeToStartProcessingMs(&elapsed));

  timer.AdvanceMs(2);
  timing_info.ProcessingStarted();
  ASSERT_TRUE(timing_info.GetTimeToStartProcessingMs(&elapsed));
  EXPECT_EQ(2, elapsed);
}

TEST(RequestContext_TimingInfo, PcacheLookup) {
  RequestContext::TimingInfo timing_info;

  MockTimer timer(100);
  timing_info.Init(&timer);

  int64 elapsed;
  ASSERT_FALSE(timing_info.GetTimeToPropertyCacheLookupStartMs(&elapsed));
  ASSERT_FALSE(timing_info.GetTimeToPropertyCacheLookupEndMs(&elapsed));

  timer.AdvanceMs(1);
  timing_info.RequestStarted();
  ASSERT_FALSE(timing_info.GetTimeToPropertyCacheLookupStartMs(&elapsed));
  ASSERT_FALSE(timing_info.GetTimeToPropertyCacheLookupEndMs(&elapsed));

  timer.AdvanceMs(2);
  timing_info.PropertyCacheLookupStarted();
  ASSERT_TRUE(timing_info.GetTimeToPropertyCacheLookupStartMs(&elapsed));
  EXPECT_EQ(2, elapsed);
  ASSERT_FALSE(timing_info.GetTimeToPropertyCacheLookupEndMs(&elapsed));

  timer.AdvanceMs(5);
  timing_info.PropertyCacheLookupFinished();
  ASSERT_TRUE(timing_info.GetTimeToPropertyCacheLookupStartMs(&elapsed));
  EXPECT_EQ(2, elapsed);
  ASSERT_TRUE(timing_info.GetTimeToPropertyCacheLookupEndMs(&elapsed));
  EXPECT_EQ(7, elapsed);
}

TEST(RequestContext_TimingInfo, TimeToStartParse) {
  RequestContext::TimingInfo timing_info;

  MockTimer timer(100);
  timing_info.Init(&timer);

  int64 elapsed;
  ASSERT_FALSE(timing_info.GetTimeToStartParseMs(&elapsed));

  timer.AdvanceMs(1);
  timing_info.RequestStarted();
  ASSERT_FALSE(timing_info.GetTimeToStartParseMs(&elapsed));

  timer.AdvanceMs(2);
  timing_info.ParsingStarted();
  ASSERT_TRUE(timing_info.GetTimeToStartParseMs(&elapsed));
  EXPECT_EQ(2, elapsed);
}

}  // namespace

}  // namespace net_instaweb
