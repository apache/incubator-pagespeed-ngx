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
//
// Author: jmarantz@google.com (Joshua Marantz)
//         lsong@google.com (Libo Song)

#include "net/instaweb/apache/serf_url_async_fetcher.h"

#include <unistd.h>
#include <cstddef>
#include <cstdlib>
#include <vector>

#include "apr_pools.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/gzip_inflater.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"

// Default domain to test URL fetches from.  If the default site is
// down, the tests can be directed to a backup host by setting the
// environment variable FETCH_TEST_DOMAIN.  Note that this relies on
// 'mod_pagespeed_examples/' and 'do_not_modify/' being available
// relative to the domain, by copying them into /var/www from
// MOD_PAGESPEED_SVN_PATH/src/install.
const char kFetchTestDomain[] = "//modpagespeed.com";

namespace net_instaweb {

namespace {
const char kProxy[] = "";
const int kMaxMs = 20000;
const int kThreadedPollMs = 200;
const int kWaitTimeoutMs = 5 * 1000;
const int kTimerAdvanceMs = 10;
const int kFetcherTimeoutMs = 5 * 1000;

const int kModpagespeedSite = 0;  // TODO(matterbury): These should be an enum?
const int kGoogleFavicon = 1;
const int kGoogleLogo = 2;
const int kCgiSlowJs = 3;
const int kModpagespeedBeacon = 4;
const int kHttpsGoogleFavicon = 5;
const int kConnectionRefused = 6;

// Note: We do not subclass StringAsyncFetch because we want to lock access
// to done_.
class SerfTestFetch : public AsyncFetch {
 public:
  explicit SerfTestFetch(AbstractMutex* mutex)
      : mutex_(mutex), enable_threaded_(false), success_(false), done_(false) {
  }
  virtual ~SerfTestFetch() {}

  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    content.AppendToString(&buffer_);
    return true;
  }
  virtual bool HandleFlush(MessageHandler* handler) { return true; }
  virtual void HandleHeadersComplete() {}
  virtual void HandleDone(bool success) {
    ScopedMutex lock(mutex_);
    CHECK(!done_);
    success_ = success;
    done_ = true;
  }

  const GoogleString& buffer() const { return buffer_; }
  bool success() const { return success_; }
  bool IsDone() const {
    ScopedMutex lock(mutex_);
    return done_;
  }

  virtual bool EnableThreaded() const {
    return enable_threaded_;
  }
  void set_enable_threaded(bool b) { enable_threaded_ = b; }

 private:
  AbstractMutex* mutex_;
  bool enable_threaded_;

  GoogleString buffer_;
  bool success_;
  bool done_;

  DISALLOW_COPY_AND_ASSIGN(SerfTestFetch);
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
    StringPiece fetch_test_domain(getenv("FETCH_TEST_DOMAIN"));
    if (fetch_test_domain.empty()) {
      fetch_test_domain = kFetchTestDomain;
    }
    apr_pool_create(&pool_, NULL);
    timer_.reset(new MockTimer(MockTimer::kApr_5_2010_ms));
    SerfUrlAsyncFetcher::InitStats(&statistics_);
    thread_system_.reset(ThreadSystem::CreateThreadSystem());
    serf_url_async_fetcher_.reset(
        new SerfUrlAsyncFetcher(kProxy, pool_, thread_system_.get(),
                                &statistics_, timer_.get(), kFetcherTimeoutMs,
                                &message_handler_));
    mutex_.reset(thread_system_->NewMutex());
    AddTestUrl(StrCat("http:", fetch_test_domain,
                      "/mod_pagespeed_example/index.html"),
               "<!doctype html>");
    // Note: We store resources in www.modpagespeed.com/do_not_modify and
    // with content hash so that we can make sure the files don't change
    // from under us and cause our tests to fail.
    AddTestUrl(StrCat("http:", fetch_test_domain, "/do_not_modify/"
                      "favicon.d034f46c06475a27478e98ef5dff965e.ico"),
               GoogleString("\000\000\001\000", 4));
    AddTestUrl(StrCat("http:", fetch_test_domain, "/do_not_modify/"
                      "logo.e80d1c59a673f560785784fb1ac10959.gif"), "GIF");
    AddTestUrl("http://modpagespeed.com/do_not_modify/cgi/slow_js.cgi",
               "alert('hello world');");
    AddTestUrl(StrCat("http:", fetch_test_domain, "/mod_pagespeed_beacon"), "");
    AddTestUrl(StrCat("https:", fetch_test_domain, "/do_not_modify/"
                      "favicon.d034f46c06475a27478e98ef5dff965e.ico"),
               GoogleString());
    AddTestUrl(StrCat("http:", fetch_test_domain, ":1023/refused.jpg"),
               GoogleString());
    prev_done_count = 0;
  }

