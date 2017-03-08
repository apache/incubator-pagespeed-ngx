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

#include "pagespeed/system/serf_url_async_fetcher.h"

#include <unistd.h>
#include <cstddef>
#include <cstdlib>
#include <vector>

#include "apr_network_io.h"
#include "apr_pools.h"
#include "apr_uri.h"
#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/request_context.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/dynamic_annotations.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/stack_buffer.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/statistics_template.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/util/gzip_inflater.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/kernel/util/simple_stats.h"
#include "pagespeed/system/tcp_server_thread_for_testing.h"

namespace {

// Default domain to test URL fetches from.  If the default site is
// down, the tests can be directed to a backup host by setting the
// environment variable PAGESPEED_TEST_HOST.  Note that this relies on
// 'mod_pagespeed_examples/' and 'do_not_modify/' being available
// relative to the domain, by copying them into /var/www from
// MOD_PAGESPEED_SVN_PATH/src/install.
const char kFetchHost[] = "selfsigned.modpagespeed.com";

// The threaded async fetching tests are a bit flaky and quite slow, especially
// on valgrind.  Ideally that should be fixed but until it becomes a priority,
// do not subject all developers to this tax.
#define SERF_FLAKY_SLOW_THREADING_TESTS 0

}  // namespace

namespace net_instaweb {

namespace {
const int kThreadedPollMs = 200;
const int kFetcherTimeoutMs = 5 * 1000;
const int kFetcherTimeoutValgrindMs = 20 * 1000;

const int kModpagespeedSite = 0;  // TODO(matterbury): These should be an enum?
const int kGoogleFavicon = 1;
const int kGoogleLogo = 2;
const int kCgiSlowJs = 3;
const int kModpagespeedBeacon = 4;
const int kConnectionRefused = 5;
const int kNoContent = 6;
const int kNextTestcaseIndex = 7;  // Should always be last.

// Note: We do not subclass StringAsyncFetch because we want to lock access
// to done_.
class SerfTestFetch : public AsyncFetch {
 public:
  explicit SerfTestFetch(const RequestContextPtr& ctx, AbstractMutex* mutex)
      : AsyncFetch(ctx),
        mutex_(mutex), success_(false), done_(false) {
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
    EXPECT_FALSE(done_);
    success_ = success;
    done_ = true;
  }

  const GoogleString& buffer() const { return buffer_; }
  bool success() const { return success_; }
  bool IsDone() const {
    ScopedMutex lock(mutex_);
    return done_;
  }

  virtual void Reset() {
    ScopedMutex lock(mutex_);
    AsyncFetch::Reset();
    done_ = false;
    success_ = false;
    response_headers()->Clear();
  }

 private:
  AbstractMutex* mutex_;

  GoogleString buffer_;
  bool success_;
  bool done_;

  DISALLOW_COPY_AND_ASSIGN(SerfTestFetch);
};

}  // namespace

class SerfUrlAsyncFetcherTest : public ::testing::Test {
 public:
  static void SetUpTestCase() {
    // Run once, before all SerfUrlAsyncFetcherTest tests.
    apr_initialize();
    atexit(apr_terminate);
  }

 protected:
  SerfUrlAsyncFetcherTest()
      : thread_system_(Platform::CreateThreadSystem()),
        message_handler_(thread_system_->NewMutex()),
        flaky_retries_(0),
        fetcher_timeout_ms_(FetcherTimeoutMs()) {
  }

  virtual void SetUp() {
    SetUpWithProxy("");
  }

  static int64 FetcherTimeoutMs() {
    return RunningOnValgrind() ? kFetcherTimeoutValgrindMs : kFetcherTimeoutMs;
  }

  void SetUpWithProxy(const char* proxy) {
    const char* env_host = getenv("PAGESPEED_TEST_HOST");
    if (env_host != NULL) {
      test_host_ = env_host;
    }
    if (test_host_.empty()) {
      test_host_ = kFetchHost;
    }
    GoogleString fetch_test_domain = StrCat("//", test_host_);
    apr_pool_create(&pool_, NULL);
    timer_.reset(Platform::CreateTimer());
    statistics_.reset(new SimpleStats(thread_system_.get()));
    SerfUrlAsyncFetcher::InitStats(statistics_.get());
    serf_url_async_fetcher_.reset(
        new SerfUrlAsyncFetcher(proxy, pool_, thread_system_.get(),
                                statistics_.get(), timer_.get(),
                                fetcher_timeout_ms_, &message_handler_));
    mutex_.reset(thread_system_->NewMutex());
    AddTestUrl(StrCat("http:", fetch_test_domain,
                      "/mod_pagespeed_example/index.html"),
               "<!doctype html>");
    // Note: We store resources in www.modpagespeed.com/do_not_modify and
    // with content hash so that we can make sure the files don't change
    // from under us and cause our tests to fail.
    GoogleString favicon_domain_and_path = StrCat(
        fetch_test_domain,
        "/do_not_modify/favicon.d034f46c06475a27478e98ef5dff965e.ico");
    static const char kFaviconHead[] = "\000\000\001\001\002\000\020";
    favicon_head_.append(kFaviconHead, STATIC_STRLEN(kFaviconHead));
    https_favicon_url_ = StrCat("https:", favicon_domain_and_path);
    AddTestUrl(StrCat("http:", favicon_domain_and_path), favicon_head_);
    AddTestUrl(StrCat("http:", fetch_test_domain, "/do_not_modify/"
                      "logo.e80d1c59a673f560785784fb1ac10959.gif"), "GIF");
    AddTestUrl(StrCat("http:", fetch_test_domain,
                      "/do_not_modify/cgi/slow_js.cgi"),
               "alert('hello world');");
    AddTestUrl(StrCat("http:", fetch_test_domain,
                      "/mod_pagespeed_beacon?ets=42"), "");
    AddTestUrl(StrCat("http:", fetch_test_domain, ":1023/refused.jpg"), "");
    AddTestUrl(StrCat("http:", fetch_test_domain, "/no_content"), "");

    prev_done_count = 0;

#if SERF_HTTPS_FETCHING
    const char* ssl_cert_dir = getenv("SSL_CERT_DIR");
    if (ssl_cert_dir != NULL) {
      serf_url_async_fetcher_->SetSslCertificatesDir(ssl_cert_dir);
    }
    const char* ssl_cert_file = getenv("SSL_CERT_FILE");
    if (ssl_cert_file != NULL) {
      serf_url_async_fetcher_->SetSslCertificatesFile(ssl_cert_file);
    }
#endif
    // Set initial timestamp so we don't roll-over monitoring stats right after
    // start.
    statistics_->GetUpDownCounter(SerfStats::kSerfFetchLastCheckTimestampMs)
        ->Set(timer_->NowMs());
  }

