// Copyright 2010 Google Inc.
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

#include "net/instaweb/apache/serf_url_async_fetcher.h"

#include <algorithm>
#include <string>
#include <vector>
#include "apr_atomic.h"
#include "apr_pools.h"
#include "apr_strings.h"
#include "apr_version.h"
#include "net/instaweb/util/public/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "net/instaweb/apache/apr_file_system.h"
#include "net/instaweb/apache/apr_mutex.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gzip_inflater.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/serf/src/serf.h"

namespace net_instaweb {

namespace {
const char kProxy[] = "";
const int kMaxMs = 20000;
const int kThreadedPollMs = 200;
const int kWaitTimeoutMs = 5 * 1000;
const int kTimerAdvanceMs = 10;
const int kFetcherTimeoutMs = 5 * 1000;

class SerfTestCallback : public UrlAsyncFetcher::Callback {
 public:
  explicit SerfTestCallback(AprMutex* mutex, const std::string& url)
      : done_(false),
        mutex_(mutex),
        url_(url),
        enable_threaded_(false),
        success_(false) {
  }
  virtual ~SerfTestCallback() {}
  virtual void Done(bool success)  {
    ScopedMutex lock(mutex_);
    CHECK(!done_);
    done_ = true;
    success_ = success;
  }
  bool IsDone() const {
    ScopedMutex lock(mutex_);
    return done_;
  }
  virtual bool EnableThreaded() const {
    return enable_threaded_;
  }
  void set_enable_threaded(bool b) { enable_threaded_ = b; }
  bool success() const { return success_; }
 private:
  bool done_;
  AprMutex* mutex_;
  std::string url_;
  bool enable_threaded_;
  bool success_;

  DISALLOW_COPY_AND_ASSIGN(SerfTestCallback);
};

}  // namespace

class SerfUrlAsyncFetcherTest: public ::testing::Test {
 public:
  static void SetUpTestCase() {
    apr_initialize();
    atexit(apr_terminate);
  }

 protected:
  SerfUrlAsyncFetcherTest() { }

  virtual void SetUp() {
    apr_pool_create(&pool_, NULL);
    timer_.reset(new MockTimer(MockTimer::kApr_5_2010_ms));
    SerfUrlAsyncFetcher::Initialize(&statistics_);
    serf_url_async_fetcher_.reset(
        new SerfUrlAsyncFetcher(kProxy, pool_, &statistics_,
                                timer_.get(), kFetcherTimeoutMs));
    mutex_ = new AprMutex(pool_);
    AddTestUrl("http://www.modpagespeed.com/", "<!doctype html>");
    AddTestUrl("http://www.google.com/favicon.ico",
               std::string("\000\000\001\000", 4));
    AddTestUrl("http://www.google.com/intl/en_ALL/images/logo.gif", "GIF");
    AddTestUrl("http://stevesouders.com/bin/resource.cgi?type=js&sleep=10",
               "var");
    prev_done_count = 0;
  }

  virtual void TearDown() {
    // Need to free the fetcher before destroy the pool.
    serf_url_async_fetcher_.reset(NULL);
    timer_.reset(NULL);
    STLDeleteElements(&request_headers_);
    STLDeleteElements(&response_headers_);
    STLDeleteElements(&contents_);
    STLDeleteElements(&writers_);
    STLDeleteElements(&callbacks_);
    delete mutex_;
    apr_pool_destroy(pool_);
  }

  void AddTestUrl(const std::string& url,
                  const std::string& content_start) {
    urls_.push_back(url);
    content_starts_.push_back(content_start);
    request_headers_.push_back(new RequestHeaders);
    response_headers_.push_back(new ResponseHeaders);
    contents_.push_back(new std::string);
    writers_.push_back(new StringWriter(contents_.back()));
    callbacks_.push_back(new SerfTestCallback(mutex_, url));
  }

