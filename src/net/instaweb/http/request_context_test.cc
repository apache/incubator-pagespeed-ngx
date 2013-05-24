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
#include "net/instaweb/util/public/null_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

namespace {

TEST(RequestContext_TimingInfo, Noop) {
  MockTimer timer(101);
  NullMutex mutex;
  RequestContext::TimingInfo timing_info(&timer, &mutex);
  EXPECT_EQ(timer.NowMs(), timing_info.init_ts_ms());
  EXPECT_EQ(0, timing_info.GetElapsedMs());
  EXPECT_EQ(-1, timing_info.start_ts_ms());
  int64 elapsed;
  ASSERT_FALSE(timing_info.GetProcessingElapsedMs(&elapsed));
  ASSERT_FALSE(timing_info.GetTimeToStartFetchMs(&elapsed));
  ASSERT_FALSE(timing_info.GetFetchLatencyMs(&elapsed));
  ASSERT_FALSE(timing_info.GetFetchHeaderLatencyMs(&elapsed));
}

TEST(RequestContext_TimingInfo, StartTime) {
  MockTimer timer(101);
  NullMutex mutex;
  RequestContext::TimingInfo timing_info(&timer, &mutex);

  timer.AdvanceMs(1);
  timing_info.RequestStarted();
  EXPECT_EQ(102, timing_info.start_ts_ms());
}

TEST(RequestContext_TimingInfo, FetchTiming) {
  MockTimer timer(100);
  NullMutex mutex;
  RequestContext::TimingInfo timing_info(&timer, &mutex);
  timing_info.RequestStarted();

  int64 latency;
  EXPECT_FALSE(timing_info.GetFetchHeaderLatencyMs(&latency));
  EXPECT_FALSE(timing_info.GetFetchLatencyMs(&latency));

  timer.AdvanceMs(1);
  timing_info.FetchStarted();

  int64 elapsed;
  ASSERT_TRUE(timing_info.GetTimeToStartFetchMs(&elapsed));
  EXPECT_EQ(1, elapsed);
  ASSERT_FALSE(timing_info.GetFetchHeaderLatencyMs(&elapsed));
  ASSERT_FALSE(timing_info.GetFetchLatencyMs(&elapsed));

  timer.AdvanceMs(2);
  timing_info.FetchHeaderReceived();
  ASSERT_TRUE(timing_info.GetFetchHeaderLatencyMs(&elapsed));
  EXPECT_EQ(2, elapsed);
  ASSERT_FALSE(timing_info.GetFetchLatencyMs(&elapsed));

  timer.AdvanceMs(3);
  timing_info.FetchFinished();
  ASSERT_TRUE(timing_info.GetFetchLatencyMs(&elapsed));
  EXPECT_EQ(5, elapsed);
}

TEST(RequestContext_TimingInfo, ProcessingTime) {
  MockTimer timer(100);
  NullMutex mutex;
  RequestContext::TimingInfo timing_info(&timer, &mutex);

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


  ASSERT_TRUE(timing_info.GetFetchLatencyMs(&elapsed));
  EXPECT_EQ(5, elapsed);
  ASSERT_TRUE(timing_info.GetProcessingElapsedMs(&elapsed));
  EXPECT_EQ(11, elapsed);
  EXPECT_EQ(16, timing_info.GetElapsedMs());
}

TEST(RequestContext_TimingInfo, ProcessingTimeNoFetch) {
  MockTimer timer(100);
  NullMutex mutex;
  RequestContext::TimingInfo timing_info(&timer, &mutex);

  timing_info.RequestStarted();

  timer.AdvanceMs(1);
  // RequestFinished not yet called.
  int64 elapsed;
  ASSERT_FALSE(timing_info.GetProcessingElapsedMs(&elapsed));

  timing_info.RequestFinished();

  // No fetch.
  ASSERT_FALSE(timing_info.GetFetchLatencyMs(&elapsed));

  ASSERT_TRUE(timing_info.GetProcessingElapsedMs(&elapsed));
  EXPECT_EQ(1, elapsed);
  EXPECT_EQ(1, timing_info.GetElapsedMs());
}

TEST(RequestContext_TimingInfo, TimeToStartProcessing) {
  MockTimer timer(100);
  NullMutex mutex;
  RequestContext::TimingInfo timing_info(&timer, &mutex);

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
  MockTimer timer(100);
  NullMutex mutex;
  RequestContext::TimingInfo timing_info(&timer, &mutex);

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
  MockTimer timer(100);
  NullMutex mutex;
  RequestContext::TimingInfo timing_info(&timer, &mutex);

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

TEST(RequestContext_TimingInfo, CacheLatency) {
  NullMutex mutex;
  RequestContext::TimingInfo timing_info(NULL, &mutex);

  int64 latency_ms;
  ASSERT_FALSE(timing_info.GetHTTPCacheLatencyMs(&latency_ms));
  ASSERT_FALSE(timing_info.GetL2HTTPCacheLatencyMs(&latency_ms));

  timing_info.SetHTTPCacheLatencyMs(1);
  ASSERT_TRUE(timing_info.GetHTTPCacheLatencyMs(&latency_ms));
  EXPECT_EQ(1, latency_ms);
  ASSERT_FALSE(timing_info.GetL2HTTPCacheLatencyMs(&latency_ms));

  timing_info.SetL2HTTPCacheLatencyMs(2);
  ASSERT_TRUE(timing_info.GetHTTPCacheLatencyMs(&latency_ms));
  EXPECT_EQ(1, latency_ms);
  ASSERT_TRUE(timing_info.GetL2HTTPCacheLatencyMs(&latency_ms));
  EXPECT_EQ(2, latency_ms);
}

}  // namespace

}  // namespace net_instaweb