  virtual void TearDown() {
    // Need to free the fetcher before destroy the pool.
    serf_url_async_fetcher_.reset(NULL);
    timer_.reset(NULL);
    STLDeleteElements(&fetches_);
    if (pool_ != nullptr) {
      apr_pool_destroy(pool_);
      pool_ = nullptr;
    }
  }

  // Adds a new URL & expected response to the url/response structure, returning
  // its index so it can be passed to StartFetch/StartFetches etc.
  int AddTestUrl(const GoogleString& url, const GoogleString& content_start) {
    urls_.push_back(url);
    content_starts_.push_back(content_start);
    int index = fetches_.size();
    fetches_.push_back(
        new SerfTestFetch(
            RequestContext::NewTestRequestContext(thread_system_.get()),
            mutex_.get()));
    return index;
  }

  void StartFetch(int idx) {
    fetches_[idx]->Reset();
    serf_url_async_fetcher_->Fetch(
        urls_[idx], &message_handler_, fetches_[idx]);
  }

  void StartFetches(size_t first, size_t last) {
    for (size_t idx = first; idx <= last; ++idx) {
      StartFetch(idx);
    }
  }

  int ActiveFetches() {
    return statistics_->GetUpDownCounter(
        SerfStats::kSerfFetchActiveCount)->Get();
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

  void FlakyRetry(int idx) {
    for (int i = 0; !fetches_[idx]->success() && (i < 10); ++i) {
      // We've started to see some flakiness in this test requesting
      // google.com/favicon, so try, at most 10 times, to re-issue
      // the request and sleep.
      //
      // Note: this flakiness appears to remain despite using static
      // resources.
      usleep(50 * Timer::kMsUs);
      LOG(ERROR) << "Serf retrying flaky url " << urls_[idx];
      ++flaky_retries_;
      fetches_[idx]->Reset();
      StartFetch(idx);
      WaitTillDone(idx, idx);
    }
  }

  void ValidateFetches(size_t first, size_t last) {
    for (size_t idx = first; idx <= last; ++idx) {
      ASSERT_TRUE(fetches_[idx]->IsDone());
      FlakyRetry(idx);
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
      EXPECT_STREQ(content_starts_[idx],
                   contents(idx).substr(0, content_starts_[idx].size()));
    }
  }

  void ValidateMonitoringStats(int64 expect_success, int64 expect_failure) {
    EXPECT_EQ(expect_success, statistics_->GetVariable(
        SerfStats::kSerfFetchUltimateSuccess)->Get());
    EXPECT_EQ(expect_failure, statistics_->GetVariable(
        SerfStats::kSerfFetchUltimateFailure)->Get());
  }

  // Valgrind will not allow the async-fetcher thread to run without a sleep.
  void YieldToThread() {
    usleep(1);
  }

  int WaitTillDone(size_t first, size_t last) {
    bool done = false;
    size_t done_count = 0;
    while (!done) {
      YieldToThread();
      serf_url_async_fetcher_->Poll(kThreadedPollMs);
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
    }
    return done_count;
  }

  int TestFetch(size_t first, size_t last) {
    StartFetches(first, last);
    int done = WaitTillDone(first, last);
    ValidateFetches(first, last);
    return (done == (last - first + 1));
  }

  // Exercise the Serf code when a connection is refused.
  void ConnectionRefusedTest() {
    StartFetches(kConnectionRefused, kConnectionRefused);
    ASSERT_EQ(WaitTillDone(kConnectionRefused, kConnectionRefused), 1);
    ASSERT_TRUE(fetches_[kConnectionRefused]->IsDone());
    EXPECT_EQ(HttpStatus::kNotFound,
              response_headers(kConnectionRefused)->status_code());
    ValidateMonitoringStats(0, 1);
  }

  // Tests that a range of URLs (established with AddTestUrl) all fail
  // with HTTPS, either because HTTPS is disabled or because of cert issues.
  void TestHttpsFails(int first, int last) {
    int num_fetches = last - first + 1;
    CHECK_LT(0, num_fetches);
    StartFetches(first, last);
    ASSERT_EQ(num_fetches, WaitTillDone(first, last));
    for (int index = first; index <= last; ++index) {
      ASSERT_TRUE(fetches_[index]->IsDone()) << urls_[index];
      ASSERT_TRUE(content_starts_[index].empty()) << urls_[index];
      EXPECT_STREQ("", contents(index)) << urls_[index];
      EXPECT_EQ(HttpStatus::kNotFound, response_headers(index)->status_code())
          << urls_[index];
    }

    // If we have enabled https, we should be counting our cert-failures.
    // Otherwise we shouldn't even be checking.
    if (serf_url_async_fetcher_->SupportsHttps()) {
      EXPECT_EQ(num_fetches, statistics_->GetVariable(
          SerfStats::kSerfFetchCertErrors)->Get());
    } else {
      EXPECT_EQ(0, statistics_->GetVariable(
          SerfStats::kSerfFetchCertErrors)->Get());
    }
  }

  // Tests a single URL fails with HTTPS.
  void TestHttpsFails(const GoogleString& url) {
    int index = AddTestUrl(url, "");
    TestHttpsFails(index, index);
  }

  // Tests that a single HTTPS URL with expected content succeeds.
  void TestHttpsSucceeds(const GoogleString& url,
                         const GoogleString& content_start) {
    int index = AddTestUrl(url, content_start);
    StartFetches(index, index);
    ExpectHttpsSucceeds(index);
  }

  // Verifies that an added & started fetch at index succeeds.
  void ExpectHttpsSucceeds(int index) {
    ASSERT_EQ(1, WaitTillDone(index, index));
    FlakyRetry(index);
    ASSERT_TRUE(fetches_[index]->IsDone());
    ASSERT_FALSE(content_starts_[index].empty());
    EXPECT_FALSE(contents(index).empty());
    EXPECT_EQ(HttpStatus::kOK, response_headers(index)->status_code());
    EXPECT_EQ(0, statistics_->GetVariable(
        SerfStats::kSerfFetchCertErrors)->Get());
    EXPECT_STREQ(content_starts_[index],
                 contents(index).substr(0, content_starts_[index].size()));
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
  GoogleString test_host_;
  std::vector<GoogleString> urls_;
  std::vector<GoogleString> content_starts_;
  std::vector<SerfTestFetch*> fetches_;
  // The fetcher to be tested.
  scoped_ptr<SerfUrlAsyncFetcher> serf_url_async_fetcher_;
  scoped_ptr<Timer> timer_;
  size_t prev_done_count;
  scoped_ptr<AbstractMutex> mutex_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockMessageHandler message_handler_;
  scoped_ptr<SimpleStats> statistics_;
  GoogleString https_favicon_url_;
  GoogleString favicon_head_;
  int64 flaky_retries_;
  int64 fetcher_timeout_ms_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SerfUrlAsyncFetcherTest);
};

TEST_F(SerfUrlAsyncFetcherTest, FetchOneURL) {
  EXPECT_TRUE(TestFetch(kModpagespeedSite, kModpagespeedSite));
  EXPECT_FALSE(response_headers(kModpagespeedSite)->IsGzipped());
  int request_count =
      statistics_->GetVariable(SerfStats::kSerfFetchRequestCount)->Get();
  EXPECT_EQ(1, request_count - flaky_retries_);
  int bytes_count =
      statistics_->GetVariable(SerfStats::kSerfFetchByteCount)->Get();
  // We don't care about the exact size, which can change, just that response
  // is non-trivial.
  EXPECT_LT(7500, bytes_count);
  ValidateMonitoringStats(1, 0);
}

// Tests that when the fetcher requests using a different request method,
// PURGE in this case, it gets the expected response.
TEST_F(SerfUrlAsyncFetcherTest, FetchUsingDifferentRequestMethod) {
  request_headers(kModpagespeedSite)->set_method(RequestHeaders::kPurge);
  StartFetches(kModpagespeedSite, kModpagespeedSite);
  ASSERT_EQ(1, WaitTillDone(kModpagespeedSite, kModpagespeedSite));
  ASSERT_TRUE(fetches_[kModpagespeedSite]->IsDone());
  EXPECT_LT(static_cast<size_t>(0), contents(kModpagespeedSite).size());
  EXPECT_EQ(501,  // PURGE method not implemented in test apache servers.
            response_headers(kModpagespeedSite)->status_code());
  EXPECT_TRUE(
      contents(kModpagespeedSite).find(
          "PURGE to /mod_pagespeed_example/index.html not supported.") !=
      GoogleString::npos);
  ValidateMonitoringStats(0, 1);
}

// Tests that when the fetcher requests gzipped data it gets it.  Note
// that the callback is delivered content that must be explicitly unzipped.
TEST_F(SerfUrlAsyncFetcherTest, FetchOneURLGzipped) {
  request_headers(kModpagespeedSite)->Add(HttpAttributes::kAcceptEncoding,
                                          HttpAttributes::kGzip);
  StartFetches(kModpagespeedSite, kModpagespeedSite);
  ASSERT_EQ(1, WaitTillDone(kModpagespeedSite, kModpagespeedSite));
  ASSERT_TRUE(fetches_[kModpagespeedSite]->IsDone());
  EXPECT_LT(static_cast<size_t>(0), contents(kModpagespeedSite).size());
  EXPECT_EQ(200, response_headers(kModpagespeedSite)->status_code());
  ASSERT_TRUE(response_headers(kModpagespeedSite)->IsGzipped());
  ValidateMonitoringStats(1, 0);

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
      statistics_->GetVariable(SerfStats::kSerfFetchRequestCount)->Get();
  EXPECT_EQ(1, request_count - flaky_retries_);
  int bytes_count =
      statistics_->GetVariable(SerfStats::kSerfFetchByteCount)->Get();
  // Since we've asked for gzipped content, we expect between 2k and 5k.
  // This might have to be regolded if modpagespeed.com site changes.
  //
  // As of Dec 27, 2011, we have:
  //   wget -q -O - --header='Accept-Encoding:gzip'
  //       http://www.modpagespeed.com/|wc -c              --> 13747
  //   wget -q -O - http://www.modpagespeed.com/|wc -c     --> 2232
  EXPECT_LT(2000, bytes_count);
  EXPECT_GT(5000, bytes_count);
  ValidateMonitoringStats(1, 0);
}

TEST_F(SerfUrlAsyncFetcherTest, FetchTwoURLs) {
  EXPECT_TRUE(TestFetch(kGoogleFavicon, kGoogleLogo));
  int request_count =
      statistics_->GetVariable(SerfStats::kSerfFetchRequestCount)->Get();
  EXPECT_EQ(2, request_count - flaky_retries_);
  int bytes_count =
      statistics_->GetVariable(SerfStats::kSerfFetchByteCount)->Get();
  // Maybe also need a rough number here. We will break if google's icon or logo
  // changes.
  //
  // TODO(jmarantz): switch to referencing some fixed-size resources on
  // modpagespeed.com so we are not sensitive to favicon changes.
  EXPECT_EQ(13988, bytes_count);
  EXPECT_EQ(0, ActiveFetches());
  ValidateMonitoringStats(2, 0);
}

TEST_F(SerfUrlAsyncFetcherTest, TestCancelThreeThreaded) {
  StartFetches(kModpagespeedSite, kGoogleLogo);
  // Explicit shutdown, it's OK to call this twice.
  TearDown();

  // Some of the fetches may succeed in the tiny window above, but none should
  // be considered failed due to the quick shutdown cancelling them.
  EXPECT_LE(statistics_->GetVariable(
                SerfStats::kSerfFetchUltimateSuccess)->Get(),
            3);
  EXPECT_EQ(0,
            statistics_->GetVariable(
                SerfStats::kSerfFetchUltimateFailure)->Get());
}

TEST_F(SerfUrlAsyncFetcherTest, TestWaitThreeThreaded) {
  if (RunningOnValgrind()) {
    return;
  }
  StartFetches(kModpagespeedSite, kGoogleLogo);
  serf_url_async_fetcher_->WaitForActiveFetches(
      fetcher_timeout_ms_, &message_handler_,
      SerfUrlAsyncFetcher::kThreadedOnly);
  EXPECT_EQ(0, ActiveFetches());
  ValidateMonitoringStats(3, 0);
}

#if SERF_FLAKY_SLOW_THREADING_TESTS

// Example flake:
// third_party/pagespeed/system/serf_url_async_fetcher_test.cc:495: Failure
// Value of: ActiveFetches()
//   Actual: 1
// Expected: 0
//
// TODO(jmarantz): analyze this flake and fix it.

TEST_F(SerfUrlAsyncFetcherTest, TestThreeThreadedAsync) {
  StartFetches(kModpagespeedSite, kModpagespeedSite);
  serf_url_async_fetcher_->WaitForActiveFetches(
      10 /* milliseconds */, &message_handler_,
      SerfUrlAsyncFetcher::kThreadedOnly);
  StartFetches(kGoogleFavicon, kGoogleLogo);

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
  ValidateMonitoringStats(3, 0);
}
#endif  // SERF_FLAKY_SLOW_THREADING_TESTS

TEST_F(SerfUrlAsyncFetcherTest, TestThreeThreaded) {
  StartFetches(kModpagespeedSite, kGoogleLogo);
  int done = WaitTillDone(kModpagespeedSite, kGoogleLogo);
  EXPECT_EQ(3, done);
  ValidateFetches(kModpagespeedSite, kGoogleLogo);
  ValidateMonitoringStats(3, 0);
}

TEST_F(SerfUrlAsyncFetcherTest, TestTimeout) {
  // Try this up to 10 times.  We expect the fetch to timeout, but it might
  // fail for some other reason instead, such as 'Serf status 111(Connection
  // refused) polling for 1 threaded fetches for 0.05 seconds', so retry a few
  // times till we get the timeout we seek.
  Variable* timeouts =
      statistics_->GetVariable(SerfStats::kSerfFetchTimeoutCount);
  for (int i = 0; i < 10; ++i) {
    statistics_->Clear();
    StartFetches(kCgiSlowJs, kCgiSlowJs);
    int64 start_ms = timer_->NowMs();
    ASSERT_EQ(1, WaitTillDone(kCgiSlowJs, kCgiSlowJs));
    if (timeouts->Get() == 1) {
      int64 elapsed_ms = timer_->NowMs() - start_ms;
      EXPECT_LE(fetcher_timeout_ms_, elapsed_ms);
      ASSERT_TRUE(fetches_[kCgiSlowJs]->IsDone());
      EXPECT_FALSE(fetches_[kCgiSlowJs]->success());

      int time_duration =
          statistics_->GetVariable(SerfStats::kSerfFetchTimeDurationMs)->Get();
      EXPECT_LE(fetcher_timeout_ms_, time_duration);
      break;
    }
  }
}

TEST_F(SerfUrlAsyncFetcherTest, Test204) {
  TestFetch(kNoContent, kNoContent);
  EXPECT_EQ(HttpStatus::kNoContent,
            response_headers(kNoContent)->status_code());
  ValidateMonitoringStats(1, 0);
}

TEST_F(SerfUrlAsyncFetcherTest, TestHttpsFailsByDefault) {
  TestHttpsFails(https_favicon_url_);
  ValidateMonitoringStats(0, 1);
}

#if SERF_HTTPS_FETCHING

TEST_F(SerfUrlAsyncFetcherTest, TestHttpsFailsForSelfSignedCert) {
  serf_url_async_fetcher_->SetHttpsOptions("enable");
  EXPECT_TRUE(serf_url_async_fetcher_->SupportsHttps());
  TestHttpsFails(https_favicon_url_);
  ValidateMonitoringStats(0, 1);
}

TEST_F(SerfUrlAsyncFetcherTest, TestHttpsSucceedsForGoogleCom) {
  serf_url_async_fetcher_->SetHttpsOptions("enable");
  EXPECT_TRUE(serf_url_async_fetcher_->SupportsHttps());
  TestHttpsSucceeds("https://www.google.com/intl/en/about/", "<!DOCTYPE html>");
  ValidateMonitoringStats(1, 0);
}

TEST_F(SerfUrlAsyncFetcherTest, TestHttpsWithExplicitHost) {
  // Make sure if the Host: header is set, we match SNI to it.
  // To do this, we need to have an alternative hostname for
  // our test domain. We cheat a little, and just append a dot, since
  // for a fully-qualified domain DNS will ignore it.
  GoogleUrl original_url(https_favicon_url_);
  GoogleUrl alt_url(StrCat("https://", original_url.Host(), ".",
                           original_url.PathAndLeaf()));

  serf_url_async_fetcher_->SetHttpsOptions("enable,allow_self_signed");
  int index = AddTestUrl(alt_url.Spec().as_string(), favicon_head_);
  request_headers(index)->Add(HttpAttributes::kHost, original_url.Host());
  StartFetches(index, index);
  ExpectHttpsSucceeds(index);
  ValidateMonitoringStats(1, 0);
}

TEST_F(SerfUrlAsyncFetcherTest, TestHttpsFailsWithIncorrectHost) {
  serf_url_async_fetcher_->SetHttpsOptions("enable");
  int index = AddTestUrl("https://www.google.com", "");
  request_headers(index)->Add(HttpAttributes::kHost, "www.example.com");
  TestHttpsFails(index, index);
  ValidateMonitoringStats(0, 1);
}

TEST_F(SerfUrlAsyncFetcherTest, TestHttpsWithExplicitHostPort) {
  // Similar to above, but just throw in an explicit port number;
  // if it doesn't get properly dropped from the SNI Apache will
  // 400 it.
  serf_url_async_fetcher_->SetHttpsOptions("enable,allow_self_signed");
  GoogleUrl original_url(https_favicon_url_);
  GoogleString with_port = StrCat(original_url.Origin(), ":443",
                                  original_url.PathAndLeaf());
  int index = AddTestUrl(with_port, favicon_head_);
  request_headers(index)->Add(HttpAttributes::kHost,
                              StrCat(original_url.Host(), ":443"));
  StartFetches(index, index);
  ExpectHttpsSucceeds(index);
  ValidateMonitoringStats(1, 0);
}

TEST_F(SerfUrlAsyncFetcherTest, TestHttpsFailsForGoogleComWithBogusCertDir) {
  serf_url_async_fetcher_->SetHttpsOptions("enable");
  serf_url_async_fetcher_->SetSslCertificatesDir(GTestTempDir());
  serf_url_async_fetcher_->SetSslCertificatesFile("");
  TestHttpsFails("https://www.google.com/intl/en/about/");
  ValidateMonitoringStats(0, 1);
}

TEST_F(SerfUrlAsyncFetcherTest, TestHttpsSucceedsWhenEnabled) {
  serf_url_async_fetcher_->SetHttpsOptions("enable,allow_self_signed");
  EXPECT_TRUE(serf_url_async_fetcher_->SupportsHttps());
  TestHttpsSucceeds(https_favicon_url_, favicon_head_);
  ValidateMonitoringStats(1, 0);
}
#else

TEST_F(SerfUrlAsyncFetcherTest, TestHttpsFailsEvenWhenEnabled) {
  serf_url_async_fetcher_->SetHttpsOptions("enable");  // ignored
  EXPECT_FALSE(serf_url_async_fetcher_->SupportsHttps());
  TestHttpsFails(https_favicon_url_);
  ValidateMonitoringStats(0, 1);
}

#endif

// TODO(jkarlin): Fix the race in WithDetail functions below.
// list_outstanding_urls_on_error will only log an error if there are active
// fetches in poll. If we get a connection refused faster than we get to the
// poll (say by connecting to localhost), then there won't be any active fetches
// by the time we poll, and won't print the message.

// TODO(jkarlin): Fix these tests for Virtualbox release testing.

TEST_F(SerfUrlAsyncFetcherTest, ThreadedConnectionRefusedNoDetail) {
  StringPiece vb_test(getenv("VIRTUALBOX_TEST"));
  if (!vb_test.empty()) {
    return;
  }
  ConnectionRefusedTest();

  // Depending on the timing of the failure, we may get 1 or two serious
  // messages.
  EXPECT_LE(1, message_handler_.SeriousMessages());
  EXPECT_GE(2, message_handler_.SeriousMessages());
}

TEST_F(SerfUrlAsyncFetcherTest, ThreadedConnectionRefusedWithDetail) {
  StringPiece vb_test(getenv("VIRTUALBOX_TEST"));
  if (!vb_test.empty()) {
    return;
  }
  serf_url_async_fetcher_->set_list_outstanding_urls_on_error(true);
  ConnectionRefusedTest();

  // Depending on the timing of the failure, we may get 1 or two serious
  // messages.
  EXPECT_LE(1, message_handler_.SeriousMessages());
  EXPECT_GE(2, message_handler_.SeriousMessages());
  GoogleString text;
  StringWriter text_writer(&text);
  message_handler_.Dump(&text_writer);
  EXPECT_TRUE(text.find(StrCat("URL ", urls_[kConnectionRefused],
                               " active for")) != GoogleString::npos)
      << text;
}

// Make sure when we use URL and Host: mismatch to route request to
// a particular point, that the message is helpful.
TEST_F(SerfUrlAsyncFetcherTest,
       ThreadedConnectionRefusedCustomRouteWithDetail) {
  serf_url_async_fetcher_->set_list_outstanding_urls_on_error(true);

  int index = AddTestUrl("http://127.0.0.1:1023/refused.jpg", "");
  request_headers(index)->Add(HttpAttributes::kHost,
                              StrCat(test_host_, ":1023"));
  StartFetches(index, index);
  ASSERT_EQ(WaitTillDone(index, index), 1);
  ASSERT_TRUE(fetches_[index]->IsDone());
  EXPECT_EQ(HttpStatus::kNotFound, response_headers(index)->status_code());
  GoogleString text;
  StringWriter text_writer(&text);
  message_handler_.Dump(&text_writer);
  GoogleString msg = StrCat(urls_[kConnectionRefused],
                            " (connecting to:127.0.0.1:1023)");
  EXPECT_TRUE(text.find(msg) != GoogleString::npos) << text;
  ValidateMonitoringStats(0, 1);
}

// Test that the X-Original-Content-Length header is properly set
// when requested.
TEST_F(SerfUrlAsyncFetcherTest, TestTrackOriginalContentLength) {
  serf_url_async_fetcher_->set_track_original_content_length(true);
  StartFetch(kModpagespeedSite);
  WaitTillDone(kModpagespeedSite, kModpagespeedSite);
  FlakyRetry(kModpagespeedSite);
  const char* ocl_header = response_headers(kModpagespeedSite)->Lookup1(
      HttpAttributes::kXOriginalContentLength);
  ASSERT_TRUE(ocl_header != NULL);
  int bytes_count =
      statistics_->GetVariable(SerfStats::kSerfFetchByteCount)->Get();
  int64 ocl_value;
  EXPECT_TRUE(StringToInt64(ocl_header, &ocl_value));
  EXPECT_EQ(bytes_count, ocl_value);
}

TEST_F(SerfUrlAsyncFetcherTest, TestHostConstruction) {
  apr_uri_t uri1;
  EXPECT_EQ(APR_SUCCESS,
            apr_uri_parse(pool_, "http://www.example.com/example.css", &uri1));
  EXPECT_STREQ("www.example.com",
               SerfUrlAsyncFetcher::ExtractHostHeader(uri1, pool_));

  apr_uri_t uri2;
  EXPECT_EQ(APR_SUCCESS,
            apr_uri_parse(pool_,
                          "http://me:password@www.example.com/example.css",
                          &uri2));
  EXPECT_STREQ("www.example.com",
               SerfUrlAsyncFetcher::ExtractHostHeader(uri2, pool_));

  apr_uri_t uri3;
  EXPECT_EQ(APR_SUCCESS,
            apr_uri_parse(pool_,
                          "http://me:password@www.example.com:42/example.css",
                          &uri3));
  EXPECT_STREQ("www.example.com:42",
               SerfUrlAsyncFetcher::ExtractHostHeader(uri3, pool_));
}

TEST_F(SerfUrlAsyncFetcherTest, TestPortRemoval) {
  // Tests our little helper for removing port numbers, which is needed to
  // compute SNI headers from Host: headers.
  EXPECT_EQ(
      "www.example.com",
      SerfUrlAsyncFetcher::RemovePortFromHostHeader("www.example.com"));
  EXPECT_EQ(
      "www.example.com",
      SerfUrlAsyncFetcher::RemovePortFromHostHeader("www.example.com:80"));
  EXPECT_EQ(
      "[::1]",
      SerfUrlAsyncFetcher::RemovePortFromHostHeader("[::1]"));
  EXPECT_EQ(
      "[::1]",
      SerfUrlAsyncFetcher::RemovePortFromHostHeader("[::1]:80"));
}

TEST_F(SerfUrlAsyncFetcherTest, TestPost) {
  int index = AddTestUrl(StrCat("http://", test_host_,
                                "/do_not_modify/cgi/verify_post.cgi"),
                         "PASS");
  request_headers(index)->set_method(RequestHeaders::kPost);
  request_headers(index)->set_message_body("a=b&c=d");
  StartFetches(index, index);
  ASSERT_EQ(WaitTillDone(index, index), 1);
  ValidateFetches(index, index);
  EXPECT_EQ(HttpStatus::kOK, response_headers(index)->status_code());
}

class SerfFetchTest : public SerfUrlAsyncFetcherTest {
 protected:
  SerfFetchTest()
      : async_fetch_(new StringAsyncFetch(
            RequestContext::NewTestRequestContext(thread_system_.get()))) {
  }