  virtual void TearDown() {
    // Need to free the fetcher before destroy the pool.
    serf_url_async_fetcher_.reset(NULL);
    timer_.reset(NULL);
    STLDeleteElements(&fetches_);
    apr_pool_destroy(pool_);
  }

  void AddTestUrl(const GoogleString& url,
                  const GoogleString& content_start) {
    urls_.push_back(url);
    content_starts_.push_back(content_start);
    fetches_.push_back(new SerfTestFetch(mutex_.get()));
  }

  void StartFetch(int idx) {
    serf_url_async_fetcher_->Fetch(
        urls_[idx], &message_handler_, fetches_[idx]);
  }

  void StartFetches(size_t first, size_t last, bool enable_threaded) {
    for (size_t idx = first; idx <= last; ++idx) {
      fetches_[idx]->set_enable_threaded(enable_threaded);
      StartFetch(idx);
    }
  }

  int ActiveFetches() {
    return statistics_.GetVariable(SerfStats::kSerfFetchActiveCount)->Get();
  }

  int CountCompletedFetches(size_t first, size_t last) {
    int completed = 0;
    for (size_t idx = first; idx <= last; ++idx) {
      if (fetches_[idx]->IsDone()) {
        ++completed;
      }
    }
    return completed;
  }

  void ValidateFetches(size_t first, size_t last) {
    for (size_t idx = first; idx <= last; ++idx) {
      ASSERT_TRUE(fetches_[idx]->IsDone());

      for (int i = 0; !fetches_[idx]->success() && (i < 10); ++i) {
        // We've started to see some flakiness in this test requesting
        // google.com/favicon, so try, at most 10 times, to re-issue
        // the request and sleep.
        // TODO(sligocki): See if this flakiness goes away now that we
        // changed to a static resource.
        usleep(50 * Timer::kMsUs);
        LOG(ERROR) << "Serf retrying flaky url " << urls_[idx];
        fetches_[idx]->Reset();
        StartFetch(idx);
        WaitTillDone(idx, idx, kMaxMs);
      }
      EXPECT_TRUE(fetches_[idx]->success());

      if (content_starts_[idx].empty()) {
        EXPECT_TRUE(contents(idx).empty());
        EXPECT_EQ(HttpStatus::kNoContent,
                  response_headers(idx)->status_code());
      } else {
        EXPECT_LT(static_cast<size_t>(0), contents(idx).size())
            << urls_[idx];
        EXPECT_EQ(HttpStatus::kOK, response_headers(idx)->status_code())
            << urls_[idx];
      }
      EXPECT_EQ(content_starts_[idx],
                contents(idx).substr(0, content_starts_[idx].size()));
    }
  }

  // Valgrind will not allow the async-fetcher thread to run without a sleep.
  void YieldToThread() {
    usleep(1);
  }

  int WaitTillDone(size_t first, size_t last, int64 delay_ms) {
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
      for (size_t idx = first; idx <= last; ++idx) {
        if (fetches_[idx]->IsDone()) {
          ++done_count;
        }
      }
      if (done_count != prev_done_count) {
        prev_done_count = done_count;
        done = (done_count == (last - first + 1));
      }
      now_ms = timer.NowMs();
    }
    return done_count;
  }

  int TestFetch(size_t first, size_t last) {
    StartFetches(first, last, false);
    timer_->AdvanceMs(kTimerAdvanceMs);
    int done = WaitTillDone(first, last, kMaxMs);
    ValidateFetches(first, last);
    return (done == (last - first + 1));
  }

  // Exercise the Serf code when a connection is refused.
  void ConnectionRefusedTest(bool threaded) {
    StartFetches(kConnectionRefused, kConnectionRefused, threaded);
    timer_->AdvanceMs(kTimerAdvanceMs);
    ASSERT_EQ(WaitTillDone(kConnectionRefused, kConnectionRefused, kMaxMs), 1);
    ASSERT_TRUE(fetches_[kConnectionRefused]->IsDone());
    EXPECT_EQ(HttpStatus::kNotFound,
              response_headers(kConnectionRefused)->status_code());
  }

  // Convenience getters.
  RequestHeaders* request_headers(int idx) {
    return fetches_[idx]->request_headers();
  }
  ResponseHeaders* response_headers(int idx) {
    return fetches_[idx]->response_headers();
  }
  const GoogleString& contents(int idx) { return fetches_[idx]->buffer(); }

  apr_pool_t* pool_;
  std::vector<GoogleString> urls_;
  std::vector<GoogleString> content_starts_;
  std::vector<SerfTestFetch*> fetches_;
  // The fetcher to be tested.
  scoped_ptr<SerfUrlAsyncFetcher> serf_url_async_fetcher_;
  scoped_ptr<MockTimer> timer_;
  SimpleStats statistics_;  // TODO(jmarantz): make this thread-safe
  MockMessageHandler message_handler_;
  size_t prev_done_count;
  scoped_ptr<AbstractMutex> mutex_;
  scoped_ptr<ThreadSystem> thread_system_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SerfUrlAsyncFetcherTest);
};

