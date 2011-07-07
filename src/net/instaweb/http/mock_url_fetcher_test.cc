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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/http/public/mock_url_fetcher.h"

#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

// Class for encapuslating objects needed to execute fetches.
class MockFetchContainer {
 public:
  MockFetchContainer(UrlFetcher* fetcher)
      : fetcher_(fetcher), response_writer_(&response_body_) {}

  bool Fetch(const GoogleString& url) {
    return fetcher_->StreamingFetchUrl(url, request_headers_,
                                       &response_headers_, &response_writer_,
                                       &handler_);
  }

  UrlFetcher* fetcher_;
  RequestHeaders request_headers_;
  ResponseHeaders response_headers_;
  GoogleString response_body_;
  StringWriter response_writer_;
  GoogleMessageHandler handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFetchContainer);
};

class MockUrlFetcherTest : public ::testing::Test {
 protected:
  MockUrlFetcherTest() {
    fetcher_.set_fail_on_unexpected(false);
  }

  void TestResponse(const GoogleString& url,
                    const ResponseHeaders& expected_header,
                    const GoogleString& expected_body) {
    MockFetchContainer fetch(&fetcher_);
    EXPECT_TRUE(fetch.Fetch(url));
    EXPECT_EQ(expected_header.ToString(), fetch.response_headers_.ToString());
    EXPECT_EQ(expected_body, fetch.response_body_);
  }

  void TestFetchFail(const GoogleString& url) {
    MockFetchContainer fetch(&fetcher_);
    EXPECT_FALSE(fetch.Fetch(url));
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

TEST_F(MockUrlFetcherTest, ConditionalFetchTest) {
  const char url[] = "http://www.example.com/successs.html";
  ResponseHeaders header;
  header.set_first_line(1, 1, 200, "OK");
  // TODO: Perhaps set Last-Modified header...
  const char body[] = "This website loaded :)";

  int64 old_time = 1000;
  int64 last_modified_time = 2000;
  int64 new_time = 3000;
  GoogleString old_time_string, now_string, new_time_string;
  ASSERT_TRUE(ConvertTimeToString(old_time, &old_time_string));
  ASSERT_TRUE(ConvertTimeToString(last_modified_time, &now_string));
  ASSERT_TRUE(ConvertTimeToString(new_time, &new_time_string));

  fetcher_.SetConditionalResponse(url, last_modified_time, header, body);

  // Response is normal to an un-conditional GET.
  TestResponse(url, header, body);

  // Also normal for conditional GET with old time.
  {
    MockFetchContainer fetch(&fetcher_);
    fetch.request_headers_.Add(HttpAttributes::kIfModifiedSince,
                               old_time_string);
    EXPECT_TRUE(fetch.Fetch(url));
    EXPECT_EQ(header.ToString(), fetch.response_headers_.ToString());
    EXPECT_EQ(body, fetch.response_body_);
  }

  // But conditional GET with current time gets 304 Not Modified response.
  {
    MockFetchContainer fetch(&fetcher_);
    fetch.request_headers_.Add(HttpAttributes::kIfModifiedSince,
                               now_string);
    EXPECT_TRUE(fetch.Fetch(url));
    EXPECT_EQ(HttpStatus::kNotModified, fetch.response_headers_.status_code());
  }

  // Future time also gets 304 Not Modified.
  {
    MockFetchContainer fetch(&fetcher_);
    fetch.request_headers_.Add(HttpAttributes::kIfModifiedSince,
                               new_time_string);
    EXPECT_TRUE(fetch.Fetch(url));
    EXPECT_EQ(HttpStatus::kNotModified, fetch.response_headers_.status_code());
  }
}

TEST_F(MockUrlFetcherTest, UpdateHeaderDates) {
  MockTimer timer(MockTimer::kApr_5_2010_ms);
  fetcher_.set_timer(&timer);
  fetcher_.set_update_date_headers(true);

  const char url[] = "http://www.example.com/foo.css";
  const char body[] = "resource body";
  ResponseHeaders header;
  header.set_first_line(1, 1, 200, "OK");
  header.SetLastModified(MockTimer::kApr_5_2010_ms - 2 * Timer::kDayMs);
  const int64 ttl_ms = 5 * Timer::kMinuteMs;
  header.SetDateAndCaching(timer.NowMs(), ttl_ms);

  fetcher_.SetResponse(url, header, body);

  // Fetch it at current time.
  {
    MockFetchContainer fetch(&fetcher_);
    EXPECT_TRUE(fetch.Fetch(url));
    // Check that response header's expiration time is set correctly.
    EXPECT_EQ(timer.NowMs() + ttl_ms,
              fetch.response_headers_.CacheExpirationTimeMs());
  }

  // Fetch it at current time.
  timer.AdvanceMs(1 * Timer::kYearMs);  // Arbitrary time > 5min (max-age).
  {
    MockFetchContainer fetch(&fetcher_);
    EXPECT_TRUE(fetch.Fetch(url));
    // Check that response header's expiration time is set correctly.
    EXPECT_EQ(timer.NowMs() + ttl_ms,
              fetch.response_headers_.CacheExpirationTimeMs());
  }
}

}  // namespace

}  // namespace net_instaweb