  virtual void TearDown() {
    async_fetch_->response_headers()->set_status_code(200);
    serf_fetch_->CallbackDone(SerfCompletionResult::kSuccess);
    // Fetch must be deleted before fetcher because it has a child pool.
    serf_fetch_.reset(NULL);
    SerfUrlAsyncFetcherTest::TearDown();
  }

  bool ParseUrl(const GoogleString& url) {
    serf_fetch_.reset(new SerfFetch(
        url, async_fetch_.get(), &message_handler_, timer_.get()));
    serf_fetch_->SetFetcherForTesting(serf_url_async_fetcher_.get());

    bool status;
    serf_fetch_->ParseUrlForTesting(
        &status, &parsed_url_, &host_header_, &sni_host_);

    return status;
  }

  scoped_ptr<StringAsyncFetch> async_fetch_;
  scoped_ptr<SerfFetch> serf_fetch_;
  apr_uri_t* parsed_url_;
  const char* host_header_;
  const char* sni_host_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SerfFetchTest);
};

TEST_F(SerfFetchTest, TestParseUrl) {
  ASSERT_TRUE(ParseUrl("http://www.example.com/foo/bar"));
  EXPECT_EQ("www.example.com", StringPiece(host_header_));
  EXPECT_EQ("www.example.com", StringPiece(parsed_url_->hostinfo));
  EXPECT_EQ("", StringPiece(sni_host_));
  EXPECT_EQ(80, parsed_url_->port);
  EXPECT_EQ("http", StringPiece(parsed_url_->scheme));
  EXPECT_EQ("/foo/bar", StringPiece(parsed_url_->path));
}

TEST_F(SerfFetchTest, TestParseUrlAlternatePort) {
  ASSERT_TRUE(ParseUrl("http://www.example.com:8080/foo/bar"));
  EXPECT_EQ("www.example.com:8080", StringPiece(host_header_));
  EXPECT_EQ("www.example.com:8080", StringPiece(parsed_url_->hostinfo));
  EXPECT_EQ("", StringPiece(sni_host_));
  EXPECT_EQ(8080, parsed_url_->port);
  EXPECT_EQ("http", StringPiece(parsed_url_->scheme));
  EXPECT_EQ("/foo/bar", StringPiece(parsed_url_->path));
}

TEST_F(SerfFetchTest, TestParseUrlHttpsDisallowed) {
  ASSERT_FALSE(ParseUrl("https://www.example.com/foo/bar"));
}

TEST_F(SerfFetchTest, TestParseUrlHttpsAllowed) {
  serf_url_async_fetcher_->SetHttpsOptions("enable");
  ASSERT_TRUE(ParseUrl("https://www.example.com/foo/bar"));
  EXPECT_EQ("www.example.com", StringPiece(host_header_));
  EXPECT_EQ("www.example.com", StringPiece(parsed_url_->hostinfo));
  EXPECT_EQ("www.example.com", StringPiece(sni_host_));
  EXPECT_EQ(443, parsed_url_->port);
  EXPECT_EQ("https", StringPiece(parsed_url_->scheme));
  EXPECT_EQ("/foo/bar", StringPiece(parsed_url_->path));
}

TEST_F(SerfFetchTest, TestParseUrlAlternatePortHttps) {
  serf_url_async_fetcher_->SetHttpsOptions("enable");
  ASSERT_TRUE(ParseUrl("https://www.example.com:8080/foo/bar"));
  EXPECT_EQ("www.example.com:8080", StringPiece(host_header_));
  EXPECT_EQ("www.example.com:8080", StringPiece(parsed_url_->hostinfo));
  EXPECT_EQ("www.example.com", StringPiece(sni_host_));
  EXPECT_EQ(8080, parsed_url_->port);
  EXPECT_EQ("https", StringPiece(parsed_url_->scheme));
  EXPECT_EQ("/foo/bar", StringPiece(parsed_url_->path));
}

TEST_F(SerfFetchTest, TestParseUrlDoubleSlash) {
  ASSERT_TRUE(ParseUrl("http://www.example.com//foo/bar"));
  EXPECT_EQ("www.example.com", StringPiece(host_header_));
  EXPECT_EQ("www.example.com", StringPiece(parsed_url_->hostinfo));
  EXPECT_EQ("", StringPiece(sni_host_));
  EXPECT_EQ(80, parsed_url_->port);
  EXPECT_EQ("http", StringPiece(parsed_url_->scheme));
  EXPECT_EQ("//foo/bar", StringPiece(parsed_url_->path));
}

TEST_F(SerfFetchTest, TestParseUrlDoubleSlashEncodedSpace) {
  ASSERT_TRUE(ParseUrl(
      "http://www.example.com//foo/bar/baz/BDKL%20319652.JPG"));
  EXPECT_EQ("www.example.com", StringPiece(host_header_));
  EXPECT_EQ("www.example.com", StringPiece(parsed_url_->hostinfo));
  EXPECT_EQ("", StringPiece(sni_host_));
  EXPECT_EQ(80, parsed_url_->port);
  EXPECT_EQ("http", StringPiece(parsed_url_->scheme));
  EXPECT_EQ("//foo/bar/baz/BDKL%20319652.JPG", StringPiece(parsed_url_->path));
}

TEST_F(SerfFetchTest, TestParseUrlDoubleSlashRawSpace) {
  ASSERT_TRUE(ParseUrl("http://www.example.com//foo/bar/baz/BDKL 319652.JPG"));
  EXPECT_EQ("www.example.com", StringPiece(host_header_));
  EXPECT_EQ("www.example.com", StringPiece(parsed_url_->hostinfo));
  EXPECT_EQ("", StringPiece(sni_host_));
  EXPECT_EQ(80, parsed_url_->port);
  EXPECT_EQ("http", StringPiece(parsed_url_->scheme));
  EXPECT_EQ("//foo/bar/baz/BDKL 319652.JPG", StringPiece(parsed_url_->path));
}

class SerfUrlAsyncFetcherTestWithProxy : public SerfUrlAsyncFetcherTest {
 protected:
  virtual void SetUp() {
    // We don't expect this to be a working proxy; this is only used for
    // just covering a crash bug.
    SetUpWithProxy("127.0.0.1:8080");
  }
};

TEST_F(SerfUrlAsyncFetcherTestWithProxy, TestBlankUrl) {
  // Fetcher used to have problems if blank URLs got to it somehow.
  int index = AddTestUrl("", "");
  StartFetches(index, index);
  ASSERT_EQ(WaitTillDone(index, index), 1);
  ASSERT_TRUE(fetches_[index]->IsDone());
  EXPECT_EQ(HttpStatus::kNotFound, response_headers(index)->status_code());
  ValidateMonitoringStats(0, 1);
}

class SerfUrlAsyncFetcherTestFakeWebServer : public SerfUrlAsyncFetcherTest {
 public:
  class FakeWebServerThread : public TcpServerThreadForTesting {
   public:
    FakeWebServerThread(apr_port_t desired_listen_port,
                        ThreadSystem* thread_system)
        : TcpServerThreadForTesting(desired_listen_port_, "fake_webserver",
                                    thread_system) {}
    virtual ~FakeWebServerThread() { ShutDown(); }

