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

#include "net/instaweb/http/public/fetcher_test.h"

#include "base/logging.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/null_mutex.h"
#include "net/instaweb/util/public/platform.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

const char FetcherTest::kStartDate[] = "Sun, 16 Dec 1979 02:27:45 GMT";
const char FetcherTest::kHtmlContent[] = "<html><body>Nuts!</body></html>";
const char FetcherTest::kErrorMessage[] = "Invalid URL";
const char FetcherTest::kGoodUrl[] = "http://pi.com";
const char FetcherTest::kNotCachedUrl[] = "http://not_cacheable.com";
const char FetcherTest::kBadUrl[] = "http://this_url_will_fail.com";
const char FetcherTest::kHeaderName[] = "header-name";
const char FetcherTest::kHeaderValue[] = "header value";

SimpleStats* FetcherTest::statistics_ = NULL;

FetcherTest::FetcherTest()
    : wait_url_async_fetcher_(&mock_fetcher_, new NullMutex),
      counting_fetcher_(&wait_url_async_fetcher_),
      thread_system_(Platform::CreateThreadSystem()) {
  mock_fetcher_.set_fail_on_unexpected(false);
  mock_fetcher_.set_error_message(kErrorMessage);

  ResponseHeaders good_headers, no_cache_headers;
  GoogleString good_content, no_cache_content;

  Populate("max-age=300", &good_headers, &good_content);
  Populate("no-cache", &no_cache_headers, &no_cache_content);
  mock_fetcher_.SetResponse(kGoodUrl, good_headers, good_content);
  mock_fetcher_.SetResponse(kNotCachedUrl, no_cache_headers, no_cache_content);
}

void FetcherTest::ValidateMockFetcherResponse(
    bool success, bool check_error_message,
    const GoogleString& content,
    const ResponseHeaders& response_headers) {
  if (success) {
    EXPECT_EQ(GoogleString(kHtmlContent), content);
    ConstStringStarVector values;
    EXPECT_TRUE(response_headers.Lookup(kHeaderName, &values));
    EXPECT_EQ(1, values.size());
    EXPECT_EQ(GoogleString(kHeaderValue), *(values[0]));
  } else if (check_error_message) {
    EXPECT_EQ(GoogleString(kErrorMessage), content);
  }
}

int FetcherTest::CountFetchesAsync(
    const StringPiece& url, bool expect_success, bool* callback_called) {
  return CountFetchesAsync(url, async_fetcher(),
                           expect_success, true, callback_called);
}

int FetcherTest::CountFetchesAsync(
    const StringPiece& url, UrlAsyncFetcher* fetcher,
    bool expect_success, bool check_error_message, bool* callback_called) {
  CHECK(fetcher != NULL);
  *callback_called = false;
  int starting_fetches = counting_fetcher_.fetch_start_count();
  CheckCallback* fetch = new CheckCallback(
      RequestContext::NewTestRequestContext(thread_system_.get()),
      expect_success, check_error_message, callback_called);
  fetcher->Fetch(url.as_string(), &message_handler_, fetch);
  return counting_fetcher_.fetch_start_count() - starting_fetches;
}

void FetcherTest::ValidateOutput(const GoogleString& content,
                                 const ResponseHeaders& response_headers) {
  // The detailed header parsing code is tested in
  // simple_meta_data_test.cc.  But let's check the response code
  // and the last header here, and make sure we got the content.
  EXPECT_EQ(200, response_headers.status_code());
  ASSERT_EQ(11, response_headers.NumAttributes());
  EXPECT_EQ(GoogleString("P3P"),
            GoogleString(response_headers.Name(6)));
  EXPECT_STREQ(
      "CP=\"This is not a P3P policy! See http://www.google.com/support/"
      "accounts/bin/answer.py?hl=en&answer=151657 for more info.\"",
      response_headers.Value(6));

  // Verifies that after the headers, we see the content.  Note that this
  // currently assumes 'wget' style output.  Wget takes care of any unzipping.
  static const char start_of_doc[] = "<!doctype html>";
  EXPECT_EQ(0, strncmp(start_of_doc, content.c_str(),
                       STATIC_STRLEN(start_of_doc)));
}

void FetcherTest::Populate(const char* cache_control,
                           ResponseHeaders* response_headers,
                           GoogleString* content) {
  response_headers->SetStatusAndReason(HttpStatus::kOK);
  response_headers->set_major_version(1);
  response_headers->set_minor_version(1);
  response_headers->Add(HttpAttributes::kCacheControl, cache_control);
  response_headers->Add(HttpAttributes::kDate, kStartDate);
  response_headers->Add(kHeaderName, kHeaderValue);
  response_headers->ComputeCaching();
  *content = kHtmlContent;
}

void FetcherTest::SetUpTestCase() {
  statistics_ = new SimpleStats;
  HTTPCache::InitStats(statistics_);
}

void FetcherTest::TearDownTestCase() {
  delete statistics_;
  statistics_ = NULL;
}

}  // namespace net_instaweb
