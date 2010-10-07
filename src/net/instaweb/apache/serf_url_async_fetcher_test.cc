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
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "net/instaweb/apache/apr_file_system.h"
#include "net/instaweb/apache/apr_mutex.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gzip_inflater.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/serf/src/serf.h"

namespace net_instaweb {

namespace {

const char kProxy[] = "";
const int kMaxMs = 10000;
const int kThreadedPollMs = 1000;
const int kWaitTimeoutMs = 5 * 1000;

class SerfTestCallback : public UrlAsyncFetcher::Callback {
 public:
  explicit SerfTestCallback(AprMutex* mutex, const std::string& url)
      : done_(false),
        mutex_(mutex),
        url_(url),
        enable_threaded_(false) {
  }
  virtual ~SerfTestCallback() {}
  virtual void Done(bool success)  {
    ScopedMutex lock(mutex_);
    done_ = true;
  }
  bool IsDone() const {
    ScopedMutex lock(mutex_);
    return done_;
  }
  virtual bool EnableThreaded() const {
    return enable_threaded_;
  }
  void set_enable_threaded(bool b) { enable_threaded_ = b; }
 private:
  bool done_;
  AprMutex* mutex_;
  std::string url_;
  bool enable_threaded_;

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
    serf_url_async_fetcher_.reset(
        new SerfUrlAsyncFetcher(kProxy, pool_));
    mutex_ = new AprMutex(pool_);
    AddTestUrl("http://www.google.com/", "<!doctype html>");
    AddTestUrl("http://www.google.com/favicon.ico",
               std::string("\000\000\001\000", 4));
    AddTestUrl("http://www.google.com/intl/en_ALL/images/logo.gif", "GIF");
    prev_done_count = 0;
  }

  virtual void TearDown() {
    // Need to free the fetcher before destroy the pool.
    serf_url_async_fetcher_.reset(NULL);
    STLDeleteElements(&request_headers_);
    STLDeleteElements(&response_headers_);
    STLDeleteElements(&contents_);
    STLDeleteElements(&writers_);
    STLDeleteElements(&callbacks_);
    apr_pool_destroy(pool_);
    delete mutex_;
  }