    void HandleClientConnection(apr_socket_t* sock) override {
      char request_buffer[kStackBufferSize];
      apr_size_t req_bufsz = sizeof(request_buffer) - 1;
      apr_socket_recv(sock, request_buffer, &req_bufsz);

      static const char kResponse[] = R"END(HTTP/1.0 282 Fake Status Code
Content-Length: 500
Connection: close
Content-Type: text/plain

This text is less than 500 bytes.
)END";

      apr_size_t response_size = STATIC_STRLEN(kResponse);
      apr_socket_send(sock, kResponse, &response_size);
      // Wait for over the timeout to make really sure it times out.
      WaitForHangupOrTimeout(sock, FetcherTimeoutMs() * 1000 * 2);
      apr_socket_close(sock);
    }

   private:
    // Use apr_poll to wait up to timeout_us for the socket to hangup or go
    // readable (which we assume to be a hangup).
    void WaitForHangupOrTimeout(apr_socket_t* socket, int timeout_us) {
      // Create pollset. Using NOCOPY so we can just feed in a stack variable
      // for the pollfd.
      apr_pollset_t* pollset;
      apr_status_t status = apr_pollset_create(&pollset, 1 /* size */, pool(),
                                               APR_POLLSET_NOCOPY);
      ASSERT_EQ(APR_SUCCESS, status);

      // Populate pollfd and add it to pollset.
      apr_pollfd_t pollfd = { 0 };
      pollfd.desc_type = APR_POLL_SOCKET;
      pollfd.desc.s = socket;
      pollfd.reqevents = APR_POLLHUP | APR_POLLERR | APR_POLLIN;
      pollfd.p = pool();
      status = apr_pollset_add(pollset, &pollfd);
      ASSERT_EQ(APR_SUCCESS, status);

      // Now actually wait.
      do {
        apr_int32_t nactive;
        const apr_pollfd_t* outfds;
        status = apr_pollset_poll(pollset, timeout_us, &nactive, &outfds);
      } while (APR_STATUS_IS_EINTR(status));
      ASSERT_EQ(APR_SUCCESS, status);

      // Be nice and cleanup.
      status = apr_pollset_destroy(pollset);
      ASSERT_EQ(APR_SUCCESS, status);
    }
  };

