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

#include "net/instaweb/util/fetcher_test.h"

namespace net_instaweb {

const char FetcherTest::kStartDate[] = "Sun, 16 Dec 1979 02:27:45 GMT";
const char FetcherTest::kHtmlContent[] = "<html><body>Nuts!</body></html>";
const char FetcherTest::kErrorMessage[] = "Invalid URL";
const char FetcherTest::kGoodUrl[] = "http://pi.com";
const char FetcherTest::kNotCachedUrl[] = "http://not_cacheable.com";
const char FetcherTest::kBadUrl[] = "http://this_url_will_fail.com";
const char FetcherTest::kHeaderName[] = "header-name";
const char FetcherTest::kHeaderValue[] = "header value";

void FetcherTest::ValidateMockFetcherResponse(
    bool success, bool check_error_message,
    const std::string& content,
    const MetaData& response_headers) {
  if (success) {
    EXPECT_EQ(std::string(kHtmlContent), content);
    CharStarVector values;
    EXPECT_TRUE(response_headers.Lookup(kHeaderName, &values));
    EXPECT_EQ(1, values.size());
    EXPECT_EQ(std::string(kHeaderValue), values[0]);
  } else if (check_error_message) {
    EXPECT_EQ(std::string(kErrorMessage), content);
  }
}

int FetcherTest::CountFetchesSync(const StringPiece& url, bool expect_success,
                                  bool check_error_message) {
  CHECK(sync_fetcher() != NULL);
  return CountFetchesSync(url, sync_fetcher(),
                          expect_success, check_error_message);
}

int FetcherTest::CountFetchesSync(
    const StringPiece& url, UrlFetcher* fetcher,
    bool expect_success, bool check_error_message) {
  int starting_fetches = mock_fetcher_.num_fetches();
  std::string content;
  StringWriter content_writer(&content);
  SimpleMetaData request_headers, response_headers;
  bool success = fetcher->StreamingFetchUrl(
      url.as_string(), request_headers, &response_headers, &content_writer,
      &message_handler_);
  EXPECT_EQ(expect_success, success);
  ValidateMockFetcherResponse(success, check_error_message, content,
                              response_headers);
  return mock_fetcher_.num_fetches() - starting_fetches;
}

int FetcherTest::CountFetchesAsync(const StringPiece& url, bool expect_success,
                                   bool* callback_called) {
  CHECK(async_fetcher() != NULL);
  *callback_called = false;
  int starting_fetches = mock_fetcher_.num_fetches();
  SimpleMetaData request_headers;
  CheckCallback* fetch = new CheckCallback(expect_success, callback_called);
  async_fetcher()->StreamingFetch(
      url.as_string(), request_headers, &fetch->response_headers_,
      &fetch->content_writer_, &message_handler_, fetch);
  return mock_fetcher_.num_fetches() - starting_fetches;
}


void FetcherTest::ValidateOutput(const std::string& content,
                                 const MetaData& response_headers) {
  // The detailed header parsing code is tested in
  // simple_meta_data_test.cc.  But let's check the rseponse code
  // and the last header here, and make sure we got the content.
  EXPECT_EQ(200, response_headers.status_code());
  EXPECT_EQ(15, response_headers.NumAttributes());
  EXPECT_EQ(std::string("X-Google-GFE-Response-Body-Transformations"),
            std::string(response_headers.Name(14)));
  EXPECT_EQ(std::string("gunzipped"),
            std::string(response_headers.Value(14)));

  // Verifies that after the headers, we see the content.  Note that this
  // currently assumes 'wget' style output.  Wget takes care of any unzipping.
  static const char start_of_doc[] = "<!doctype html>";
  EXPECT_EQ(0, strncmp(start_of_doc, content.c_str(),
                       sizeof(start_of_doc) - 1));
}


// MockFetcher
bool FetcherTest::MockFetcher::StreamingFetchUrl(
    const std::string& url,
    const MetaData& request_headers,
    MetaData* response_headers,
    Writer* writer,
    MessageHandler* message_handler) {
  bool ret = false;
  if (url == kGoodUrl) {
    ret = Populate("public, max-age=300", response_headers, writer,
                   message_handler);
  } else if (url == kNotCachedUrl) {
    ret = Populate("no-cache", response_headers, writer,
                   message_handler);
  } else {
    writer->Write(kErrorMessage, message_handler);
  }
  ++num_fetches_;
  return ret;
}

bool FetcherTest::MockFetcher::Populate(const char* cache_control,
                                        MetaData* response_headers,
                                        Writer* writer,
                                        MessageHandler* message_handler) {
  response_headers->set_status_code(HttpStatus::kOK);
  response_headers->Add(HttpAttributes::kCacheControl, cache_control);
  response_headers->Add("Date", kStartDate);
  response_headers->Add(kHeaderName, kHeaderValue);
  response_headers->ComputeCaching();
  response_headers->set_headers_complete(true);
  writer->Write(kHtmlContent, message_handler);
  return true;
}


// MockAsyncFetcher
bool FetcherTest::MockAsyncFetcher::StreamingFetch(
    const std::string& url,
    const MetaData& request_headers,
    MetaData* response_headers,
    Writer* writer,
    MessageHandler* handler,
    Callback* callback) {
  bool status = url_fetcher_->StreamingFetchUrl(
      url, request_headers, response_headers, writer, handler);
  deferred_callbacks_.push_back(std::make_pair(status, callback));
  return false;
}

void FetcherTest::MockAsyncFetcher::CallCallbacks() {
  for (int i = 0, n = deferred_callbacks_.size(); i < n; ++i) {
    bool status = deferred_callbacks_[i].first;
    Callback* callback = deferred_callbacks_[i].second;
    callback->Done(status);
  }
  deferred_callbacks_.clear();
}

}  // namespace net_isntaweb