  void AddTestUrl(const std::string& url,
                  const std::string& content_start) {
    urls_.push_back(url);
    content_starts_.push_back(content_start);
    request_headers_.push_back(new SimpleMetaData);
    response_headers_.push_back(new SimpleMetaData);
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

  bool WaitTillDone(size_t begin, size_t end, int64 delay_ms) {
    AprTimer timer;
    bool done = false;
    int64 now_ms = timer.NowMs();
    int64 end_ms = now_ms + delay_ms;
    while (!done && (now_ms < end_ms)) {
      int64 remaining_ms = end_ms - now_ms;
      serf_url_async_fetcher_->Poll(1000 * remaining_ms);
      size_t done_count = 0;
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
    return done;
  }

  bool TestFetch(size_t begin, size_t end) {
    StartFetches(begin, end, false);
    bool done = WaitTillDone(begin, end, kMaxMs);
    ValidateFetches(begin, end);
    return done;
  }

  apr_pool_t* pool_;
  std::vector<std::string> urls_;
  std::vector<std::string> content_starts_;
  std::vector<SimpleMetaData*> request_headers_;
  std::vector<SimpleMetaData*> response_headers_;
  std::vector<std::string*> contents_;
  std::vector<StringWriter*> writers_;
  std::vector<SerfTestCallback*> callbacks_;
  // The fetcher to be tested.
  scoped_ptr<SerfUrlAsyncFetcher> serf_url_async_fetcher_;
  GoogleMessageHandler message_handler_;
  size_t prev_done_count;
  AprMutex* mutex_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SerfUrlAsyncFetcherTest);
};

TEST_F(SerfUrlAsyncFetcherTest, FetchOneURL) {
  EXPECT_TRUE(TestFetch(0, 1));
  EXPECT_FALSE(response_headers_[0]->IsGzipped());
}

TEST_F(SerfUrlAsyncFetcherTest, FetchOneURLGzipped) {
  request_headers_[0]->Add(HttpAttributes::kAcceptEncoding,
                           HttpAttributes::kGzip);

  // www.google.com doesn't respect our 'gzip' encoding request unless
  // we have a reasonable user agent.
  const char kDefaultUserAgent[] =
    "Mozilla/5.0 (X11; U; Linux x86_64; en-US) "
    "AppleWebKit/534.0 (KHTML, like Gecko) Chrome/6.0.408.1 Safari/534.0";

  request_headers_[0]->Add(HttpAttributes::kUserAgent,
                           kDefaultUserAgent);
  StartFetches(0, 1, false);
  ASSERT_TRUE(WaitTillDone(0, 1, kMaxMs));
  ASSERT_TRUE(callbacks_[0]->IsDone());
  EXPECT_LT(static_cast<size_t>(0), contents_[0]->size());
  EXPECT_EQ(200, response_headers_[0]->status_code());
  ASSERT_TRUE(response_headers_[0]->IsGzipped());

  GzipInflater inflater;
  ASSERT_TRUE(inflater.Init());
  ASSERT_TRUE(inflater.SetInput(contents_[0]->data(), contents_[0]->size()));
  ASSERT_TRUE(inflater.HasUnconsumedInput());
  int size = content_starts_[0].size();
  scoped_array<char> buf(new char[size]);
  ASSERT_EQ(size, inflater.InflateBytes(buf.get(), size));
  EXPECT_EQ(content_starts_[0], std::string(buf.get(), size));
}

TEST_F(SerfUrlAsyncFetcherTest, FetchTwoURLs) {
  EXPECT_TRUE(TestFetch(1, 3));
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
  serf_url_async_fetcher_->WaitForInProgressFetches(
      kWaitTimeoutMs, &message_handler_,
      SerfUrlAsyncFetcher::kThreadedOnly);
}

TEST_F(SerfUrlAsyncFetcherTest, TestThreeThreadedAsync) {
  StartFetches(0, 1, true);
  serf_url_async_fetcher_->WaitForInProgressFetches(
      10 /* milliseconds */, &message_handler_,
      SerfUrlAsyncFetcher::kThreadedOnly);
  StartFetches(1, 3, true);

  // In this test case, we are not going to call the explicit threaded
  // wait function, WaitForInProgressFetches.  We have initiated async
  // fetches and we are hoping they will complete within a certain amount
  // of time.  If the system is running well then we they will finish
  // within a 100ms or so, so we'll loop in 50ms sleep intervals until
  // we hit a max.  We'll give it 5 seconds before declaring failure.
  const int kMaxSeconds = 5;
  const int kPollTimeUs = 50000;
  const int kPollsPerSecond = 1000000 / kPollTimeUs;
  const int kMaxIters = kMaxSeconds * kPollsPerSecond;
  int completed = 0;
  for (int i = 0; (completed < 3) && (i < kMaxIters); ++i) {
    usleep(kPollTimeUs);
    completed = CountCompletedFetches(0, 3);
  }
  ASSERT_EQ(3, completed) << "Async fetches times out before completing";
  EXPECT_TRUE(TestFetch(0, 3));
}

TEST_F(SerfUrlAsyncFetcherTest, TestWaitOneThreadedTwoSync) {
  StartFetches(0, 1, true);
  StartFetches(1, 3, false);
  serf_url_async_fetcher_->WaitForInProgressFetches(
      kWaitTimeoutMs, &message_handler_,
      SerfUrlAsyncFetcher::kThreadedAndMainline);
}

TEST_F(SerfUrlAsyncFetcherTest, TestWaitTwoThreadedOneSync) {
  StartFetches(0, 1, false),
  StartFetches(1, 3, true);
  serf_url_async_fetcher_->WaitForInProgressFetches(
      kWaitTimeoutMs, &message_handler_,
      SerfUrlAsyncFetcher::kThreadedAndMainline);
}

TEST_F(SerfUrlAsyncFetcherTest, TestThreeThreaded) {
  StartFetches(0, 3, true);
  bool done = false;
  for (int i = 0; !done && (i < 100); ++i) {
    done = WaitTillDone(0, 3, kThreadedPollMs);
  }
  EXPECT_TRUE(done);
  ValidateFetches(0, 3);
}

TEST_F(SerfUrlAsyncFetcherTest, TestOneThreadedTwoSync) {
  StartFetches(0, 1, true);
  StartFetches(1, 3, false);
  bool done = false;
  for (int i = 0; !done && (i < 100); ++i) {
    done = WaitTillDone(0, 3, kThreadedPollMs);
  }
  EXPECT_TRUE(done);
  ValidateFetches(0, 3);
}

TEST_F(SerfUrlAsyncFetcherTest, TestTwoThreadedOneSync) {
  StartFetches(0, 1, false),
  StartFetches(1, 3, true);
  bool done = false;
  for (int i = 0; !done && (i < 100); ++i) {
    done = WaitTillDone(0, 3, kThreadedPollMs);
  }
  EXPECT_TRUE(done);
  ValidateFetches(0, 3);
}

}  // namespace net_instaweb
