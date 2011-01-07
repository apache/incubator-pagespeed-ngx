/**
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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/util/public/mock_url_fetcher.h"

#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include <string>
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

namespace {

// See http://goto/gunitprimer for an introduction to gUnit.

class MockUrlFetcherTest : public ::testing::Test {
 protected:
  MockUrlFetcherTest() {
    fetcher_.set_fail_on_unexpected(false);
  }

  void TestResponse(const std::string& url,
                    const ResponseHeaders& expected_header,
                    const std::string& expected_body) {
    const RequestHeaders dummy_header;
    ResponseHeaders response_header;
    std::string response_body;
    StringWriter response_writer(&response_body);
    GoogleMessageHandler handler;

    EXPECT_TRUE(fetcher_.StreamingFetchUrl(url, dummy_header, &response_header,
                                           &response_writer, &handler));
    EXPECT_EQ(expected_header.ToString(), response_header.ToString());
    EXPECT_EQ(expected_body, response_body);
  }

  void TestFetchFail(const std::string& url) {
    const RequestHeaders dummy_header;
    ResponseHeaders response_header;
    std::string response_body;
    StringWriter response_writer(&response_body);
    GoogleMessageHandler handler;

    EXPECT_FALSE(fetcher_.StreamingFetchUrl(url, dummy_header, &response_header,
                                            &response_writer, &handler));
  }

  MockUrlFetcher fetcher_;
};

TEST_F(MockUrlFetcherTest, GetsCorrectMappedResponse) {
  const char url1[] = "http://www.example.com/successs.html";
  ResponseHeaders header1;
  header1.set_first_line(1, 1, 200, "OK");
  const char body1[] = "This website loaded :)";

  const char url2[] = "http://www.example.com/failure.html";
  ResponseHeaders header2;
  header2.set_first_line(1, 1, 404, "Not Found");
  const char body2[] = "File Not Found :(";

  // We can't fetch the URLs before they're set.
  // Note: this does not crash because we are using NullMessageHandler.
  TestFetchFail(url1);
  TestFetchFail(url2);

  // Set the responses.
  fetcher_.SetResponse(url1, header1, body1);
  fetcher_.SetResponse(url2, header2, body2);

  // Now we can fetch the correct URLs
  TestResponse(url1, header1, body1);
  TestResponse(url2, header2, body2);

  // Check that we can fetch the same URL multiple times.
  TestResponse(url1, header1, body1);


  // Check that fetches fail after disabling the fetcher_.
  fetcher_.Disable();
  TestFetchFail(url1);
  // And then work again when re-enabled.
  fetcher_.Enable();
  TestResponse(url1, header1, body1);


  // Change the response. Test both editability and memory management.
  fetcher_.SetResponse(url1, header2, body2);

  // Check that we get the new response.
  TestResponse(url1, header2, body2);

  // Change it back.
  fetcher_.SetResponse(url1, header1, body1);
  TestResponse(url1, header1, body1);
}

}  // namespace

}  // namespace net_instaweb
