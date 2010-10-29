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

#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_url_fetcher.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include <string>
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/simple_meta_data.h"

namespace net_instaweb {

namespace {

// See http://goto/gunitprimer for an introduction to gUnit.

class MockUrlFetcherTest : public ::testing::Test {
 protected:
  void ExpectEqualMetaData(const MetaData& expected, const MetaData& actual) {
    EXPECT_EQ(expected.ToString(), actual.ToString());
  }
};

TEST_F(MockUrlFetcherTest, GetsCorrectMappedResponse) {
  MockUrlFetcher fetcher;
  fetcher.set_fail_on_unexpected(false);
  const SimpleMetaData dummy_header;
  SimpleMetaData response_header;
  std::string response_body;
  StringWriter response_writer(&response_body);
  NullMessageHandler handler;

  const char url1[] = "http://www.example.com/successs.html";
  SimpleMetaData header1;
  header1.set_first_line(1, 1, 200, "OK");
  const char body1[] = "This website loaded :)";

  const char url2[] = "http://www.example.com/failure.html";
  SimpleMetaData header2;
  header2.set_first_line(1, 1, 404, "Not Found");
  const char body2[] = "File Not Found :(";

  // We can't fetch the URLs before they're set.
  // Note: this does not crash because we are using NullMessageHandler.
  EXPECT_FALSE(fetcher.StreamingFetchUrl(url1, dummy_header, &response_header,
                                         &response_writer, &handler));
  EXPECT_FALSE(fetcher.StreamingFetchUrl(url2, dummy_header, &response_header,
                                         &response_writer, &handler));

  // Set the responses.
  fetcher.SetResponse(url1, header1, body1);
  fetcher.SetResponse(url2, header2, body2);

  // Now we can fetch the correct URLs
  EXPECT_TRUE(fetcher.StreamingFetchUrl(url1, dummy_header, &response_header,
                                        &response_writer, &handler));
  ExpectEqualMetaData(header1, response_header);
  EXPECT_EQ(body1, response_body);
  response_header.Clear();
  response_body.clear();

  EXPECT_TRUE(fetcher.StreamingFetchUrl(url2, dummy_header, &response_header,
                                        &response_writer, &handler));
  ExpectEqualMetaData(header2, response_header);
  EXPECT_EQ(body2, response_body);
  response_header.Clear();
  response_body.clear();

  // Check that we can fetch the same URL multiple times.
  EXPECT_TRUE(fetcher.StreamingFetchUrl(url1, dummy_header, &response_header,
                                        &response_writer, &handler));
  ExpectEqualMetaData(header1, response_header);
  EXPECT_EQ(body1, response_body);
  response_header.Clear();
  response_body.clear();

  // Check that fetches fail after disabling the fetcher.
  fetcher.Disable();
  EXPECT_FALSE(fetcher.StreamingFetchUrl(url1, dummy_header, &response_header,
                                         &response_writer, &handler));
  // And then work again when re-enabled.
  fetcher.Enable();
  EXPECT_TRUE(fetcher.StreamingFetchUrl(url1, dummy_header, &response_header,
                                        &response_writer, &handler));
}

}  // namespace

}  // namespace net_instaweb
