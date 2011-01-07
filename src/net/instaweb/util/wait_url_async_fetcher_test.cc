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

#include "net/instaweb/util/public/wait_url_async_fetcher.h"

#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/mock_url_fetcher.h"

namespace net_instaweb {

namespace {

// Callback that just checks that it was called with success.
class DummyCallback : public UrlAsyncFetcher::Callback {
 public:
  DummyCallback() : done_(false) {}
  virtual ~DummyCallback() {
    EXPECT_TRUE(done_);
  }
  virtual void Done(bool success) {
    EXPECT_FALSE(done_) << "Already Done; perhaps you reused without Reset()";
    done_ = true;
    EXPECT_TRUE(success);
  }
  bool done_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyCallback);
};

class WaitUrlAsyncFetcherTest : public ::testing::Test {};

TEST_F(WaitUrlAsyncFetcherTest, FetcherWaits) {
  MockUrlFetcher base_fetcher;
  WaitUrlAsyncFetcher wait_fetcher(&base_fetcher);

  const char url[] = "http://www.example.com/";
  ResponseHeaders header;
  header.set_first_line(1, 1, 200, "OK");
  const char body[] = "Contents.";

  base_fetcher.SetResponse(url, header, body);

  const RequestHeaders request_headers;
  ResponseHeaders response_headers;
  std::string response_body;
  StringWriter response_writer(&response_body);
  GoogleMessageHandler handler;
  DummyCallback callback;

  EXPECT_FALSE(wait_fetcher.StreamingFetch(url, request_headers,
                                           &response_headers, &response_writer,
                                           &handler, &callback));

  // Nothing gets set ...
  EXPECT_EQ(false, callback.done_);
  EXPECT_EQ("", response_body);

  // ... until we CallCallbacks.
  wait_fetcher.CallCallbacks();
  EXPECT_EQ(true, callback.done_);
  EXPECT_EQ(body, response_body);

}

}  // namespace

}  // namespace net_instaweb
