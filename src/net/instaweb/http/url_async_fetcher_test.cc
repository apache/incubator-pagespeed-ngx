/*
 * Copyright 2011 Google Inc.
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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/http/public/url_async_fetcher.h"

#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/time_util.h"

namespace net_instaweb {

namespace {

const int64 kOldTime = 1000;  // Date of an old resource.
const int64 kNewTime = 2000;  // Date of a new resource.
const char kUrl[] = "http://www.example.com/foo/bar.css";
const char kNewContents[] = "These are the new contents!";

// Mock fetcher that returns one of two things.
//   1) Empty "304 Not Modified" if the correct headers and time were sent.
//   2) Normal "200 OK" with contents otherwise.
class MockConditionalFetcher : public UrlAsyncFetcher {
 public:
  MockConditionalFetcher() {}
  virtual ~MockConditionalFetcher() {}

  virtual bool StreamingFetch(const GoogleString& url,
                              const RequestHeaders& request_headers,
                              ResponseHeaders* response_headers,
                              Writer* response_writer,
                              MessageHandler* message_handler,
                              Callback* callback) {
    StringStarVector values;
    int64 if_modified_since_time;
    if (request_headers.Lookup(HttpAttributes::kIfModifiedSince, &values) &&
        values.size() == 1 &&
        ConvertStringToTime(*values[0], &if_modified_since_time) &&
        if_modified_since_time >= kNewTime) {
      // We recieved and If-Modified-Since header with a date that was
      // parsable and at least as new our new resource.
      //
      // So, just serve 304 Not Modified.
      response_headers->SetStatusAndReason(HttpStatus::kNotModified);
      response_headers->Add(HttpAttributes::kContentLength, "0");
    } else {
      // Otherwise serve a normal 200 OK response.
      response_headers->SetStatusAndReason(HttpStatus::kOK);
      response_headers->SetLastModified(kNewTime);
      response_headers->Add(HttpAttributes::kContentLength,
                           IntegerToString(STATIC_STRLEN(kNewContents)));
      response_writer->Write(kNewContents, message_handler);
    }

    callback->Done(true);
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockConditionalFetcher);
};

class CheckCallback : public UrlAsyncFetcher::ConditionalCallback {
 public:
  CheckCallback() : done_(false), status_(UrlAsyncFetcher::kFetchFailure) {}
  virtual ~CheckCallback() {}

  virtual void Done(UrlAsyncFetcher::FetchStatus status) {
    done_ = true;
    status_ = status;
  }

  bool done_;
  UrlAsyncFetcher::FetchStatus status_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CheckCallback);
};

class UrlAsyncFetcherTest : public ::testing::Test {
 protected:
  void TestConditionalFetch(int64 if_modified_since_ms, bool expect_modified) {
    MockConditionalFetcher mock_fetcher;
    RequestHeaders request_headers;
    ResponseHeaders response_headers;
    GoogleString response;
    StringWriter response_writer(&response);
    NullMessageHandler handler;
    CheckCallback check_callback;

    EXPECT_FALSE(check_callback.done_);
    mock_fetcher.ConditionalFetch(kUrl, if_modified_since_ms, request_headers,
                                  &response_headers, &response_writer,
                                  &handler, &check_callback);

    EXPECT_TRUE(check_callback.done_);
    if (expect_modified) {
      EXPECT_EQ(UrlAsyncFetcher::kModifiedResource, check_callback.status_);
      EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());
      EXPECT_FALSE(response.empty());
    } else {
      EXPECT_EQ(UrlAsyncFetcher::kNotModifiedResource, check_callback.status_);
      EXPECT_EQ(HttpStatus::kNotModified, response_headers.status_code());
      EXPECT_TRUE(response.empty());
    }
  }

};

TEST_F(UrlAsyncFetcherTest, ConditionalFetchCorrectly) {
  // Test that responses are correct.
  //   1) Yes, modified since kOldTime.
  TestConditionalFetch(kOldTime, true);
  //   2) No, not modified since kNewTime.
  TestConditionalFetch(kNewTime, false);
}

}  // namespace

}  // namespace net_instaweb