  void StartFetches(size_t begin, size_t end, bool enable_threaded) {
    for (size_t idx = begin; idx < end; ++idx) {
      SerfTestCallback* callback = callbacks_[idx];
      callback->set_enable_threaded(enable_threaded);
      serf_url_async_fetcher_->StreamingFetch(
          urls_[idx], *request_headers_[idx], response_headers_[idx],
          writers_[idx], &message_handler_, callback);
    }
  }

  int ActiveFetches() {
    return statistics_.GetVariable(SerfStats::kSerfFetchActiveCount)
        ->Get();
  }

  int CountCompletedFetches(size_t begin, size_t end) {
    int completed = 0;
    for (size_t idx = begin; idx < end; ++idx) {
      if (callbacks_[idx]->IsDone()) {
        ++completed;
      }
    }
    return completed;
  }

  void ValidateFetches(size_t begin, size_t end) {
    for (size_t idx = begin; idx < end; ++idx) {
      ASSERT_TRUE(callbacks_[idx]->IsDone());
      EXPECT_LT(static_cast<size_t>(0), contents_[idx]->size());
      EXPECT_EQ(200, response_headers_[idx]->status_code());
      EXPECT_EQ(content_starts_[idx],
                contents_[idx]->substr(0, content_starts_[idx].size()));
    }
  }

  // Valgrind will not allow the async-fetcher thread to run without a sleep.
  void YieldToThread() {
    usleep(1);
  }

  int WaitTillDone(size_t begin, size_t end, int64 delay_ms) {
    AprTimer timer;
    bool done = false;
    int64 now_ms = timer.NowMs();
    int64 end_ms = now_ms + delay_ms;
    size_t done_count = 0;
    while (!done && (now_ms < end_ms)) {
      int64 to_wait_ms = end_ms - now_ms;
      if (to_wait_ms > kThreadedPollMs) {
        to_wait_ms = kThreadedPollMs;
      }
      YieldToThread();
      serf_url_async_fetcher_->Poll(to_wait_ms);
      done_count = 0;
      for (size_t idx = begin; idx < end; ++idx) {
        if (callbacks_[idx]->IsDone()) {
          ++done_count;
        }
      }
      if (done_count != prev_done_count) {
        prev_done_count = done_count;
        done = (done_count == (end - begin));
      }
      now_ms = timer.NowMs();
    }
    return done_count;
  }

  int TestFetch(size_t begin, size_t end) {
    StartFetches(begin, end, false);
    timer_->AdvanceMs(kTimerAdvanceMs);
    int done = WaitTillDone(begin, end, kMaxMs);
    ValidateFetches(begin, end);
    return (done == (end - begin));
  }

  apr_pool_t* pool_;
  std::vector<std::string> urls_;
  std::vector<std::string> content_starts_;
  std::vector<RequestHeaders*> request_headers_;
  std::vector<ResponseHeaders*> response_headers_;
  std::vector<std::string*> contents_;
  std::vector<StringWriter*> writers_;
  std::vector<SerfTestCallback*> callbacks_;
  // The fetcher to be tested.
  scoped_ptr<SerfUrlAsyncFetcher> serf_url_async_fetcher_;
  scoped_ptr<MockTimer> timer_;
  SimpleStats statistics_;  // TODO(jmarantz): make this thread-safe
  GoogleMessageHandler message_handler_;
  size_t prev_done_count;
  AprMutex* mutex_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SerfUrlAsyncFetcherTest);
};

TEST_F(SerfUrlAsyncFetcherTest, FetchOneURL) {
  EXPECT_TRUE(TestFetch(0, 1));
  EXPECT_FALSE(response_headers_[0]->IsGzipped());
  int request_count =
      statistics_.GetVariable(SerfStats::kSerfFetchRequestCount)->Get();
  EXPECT_EQ(1, request_count);
  int bytes_count =
      statistics_.GetVariable(SerfStats::kSerfFetchByteCount)->Get();
  // We don't care about the exact size, which can change, just that response
  // is non-trivial.
  EXPECT_LT(7500, bytes_count);
  int time_duration =
      statistics_.GetVariable(SerfStats::kSerfFetchTimeDurationMs)->Get();
  EXPECT_EQ(kTimerAdvanceMs, time_duration);
}

