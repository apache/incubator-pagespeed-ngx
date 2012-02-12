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

#include "net/instaweb/http/public/fake_url_async_fetcher.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

namespace {

const int64 kOldTime = 1000;  // Date of an old resource.
const int64 kNewTime = 2000;  // Date of a new resource.
const char kUrl[] = "http://www.example.com/foo/bar.css";
const char kNewContents[] = "These are the new contents!";

class UrlAsyncFetcherTest : public ::testing::Test {
 protected:
  void TestConditionalFetch(int64 if_modified_since_ms, bool expect_modified) {
    MockUrlFetcher fetcher;
    FakeUrlAsyncFetcher async_fetcher(&fetcher);
    RequestHeaders request_headers;
    ResponseHeaders response_headers;
    GoogleString response;
    StringWriter response_writer(&response);
    NullMessageHandler handler;
    MockCallback check_callback;

    // Set MockUrlFetcher to respond conditionally based upon time given.
    {
      ResponseHeaders good_headers;
      good_headers.set_first_line(1, 1, HttpStatus::kOK, "OK");
      fetcher.SetConditionalResponse(kUrl, kNewTime, "",
                                     good_headers, kNewContents);
    }

    EXPECT_FALSE(check_callback.done());
    async_fetcher.ConditionalFetch(kUrl, if_modified_since_ms, request_headers,
                                   &response_headers, &response_writer,
                                   &handler, &check_callback);

    EXPECT_TRUE(check_callback.done());
    EXPECT_TRUE(check_callback.success());
    EXPECT_EQ(expect_modified, check_callback.modified());
    if (expect_modified) {
      EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());
      EXPECT_FALSE(response.empty());
    } else {
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