  static void SetUpTestCase() {
    TcpServerThreadForTesting::PickListenPortOnce(&desired_listen_port_);
  }

  void SetUp() override {
    thread_.reset(
        new FakeWebServerThread(desired_listen_port_, thread_system_.get()));
    ASSERT_TRUE(thread_->Start());
    // This blocks until the thread is actually listening.
    int port = thread_->GetListeningPort();
    GoogleString proxy_address = StrCat("127.0.0.1:", IntegerToString(port));
    SetUpWithProxy(proxy_address.c_str());
  }

  scoped_ptr<FakeWebServerThread> thread_;

 private:
  static apr_port_t desired_listen_port_;
};

apr_port_t SerfUrlAsyncFetcherTestFakeWebServer::desired_listen_port_ = 0;

TEST_F(SerfUrlAsyncFetcherTestFakeWebServer, TestHangingGet) {
  Variable* timeouts =
      statistics_->GetVariable(SerfStats::kSerfFetchTimeoutCount);
  EXPECT_EQ(0, timeouts->Get());
  int index = AddTestUrl(StrCat("http://", test_host_, "/never_fetched"), "");
  StartFetches(index, index);
  ASSERT_EQ(WaitTillDone(index, index), 1);
  ASSERT_TRUE(fetches_[index]->IsDone());
  EXPECT_EQ(1, timeouts->Get());
  EXPECT_EQ(282, response_headers(index)->status_code());
  EXPECT_STREQ("This text is less than 500 bytes.\n", contents(index));
  Variable* read_calls =
      statistics_->FindVariable("serf_fetch_num_calls_to_read");
#ifdef NDEBUG
  // We don't want this statistic on prod builds.
  EXPECT_EQ(nullptr, read_calls);
#else
  // This is the most important part of this test. Verify that read was only
  // called a handful of times (while we waited in poll), not millions of times
  // until it stopped returning EAGAIN.
  EXPECT_GE(5, read_calls->Get());
#endif
}

}  // namespace net_instaweb
