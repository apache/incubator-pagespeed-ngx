/*
 * Copyright 2010 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test framework for wget fetcher

#ifndef NET_INSTAWEB_HTTP_PUBLIC_FETCHER_TEST_H_
#define NET_INSTAWEB_HTTP_PUBLIC_FETCHER_TEST_H_

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/wait_url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/simple_stats.h"

namespace net_instaweb {

class ResponseHeaders;
class UrlAsyncFetcher;

class FetcherTest : public testing::Test {
 protected:
  static const char kStartDate[];
  static const char kHtmlContent[];
  static const char kGoodUrl[];
  static const char kNotCachedUrl[];
  static const char kBadUrl[];
  static const char kHeaderName[];
  static const char kHeaderValue[];
  static const char kErrorMessage[];

  FetcherTest();

  // Helpful classes for testing.

  // We set up a chain of fetchers:
  // Counting -> Wait -> Mock, where the mock will only fetch
  // kGoodUrl and kNotCachedUrl, returning kHtmlContent.
  WaitUrlAsyncFetcher* wait_fetcher() {
      return &wait_url_async_fetcher_;
  }

  CountingUrlAsyncFetcher* counting_fetcher() { return &counting_fetcher_; }

  // Callback that just checks correct Done status and keeps track of whether
  // it has been called yet or not.
  class CheckCallback : public StringAsyncFetch {
   public:
    CheckCallback(const RequestContextPtr& ctx, bool expect_success,
                  bool check_error_message,
                  bool* callback_called)
        : StringAsyncFetch(ctx),
          expect_success_(expect_success),
          check_error_message_(check_error_message),
          callback_called_(callback_called) {
    }

    virtual void HandleDone(bool success) {
      *callback_called_ = true;
      CHECK_EQ(expect_success_, success);
      ValidateMockFetcherResponse(
          success, check_error_message_, buffer(), *response_headers());
      delete this;
    }

   private:
    bool expect_success_;
    bool check_error_message_;
    bool* callback_called_;

    DISALLOW_COPY_AND_ASSIGN(CheckCallback);
  };

  // This checks that response matches the mock response we setup.
  static void ValidateMockFetcherResponse(
      bool success, bool check_error_message, const GoogleString& content,
      const ResponseHeaders& response_headers);

  // Initiate an async URL fetch, and return the number of times the counting
  // fetcher had to be run to perform the fetch.
  // Note: You must override async_fetcher() to return the correct fetcher.
  int CountFetchesAsync(const StringPiece& url, bool expect_success,
                        bool* callback_called);

  // Like above, but doesn't use async_fetcher(), and lets you opt-out
  // of checking of error messages
  int CountFetchesAsync(const StringPiece& url, UrlAsyncFetcher* fetcher,
                        bool expect_success, bool check_error_message,
                        bool* callback_called);

  // Override this to allow CountFetchesAsync w/o fetcher argument.
  // It is not abstract (= 0) because they only need to be overridden by
  // classes which want to use CountFetchersAsync.
  virtual UrlAsyncFetcher* async_fetcher() {
    LOG(FATAL) << "async_fetcher() must be overridden before use.";
    return NULL;
  };

  GoogleString TestFilename() {
    return (GTestSrcDir() +
            "/net/instaweb/http/testdata/google.http");
  }

  // This validation code is hard-coded to the http request capture in
  // testdata/google.http.
  void ValidateOutput(const GoogleString& content,
                      const ResponseHeaders& response_headers);

  GoogleMessageHandler message_handler_;
  MockUrlFetcher mock_fetcher_;
  WaitUrlAsyncFetcher wait_url_async_fetcher_;
  CountingUrlAsyncFetcher counting_fetcher_;
  scoped_ptr<ThreadSystem> thread_system_;
  SimpleStats statistics_;

 private:
  void Populate(const char* cache_control,
                ResponseHeaders* response_headers,
                GoogleString* content);

  DISALLOW_COPY_AND_ASSIGN(FetcherTest);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_FETCHER_TEST_H_
