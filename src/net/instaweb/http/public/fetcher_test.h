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

#include <algorithm>
#include <utility>  // for pair
#include <vector>
#include "base/basictypes.h"
#include "base/logging.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/http/public/url_fetcher.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

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

  FetcherTest() : mock_async_fetcher_(&mock_fetcher_) {}

  // Helpful classes for testing.

  // This mock fetcher will only fetch kGoodUrl, returning kHtmlContent.
  // If you ask for any other URL it will fail.
  class MockFetcher : public UrlFetcher {
   public:
    MockFetcher() : num_fetches_(0) {}


    virtual bool StreamingFetchUrl(const GoogleString& url,
                                   const RequestHeaders& request_headers,
                                   ResponseHeaders* response_headers,
                                   Writer* response_writer,
                                   MessageHandler* message_handler);

    int num_fetches() const { return num_fetches_; }

   private:
    bool Populate(const char* cache_control, ResponseHeaders* response_headers,
                  Writer* writer, MessageHandler* message_handler);

    int num_fetches_;

    DISALLOW_COPY_AND_ASSIGN(MockFetcher);
  };

  // This is a pseudo-asynchronous interface to MockFetcher.  It performs
  // fetches instantly, but defers calling the callback until the user
  // calls CallCallbacks().  Then it will execute the deferred callbacks.
  class MockAsyncFetcher : public UrlAsyncFetcher {
   public:
    explicit MockAsyncFetcher(UrlFetcher* url_fetcher)
        : url_fetcher_(url_fetcher) {}

    virtual bool StreamingFetch(const GoogleString& url,
                                const RequestHeaders& request_headers,
                                ResponseHeaders* response_headers,
                                Writer* response_writer,
                                MessageHandler* handler,
                                Callback* callback);

    void CallCallbacks();

   private:
    UrlFetcher* url_fetcher_;
    std::vector<std::pair<bool, Callback*> > deferred_callbacks_;

    DISALLOW_COPY_AND_ASSIGN(MockAsyncFetcher);
  };

  // Callback that just checks correct Done status and keeps track of whether
  // it has been called yet or not.
  class CheckCallback : public UrlAsyncFetcher::Callback {
   public:
    explicit CheckCallback(bool expect_success, bool* callback_called)
        : expect_success_(expect_success),
          content_writer_(&content_),
          callback_called_(callback_called) {
    }

    virtual void Done(bool success) {
      *callback_called_ = true;
      CHECK_EQ(expect_success_, success);
      ValidateMockFetcherResponse(success, true, content_, response_headers_);
      delete this;
    }

    bool expect_success_;
    ResponseHeaders response_headers_;
    GoogleString content_;
    StringWriter content_writer_;
    bool* callback_called_;

   private:
    DISALLOW_COPY_AND_ASSIGN(CheckCallback);
  };

  static void ValidateMockFetcherResponse(
      bool success, bool check_error_message, const GoogleString& content,
      const ResponseHeaders& response_headers);

  // Do a URL fetch, and return the number of times the mock fetcher
  // had to be run to perform the fetch.
  // Note: You must override sync_fetcher() to return the correct fetcher.
  int CountFetchesSync(const StringPiece& url, bool expect_success,
                       bool check_error_message);
  // Use an explicit fetcher (you don't need to override sync_fetcher()).
  int CountFetchesSync(const StringPiece& url, UrlFetcher* fetcher,
                       bool expect_success, bool check_error_message);

  // Initiate an async URL fetch, and return the number of times the mock
  // fetcher had to be run to perform the fetch.
  // Note: You must override async_fetcher() to return the correct fetcher.
  int CountFetchesAsync(const StringPiece& url, bool expect_success,
                        bool* callback_called);

  // Override these to allow CountFetchesSync or Async respectively.
  // These are not abstract (= 0) because they only need to be overridden by
  // classes which want to use CountFetchersSync/Async without specifying the
  // fetcher in each call.
  virtual UrlFetcher* sync_fetcher() {
    CHECK(false) << "sync_fetcher() must be overridden before use.";
    return NULL;
  };
  virtual UrlAsyncFetcher* async_fetcher() {
    CHECK(false) << "async_fetcher() must be overridden before use.";
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
  MockFetcher mock_fetcher_;
  MockAsyncFetcher mock_async_fetcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FetcherTest);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_FETCHER_TEST_H_