TEST_F(SerfUrlAsyncFetcherTest, FetchOneURL) {
  EXPECT_TRUE(TestFetch(kModpagespeedSite, kModpagespeedSite));
  EXPECT_FALSE(response_headers(kModpagespeedSite)->IsGzipped());
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

// Tests that when the fetcher requests gzipped data it gets it.  Note
// that the callback is delivered content that must be explicitly unzipped.
TEST_F(SerfUrlAsyncFetcherTest, FetchOneURLGzipped) {
  request_headers(kModpagespeedSite)->Add(HttpAttributes::kAcceptEncoding,
                                          HttpAttributes::kGzip);
  StartFetches(kModpagespeedSite, kModpagespeedSite, false);
  EXPECT_EQ(1, ActiveFetches());
  ASSERT_EQ(1, WaitTillDone(kModpagespeedSite, kModpagespeedSite, kMaxMs));
  ASSERT_TRUE(fetches_[kModpagespeedSite]->IsDone());
  EXPECT_LT(static_cast<size_t>(0), contents(kModpagespeedSite).size());
  EXPECT_EQ(200, response_headers(kModpagespeedSite)->status_code());
  ASSERT_TRUE(response_headers(kModpagespeedSite)->IsGzipped());

  GzipInflater inflater(GzipInflater::kGzip);
  ASSERT_TRUE(inflater.Init());
  ASSERT_TRUE(inflater.SetInput(contents(kModpagespeedSite).data(),
                                contents(kModpagespeedSite).size()));
  ASSERT_TRUE(inflater.HasUnconsumedInput());
  int size = content_starts_[kModpagespeedSite].size();
  scoped_array<char> buf(new char[size]);
  ASSERT_EQ(size, inflater.InflateBytes(buf.get(), size));
  EXPECT_EQ(content_starts_[kModpagespeedSite], GoogleString(buf.get(), size));
  EXPECT_EQ(0, ActiveFetches());
}

// In this variant, we do not add accept-encoding gzip, but we *do*
// enable the fetcher to transparently add gzipped content.  In
// mod_pagespeed this is an off-by-default option for the site owner
// because local loopback fetches might be more efficient without
// gzip.
TEST_F(SerfUrlAsyncFetcherTest, FetchOneURLWithGzip) {
  serf_url_async_fetcher_->set_fetch_with_gzip(true);
  EXPECT_TRUE(TestFetch(kModpagespeedSite, kModpagespeedSite));
  EXPECT_FALSE(response_headers(kModpagespeedSite)->IsGzipped());
  int request_count =
      statistics_.GetVariable(SerfStats::kSerfFetchRequestCount)->Get();
  EXPECT_EQ(1, request_count);
  int bytes_count =
      statistics_.GetVariable(SerfStats::kSerfFetchByteCount)->Get();
  // Since we've asked for gzipped content, we expect between 2k and 5k.
  // This might have to be regolded if modpagespeed.com site changes.
  //
  // As of Dec 27, 2011, we have:
  //   wget -q -O - --header='Accept-Encoding:gzip'
  //       http://www.modpagespeed.com/|wc -c              --> 13747
  //   wget -q -O - http://www.modpagespeed.com/|wc -c     --> 2232
  EXPECT_LT(2000, bytes_count);
  EXPECT_GT(5000, bytes_count);
  int time_duration =
      statistics_.GetVariable(SerfStats::kSerfFetchTimeDurationMs)->Get();
  EXPECT_EQ(kTimerAdvanceMs, time_duration);
}

TEST_F(SerfUrlAsyncFetcherTest, FetchTwoURLs) {
  EXPECT_TRUE(TestFetch(kGoogleFavicon, kGoogleLogo));
  int request_count =
      statistics_.GetVariable(SerfStats::kSerfFetchRequestCount)->Get();
  EXPECT_EQ(2, request_count);
  int bytes_count =
      statistics_.GetVariable(SerfStats::kSerfFetchByteCount)->Get();
  // Maybe also need a rough number here. We will break if google's icon or logo
  // changes.
  //
  // TODO(jmarantz): switch to referencing some fixed-size resources on
  // modpagespeed.com so we are not sensitive to favicon changes.
  EXPECT_EQ(13988, bytes_count);
  int time_duration =
      statistics_.GetVariable(SerfStats::kSerfFetchTimeDurationMs)->Get();
  EXPECT_EQ(2 * kTimerAdvanceMs, time_duration);
  EXPECT_EQ(0, ActiveFetches());
}

TEST_F(SerfUrlAsyncFetcherTest, TestCancelThreeThreaded) {
  StartFetches(kModpagespeedSite, kGoogleLogo, true);
}

TEST_F(SerfUrlAsyncFetcherTest, TestCancelOneThreadedTwoSync) {
  StartFetches(kModpagespeedSite, kModpagespeedSite, true);
  StartFetches(kGoogleFavicon, kGoogleLogo, false);
}

TEST_F(SerfUrlAsyncFetcherTest, TestCancelTwoThreadedOneSync) {
  StartFetches(kModpagespeedSite, kModpagespeedSite, false),
  StartFetches(kGoogleFavicon, kGoogleLogo, true);
}

TEST_F(SerfUrlAsyncFetcherTest, TestWaitThreeThreaded) {
  StartFetches(kModpagespeedSite, kGoogleLogo, true);
  serf_url_async_fetcher_->WaitForActiveFetches(
      kWaitTimeoutMs, &message_handler_,
      SerfUrlAsyncFetcher::kThreadedOnly);
  EXPECT_EQ(0, ActiveFetches());
}

TEST_F(SerfUrlAsyncFetcherTest, TestThreeThreadedAsync) {
  StartFetches(kModpagespeedSite, kModpagespeedSite, true);
  serf_url_async_fetcher_->WaitForActiveFetches(
      10 /* milliseconds */, &message_handler_,
      SerfUrlAsyncFetcher::kThreadedOnly);
  StartFetches(kGoogleFavicon, kGoogleLogo, true);

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
  for (int i = kModpagespeedSite;
       (completed <= kGoogleLogo) && (i < kMaxIters);
       ++i) {
    usleep(kPollTimeUs);
    completed = CountCompletedFetches(kModpagespeedSite, kGoogleLogo);
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
  ValidateFetches(kModpagespeedSite, kGoogleLogo);
  EXPECT_EQ(0, ActiveFetches());
}

TEST_F(SerfUrlAsyncFetcherTest, TestWaitOneThreadedTwoSync) {
  StartFetches(kModpagespeedSite, kModpagespeedSite, true);
  StartFetches(kGoogleFavicon, kGoogleLogo, false);
  serf_url_async_fetcher_->WaitForActiveFetches(
      kWaitTimeoutMs, &message_handler_,
      SerfUrlAsyncFetcher::kThreadedAndMainline);
  EXPECT_EQ(0, ActiveFetches());
}

TEST_F(SerfUrlAsyncFetcherTest, TestWaitTwoThreadedOneSync) {
  StartFetches(kModpagespeedSite, kModpagespeedSite, false),
  StartFetches(kGoogleFavicon, kGoogleLogo, true);
  serf_url_async_fetcher_->WaitForActiveFetches(
      kWaitTimeoutMs, &message_handler_,
      SerfUrlAsyncFetcher::kThreadedAndMainline);
  EXPECT_EQ(0, ActiveFetches());
}

TEST_F(SerfUrlAsyncFetcherTest, TestThreeThreaded) {
  StartFetches(kModpagespeedSite, kGoogleLogo, true);
  int done = 0;
  done = WaitTillDone(kModpagespeedSite, kGoogleLogo, kMaxMs);
  EXPECT_EQ(3, done);
  ValidateFetches(kModpagespeedSite, kGoogleLogo);
}

TEST_F(SerfUrlAsyncFetcherTest, TestOneThreadedTwoSync) {
  StartFetches(kModpagespeedSite, kModpagespeedSite, true);
  StartFetches(kGoogleFavicon, kGoogleLogo, false);
  int done = 0;
  done = WaitTillDone(kModpagespeedSite, kGoogleLogo, kMaxMs);
  EXPECT_EQ(3, done);
  ValidateFetches(kModpagespeedSite, kGoogleLogo);
}

TEST_F(SerfUrlAsyncFetcherTest, TestTwoThreadedOneSync) {
  StartFetches(kModpagespeedSite, kModpagespeedSite, false);
  StartFetches(kGoogleFavicon, kGoogleLogo, true);
  int done = 0;
  done = WaitTillDone(kModpagespeedSite, kGoogleLogo, kMaxMs);
  EXPECT_EQ(3, done);
  ValidateFetches(kModpagespeedSite, kGoogleLogo);
}

TEST_F(SerfUrlAsyncFetcherTest, TestTimeout) {
  StartFetches(kCgiSlowJs, kCgiSlowJs, false);
  int timeouts =
      statistics_.GetVariable(SerfStats::kSerfFetchTimeoutCount)->Get();
  ASSERT_EQ(0, WaitTillDone(kCgiSlowJs, kCgiSlowJs, kThreadedPollMs));
  timer_->AdvanceMs(2 * kFetcherTimeoutMs);
  ASSERT_EQ(1, WaitTillDone(kCgiSlowJs, kCgiSlowJs, kThreadedPollMs));
  ASSERT_TRUE(fetches_[kCgiSlowJs]->IsDone());
  EXPECT_FALSE(fetches_[kCgiSlowJs]->success());
  EXPECT_EQ(timeouts + 1,
            statistics_.GetVariable(SerfStats::kSerfFetchTimeoutCount)->Get());
}

TEST_F(SerfUrlAsyncFetcherTest, Test204) {
  TestFetch(kModpagespeedBeacon, kModpagespeedBeacon);
  EXPECT_EQ(HttpStatus::kNoContent,
            response_headers(kModpagespeedBeacon)->status_code());
}

TEST_F(SerfUrlAsyncFetcherTest, TestHttpsFails) {
  StartFetches(kHttpsGoogleFavicon, kHttpsGoogleFavicon, false);
  timer_->AdvanceMs(kTimerAdvanceMs);
  ASSERT_EQ(WaitTillDone(kHttpsGoogleFavicon, kHttpsGoogleFavicon, kMaxMs), 1);
  ASSERT_TRUE(fetches_[kHttpsGoogleFavicon]->IsDone());
  ASSERT_TRUE(content_starts_[kHttpsGoogleFavicon].empty());
  EXPECT_TRUE(contents(kHttpsGoogleFavicon).empty());

  // TODO(jmarantz): Consider using a 500 error code for https support.
  EXPECT_EQ(HttpStatus::kNotFound,
            response_headers(kHttpsGoogleFavicon)->status_code());
}

TEST_F(SerfUrlAsyncFetcherTest, ConnectionRefusedNoDetail) {
  ConnectionRefusedTest(false);
  EXPECT_EQ(1, message_handler_.SeriousMessages());
}

TEST_F(SerfUrlAsyncFetcherTest, ConnectionRefusedWithDetail) {
  serf_url_async_fetcher_->set_list_outstanding_urls_on_error(true);
  ConnectionRefusedTest(false);
  EXPECT_EQ(2, message_handler_.SeriousMessages());
}

TEST_F(SerfUrlAsyncFetcherTest, ThreadedConnectionRefusedNoDetail) {
  ConnectionRefusedTest(true);
  EXPECT_EQ(1, message_handler_.SeriousMessages());
}

TEST_F(SerfUrlAsyncFetcherTest, ThreadedConnectionRefusedWithDetail) {
  serf_url_async_fetcher_->set_list_outstanding_urls_on_error(true);
  ConnectionRefusedTest(true);
  EXPECT_EQ(2, message_handler_.SeriousMessages());
}

// Test that the X-Original-Content-Length header is properly set
// when requested.
TEST_F(SerfUrlAsyncFetcherTest, TestTrackOriginalContentLength) {
  serf_url_async_fetcher_->set_track_original_content_length(true);
  StringAsyncFetch async_fetch;
  serf_url_async_fetcher_->Fetch(urls_[kModpagespeedSite], &message_handler_,
                                 &async_fetch);
  while (!async_fetch.done()) {
    YieldToThread();
    serf_url_async_fetcher_->Poll(kThreadedPollMs);
  }
  const char* ocl_header = async_fetch.response_headers()->Lookup1(
      HttpAttributes::kXOriginalContentLength);
  EXPECT_TRUE(ocl_header != NULL);
  int bytes_count =
      statistics_.GetVariable(SerfStats::kSerfFetchByteCount)->Get();
  int64 ocl_value;
  EXPECT_TRUE(StringToInt64(ocl_header, &ocl_value));
  EXPECT_EQ(bytes_count, ocl_value);
}

}  // namespace net_instaweb
