/*
 * Copyright 2014 Google Inc.
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
// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/http/public/simulated_delay_fetcher.h"

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/mock_scheduler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/platform.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "pagespeed/kernel/base/google_message_handler.h"

namespace net_instaweb {

namespace {

const char kConfigPath[] = "hosts.txt";
const char kLogPath[] = "request_log.txt";

const char kHostA[] = "foo.com";
const int  kDelayMsA = 200;
const char kHostB[] = "bar.com";
const int  kDelayMsB = 100;

class SimulatedDelayFetcherTest : public ::testing::Test {
 protected:
  SimulatedDelayFetcherTest()
      : thread_system_(Platform::CreateThreadSystem()),
        timer_(thread_system_->NewMutex(), MockTimer::kApr_5_2010_ms),
        scheduler_(thread_system_.get(), &timer_),
        file_system_(thread_system_.get(), &timer_) {
    // Write out configuration.
    GoogleString config;
    StrAppend(&config, kHostA, "= ", IntegerToString(kDelayMsA), ";\n");
    StrAppend(&config, kHostB, "= ", IntegerToString(kDelayMsB), ";\n");
    file_system_.WriteFile(kConfigPath, config, &handler_);

    fetcher_.reset(
        new SimulatedDelayFetcher(thread_system_.get(), &timer_, &scheduler_,
                                  &handler_, &file_system_, kConfigPath,
                                  kLogPath, 2 /* flush after 2 requests */));
  }

  virtual ~SimulatedDelayFetcherTest() {}

  scoped_ptr<ThreadSystem> thread_system_;
  GoogleMessageHandler handler_;
  MockTimer timer_;
  MockScheduler scheduler_;
  MemFileSystem file_system_;
  scoped_ptr<SimulatedDelayFetcher> fetcher_;
};

TEST_F(SimulatedDelayFetcherTest, BasicOperation) {
  GoogleString result_a;
  StringAsyncFetch fetch_a(
      RequestContext::NewTestRequestContext(thread_system_.get()),
      &result_a);

  GoogleString result_b;
  StringAsyncFetch fetch_b(
      RequestContext::NewTestRequestContext(thread_system_.get()),
      &result_b);

  fetcher_->Fetch(StrCat("http://", kHostA), &handler_, &fetch_a);

  GoogleString log1;
  file_system_.ReadFile(kLogPath, &log1, &handler_);
  // Nothing should be in log yet, as flush after 2 requests.
  EXPECT_TRUE(log1.empty()) << log1;

  fetcher_->Fetch(StrCat("http://", kHostB), &handler_, &fetch_b);

  // Now we should have flushed stuff.
  GoogleString log2;
  file_system_.ReadFile(kLogPath, &log2, &handler_);

  EXPECT_EQ(
      "Mon, 05 Apr 2010 18:51:26 GMT http://foo.com\n"
      "Mon, 05 Apr 2010 18:51:26 GMT http://bar.com\n",
      log2);

  // Fetch results aren't returned at first.
  EXPECT_FALSE(fetch_a.done());
  EXPECT_FALSE(fetch_b.done());

  // Fetch B is supposed to fire earlier, since kDelayMsB < kDelayMsA
  CHECK_LT(kDelayMsB, kDelayMsA);

  scheduler_.AdvanceTimeMs(kDelayMsB);
  EXPECT_FALSE(fetch_a.done());
  EXPECT_TRUE(fetch_b.done());
  EXPECT_TRUE(fetch_b.success());
  EXPECT_EQ(SimulatedDelayFetcher::kPayload, result_b);

  // Next fetch A files.
  scheduler_.AdvanceTimeMs(kDelayMsA - kDelayMsB);
  EXPECT_TRUE(fetch_a.done());
  EXPECT_TRUE(fetch_a.success());
  EXPECT_EQ(SimulatedDelayFetcher::kPayload, result_a);
}

}  // namespace

}  // namespace net_instaweb
