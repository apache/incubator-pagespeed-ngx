// Copyright 2010 Google Inc. All Rights Reserved.
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
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "net/instaweb/apache/apr_file_system.h"
#include "net/instaweb/apache/apr_mutex.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/html_parser_message_handler.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/serf/src/serf.h"

using html_rewriter::AprFileSystem;
using html_rewriter::HtmlParserMessageHandler;

namespace {

const char kProxy[] = "";
const int kMaxMs = 10000;
const int kThreadedPollMs = 1000;
const int kWaitTimeoutMs = 5 * 1000;

class TestCallback : public net_instaweb::UrlAsyncFetcher::Callback {
 public:
  explicit TestCallback(html_rewriter::AprMutex* mutex, const std::string& url)
      : done_(false),
        mutex_(mutex),
        url_(url),
        enable_threaded_(false) {
  }
  virtual ~TestCallback() {}
  virtual void Done(bool success)  {
    net_instaweb::ScopedMutex lock(mutex_);
    done_ = true;
  }
  bool IsDone() const {
    net_instaweb::ScopedMutex lock(mutex_);
    return done_;
  }
  virtual bool EnableThreaded() const {
    return enable_threaded_;
  }
  void set_enable_threaded(bool b) { enable_threaded_ = b; }
 private:
  bool done_;
  html_rewriter::AprMutex* mutex_;
  std::string url_;
  bool enable_threaded_;
};

class SerfUrlAsyncFetcherTest: public ::testing::Test {
 public:
  static void SetUpTestCase() {
    apr_initialize();
    atexit(apr_terminate);
  }

 protected:
  virtual void SetUp() {
    apr_pool_create(&pool_, NULL);
    serf_url_async_fetcher_.reset(
        new html_rewriter::SerfUrlAsyncFetcher(kProxy, pool_));
    mutex_ = new html_rewriter::AprMutex(pool_);
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
    request_headers_.push_back(new net_instaweb::SimpleMetaData);
    response_headers_.push_back(new net_instaweb::SimpleMetaData);
    contents_.push_back(new std::string);
    writers_.push_back(new net_instaweb::StringWriter(contents_.back()));
    callbacks_.push_back(new TestCallback(mutex_, url));
  }

  void StartFetches(size_t begin, size_t end, bool enable_threaded) {
    for (size_t idx = begin; idx < end; ++idx) {
      TestCallback* callback = callbacks_[idx];
      callback->set_enable_threaded(enable_threaded);
      serf_url_async_fetcher_->StreamingFetch(
          urls_[idx], *request_headers_[idx], response_headers_[idx],
          writers_[idx], &message_handler_, callback);
    }
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
    html_rewriter::AprTimer timer;
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
  std::vector<net_instaweb::SimpleMetaData*> request_headers_;
  std::vector<net_instaweb::SimpleMetaData*> response_headers_;
  std::vector<std::string*> contents_;
  std::vector<net_instaweb::StringWriter*> writers_;
  std::vector<TestCallback*> callbacks_;
  // The fetcher to be tested.
  scoped_ptr<html_rewriter::SerfUrlAsyncFetcher> serf_url_async_fetcher_;
  html_rewriter::HtmlParserMessageHandler message_handler_;
  size_t prev_done_count;
  html_rewriter::AprMutex* mutex_;
};

TEST_F(SerfUrlAsyncFetcherTest, FetchOneURL) {
  EXPECT_TRUE(TestFetch(0, 1));
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
      kWaitTimeoutMs, &message_handler_);
}

TEST_F(SerfUrlAsyncFetcherTest, TestWaitOneThreadedTwoSync) {
  StartFetches(0, 1, true);
  StartFetches(1, 3, false);
  serf_url_async_fetcher_->WaitForInProgressFetches(
      kWaitTimeoutMs, &message_handler_);
}

TEST_F(SerfUrlAsyncFetcherTest, TestWaitTwoThreadedOneSync) {
  StartFetches(0, 1, false),
  StartFetches(1, 3, true);
  serf_url_async_fetcher_->WaitForInProgressFetches(
      kWaitTimeoutMs, &message_handler_);
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

}  // namespace