TEST_F(SerfUrlAsyncFetcherTest, FetchOneURLGzipped) {
  request_headers_[0]->Add(HttpAttributes::kAcceptEncoding,
                           HttpAttributes::kGzip);
  StartFetches(0, 1, false);
  EXPECT_EQ(1, ActiveFetches());
  ASSERT_EQ(1, WaitTillDone(0, 1, kMaxMs));
  ASSERT_TRUE(callbacks_[0]->IsDone());
  EXPECT_LT(static_cast<size_t>(0), contents_[0]->size());
  EXPECT_EQ(200, response_headers_[0]->status_code());
  ASSERT_TRUE(response_headers_[0]->IsGzipped());

  GzipInflater inflater(GzipInflater::kGzip);
  ASSERT_TRUE(inflater.Init());
  ASSERT_TRUE(inflater.SetInput(contents_[0]->data(), contents_[0]->size()));
  ASSERT_TRUE(inflater.HasUnconsumedInput());
  int size = content_starts_[0].size();
  scoped_array<char> buf(new char[size]);
  ASSERT_EQ(size, inflater.InflateBytes(buf.get(), size));
  EXPECT_EQ(content_starts_[0], GoogleString(buf.get(), size));
  EXPECT_EQ(0, ActiveFetches());
}

TEST_F(SerfUrlAsyncFetcherTest, FetchTwoURLs) {
  EXPECT_TRUE(TestFetch(1, 3));
  int request_count =
      statistics_.GetVariable(SerfStats::kSerfFetchRequestCount)->Get();
  EXPECT_EQ(2, request_count);
  int bytes_count =
      statistics_.GetVariable(SerfStats::kSerfFetchByteCount)->Get();
  // Maybe also need a rough number here. We will break if google's icon or logo
  // changes.
  EXPECT_EQ(9708, bytes_count);
  int time_duration =
      statistics_.GetVariable(SerfStats::kSerfFetchTimeDurationMs)->Get();
  EXPECT_EQ(2 * kTimerAdvanceMs, time_duration);
  EXPECT_EQ(0, ActiveFetches());
}

TEST_F(SerfUrlAsyncFetcherTest, TestCancelThreeThreaded) {
  StartFetches(0, 3, true);
}

TEST_F(SerfUrlAsyncFetcherTest, TestCancelOneThreadedTwoSync) {
  StartFetches(0, 1, true);
  StartFetches(1, 3, false);
}

TEST_F(SerfUrlAsyncFetcherTest, TestCancelTwoThreadedOneSync) {
  StartFetches(0, 1, false),
  StartFetches(1, 3, true);
}

TEST_F(SerfUrlAsyncFetcherTest, TestWaitThreeThreaded) {
  StartFetches(0, 3, true);
  serf_url_async_fetcher_->WaitForActiveFetches(
      kWaitTimeoutMs, &message_handler_,
      SerfUrlAsyncFetcher::kThreadedOnly);
  EXPECT_EQ(0, ActiveFetches());
}

