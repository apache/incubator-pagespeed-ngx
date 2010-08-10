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
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "net/instaweb/apache/apr_file_system.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/html_parser_message_handler.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/apache/apr/src/include/apr_atomic.h"
#include "third_party/apache/apr/src/include/apr_pools.h"
#include "third_party/apache/apr/src/include/apr_strings.h"
#include "third_party/apache/apr/src/include/apr_version.h"
#include "third_party/serf/src/serf.h"

using html_rewriter::AprFileSystem;
using html_rewriter::HtmlParserMessageHandler;

namespace {

const char kProxy[] = "";

class TestCallback : public net_instaweb::UrlAsyncFetcher::Callback {
 public:
  explicit TestCallback(net_instaweb::MessageHandler* message_handler)
      : done_(false),
        message_handler_(message_handler) {}
  virtual ~TestCallback() {}
  virtual void Done(bool success)  {
    done_ = true;
  }
  bool IsDone() const { return done_; }
 private:
  bool done_;
  net_instaweb::MessageHandler* message_handler_;
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
  }

  virtual void TearDown() {
    STLDeleteElements(&request_headers_);
    STLDeleteElements(&response_headers_);
    STLDeleteElements(&contents_);
    STLDeleteElements(&writers_);
    STLDeleteElements(&callbacks_);
    // Need to free the fetcher before destroy the pool.
    serf_url_async_fetcher_.reset(NULL);
    apr_pool_destroy(pool_);
  }

  void AddTestUrl(const std::string& url,
                  const std::string& content_start) {
    urls_.push_back(url);
    content_starts_.push_back(content_start);
    request_headers_.push_back(new net_instaweb::SimpleMetaData);
    response_headers_.push_back(new net_instaweb::SimpleMetaData);
    contents_.push_back(new std::string);
    writers_.push_back(new net_instaweb::StringWriter(contents_.back()));
    callbacks_.push_back(new TestCallback(&message_handler_));
  }

  void TestFetch() {
    for (size_t idx = 0; idx < urls_.size(); ++idx) {
      serf_url_async_fetcher_->StreamingFetch(
          urls_[idx], *request_headers_[idx], response_headers_[idx],
          writers_[idx], &message_handler_, callbacks_[idx]);
    }
    html_rewriter::AprTimer timer;
    int64 max_ms = 10000;
    bool not_done = true;
    for (int64 start_ms = timer.NowMs(), now_ms = start_ms;
         not_done && now_ms - start_ms < max_ms;
         now_ms = timer.NowMs()) {
      not_done = false;
      for (size_t idx = 0; idx < callbacks_.size(); ++idx) {
        if (!callbacks_[idx]->IsDone()) {
          not_done = true;
          break;
        }
      }
      int64 remaining_us = std::max(static_cast<int64>(0),
                                    1000 * (max_ms - now_ms));
      serf_url_async_fetcher_->Poll(remaining_us, &message_handler_);
    }

    // validate
    for (size_t idx = 0; idx < urls_.size(); ++idx) {
      ASSERT_TRUE(callbacks_[idx]->IsDone());
      EXPECT_LT(static_cast<size_t>(0), contents_[idx]->size());
      EXPECT_EQ(200, response_headers_[idx]->status_code());
      EXPECT_EQ(content_starts_[idx],
                contents_[idx]->substr(0, content_starts_[idx].size()));
    }
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
};

TEST_F(SerfUrlAsyncFetcherTest, FetchOneURL) {
  std::string url("http://www.google.com/");
  const std::string start_of_html("<!doctype html>");
  AddTestUrl(url, start_of_html);
  TestFetch();
}


TEST_F(SerfUrlAsyncFetcherTest, FetchTwoURLs) {
  std::string url("http://www.google.com/favicon.ico");
  const std::string start_of_ico("\000\000\001\000", 4);
  AddTestUrl(url, start_of_ico);

  std::string url_2("http://www.google.com/intl/en_ALL/images/logo.gif");
  const std::string start_of_gif("GIF");
  AddTestUrl(url_2, start_of_gif);
  TestFetch();
}

}  // namespace