TEST_F(SerfUrlAsyncFetcherTest, TestThreeThreadedAsync) {
  StartFetches(0, 1, true);
  serf_url_async_fetcher_->WaitForActiveFetches(
      10 /* milliseconds */, &message_handler_,
      SerfUrlAsyncFetcher::kThreadedOnly);
  StartFetches(1, 3, true);

  // In this test case, we are not going to call the explicit threaded
  // wait function, WaitForActiveFetches.  We have initiated async
  // fetches and we are hoping they will complete within a certain amount
  // of time.  If the system is running well then we they will finish
  // within a 100ms or so, so we'll loop in 50ms sleep intervals until
  // we hit a max.  We'll give it 20 seconds before declaring failure.
  const int kMaxSeconds = 20;
  const int kPollTimeUs = 50000;
  const int kPollsPerSecond = 1000000 / kPollTimeUs;
  const int kMaxIters = kMaxSeconds * kPollsPerSecond;
  int completed = 0;
  for (int i = 0; (completed < 3) && (i < kMaxIters); ++i) {
    usleep(kPollTimeUs);
    completed = CountCompletedFetches(0, 3);
  }

  // TODO(jmarantz): I have seen this test fail; then pass when it was
  // run a second time.  Find the flakiness and fix it.
  //    Value of: completed
  //    Actual: 0
  //    Expected: 3
  //
  // In the meantime, if this fails, re-running will help you determine whether
  // this is due to your CL or not.  It's possible this is associated with a
  // recent change to the thread loop in serf_url_async_fetcher.cc to use
  // sleep(1) rather than a mutex to keep from spinning when there is nothing
  // to do.  Maybe a little more than 5 seconds is now needed to complete 3
  // async fetches.
  ASSERT_EQ(3, completed) << "Async fetches times out before completing";
  ValidateFetches(0, 3);
  EXPECT_EQ(0, ActiveFetches());
}

TEST_F(SerfUrlAsyncFetcherTest, TestWaitOneThreadedTwoSync) {
  StartFetches(0, 1, true);
  StartFetches(1, 3, false);
  serf_url_async_fetcher_->WaitForActiveFetches(
      kWaitTimeoutMs, &message_handler_,
      SerfUrlAsyncFetcher::kThreadedAndMainline);
  EXPECT_EQ(0, ActiveFetches());
}

TEST_F(SerfUrlAsyncFetcherTest, TestWaitTwoThreadedOneSync) {
  StartFetches(0, 1, false),
  StartFetches(1, 3, true);
  serf_url_async_fetcher_->WaitForActiveFetches(
      kWaitTimeoutMs, &message_handler_,
      SerfUrlAsyncFetcher::kThreadedAndMainline);
  EXPECT_EQ(0, ActiveFetches());
}

TEST_F(SerfUrlAsyncFetcherTest, TestThreeThreaded) {
  StartFetches(0, 3, true);
  int done = 0;
  done = WaitTillDone(0, 3, kMaxMs);
  EXPECT_EQ(3, done);
  ValidateFetches(0, 3);
}

TEST_F(SerfUrlAsyncFetcherTest, TestOneThreadedTwoSync) {
  StartFetches(0, 1, true);
  StartFetches(1, 3, false);
  int done = 0;
  done = WaitTillDone(0, 3, kMaxMs);
  EXPECT_EQ(3, done);
  ValidateFetches(0, 3);
}

TEST_F(SerfUrlAsyncFetcherTest, TestTwoThreadedOneSync) {
  StartFetches(0, 1, false);
  StartFetches(1, 3, true);
  int done = 0;
  done = WaitTillDone(0, 3, kMaxMs);
  EXPECT_EQ(3, done);
  ValidateFetches(0, 3);
}

TEST_F(SerfUrlAsyncFetcherTest, TestTimeout) {
  StartFetches(3, 4, false);
  int timeouts =
      statistics_.GetVariable(SerfStats::kSerfFetchTimeoutCount)->Get();
  ASSERT_EQ(0, WaitTillDone(3, 4, kThreadedPollMs));
  timer_->AdvanceMs(2 * kFetcherTimeoutMs);
  ASSERT_EQ(1, WaitTillDone(3, 4, kThreadedPollMs));
  ASSERT_TRUE(callbacks_[3]->IsDone());
  EXPECT_FALSE(callbacks_[3]->success());
  EXPECT_EQ(timeouts + 1,
            statistics_.GetVariable(SerfStats::kSerfFetchTimeoutCount)->Get());
}

}  // namespace net_instaweb
