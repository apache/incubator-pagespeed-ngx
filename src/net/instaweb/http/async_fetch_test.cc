/*
 * Copyright 2013 Google Inc.
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

#include "net/instaweb/http/public/async_fetch.h"

#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/null_mutex.h"
#include "pagespeed/kernel/http/http_options.h"

namespace net_instaweb {
namespace {

const char kUrl[] = "http://www.example.com/";

class TestSharedAsyncFetch : public SharedAsyncFetch {
 public:
  explicit TestSharedAsyncFetch(AsyncFetch* base_fetch)
      : SharedAsyncFetch(base_fetch) {
  }

  virtual ~TestSharedAsyncFetch() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSharedAsyncFetch);
};

// Tests the AsyncFetch class and some of its derivations.
class AsyncFetchTest : public testing::Test {
 protected:
  AsyncFetchTest()
      : request_context_(new RequestContext(
            kDefaultHttpOptionsForTests, new NullMutex, NULL)),
        string_fetch_(request_context_),
        handler_(new NullMutex) {
  }

 protected:
  RequestContextPtr request_context_;
  StringAsyncFetch string_fetch_;
  HTTPValue fallback_value_;
  MockMessageHandler handler_;
};

TEST_F(AsyncFetchTest, LackOfContentLengthPropagatesToShared) {
  TestSharedAsyncFetch fetch(&string_fetch_);
  fetch.response_headers()->set_status_code(HttpStatus::kOK);
  EXPECT_FALSE(fetch.content_length_known());
  fetch.HeadersComplete();
  EXPECT_FALSE(string_fetch_.content_length_known());
}

TEST_F(AsyncFetchTest, ContentLengthPropagatesToShared) {
  TestSharedAsyncFetch fetch(&string_fetch_);
  fetch.response_headers()->set_status_code(HttpStatus::kOK);
  fetch.set_content_length(42);
  EXPECT_TRUE(fetch.content_length_known());
  fetch.HeadersComplete();
  EXPECT_TRUE(string_fetch_.content_length_known());
  EXPECT_EQ(42, string_fetch_.content_length());
}

TEST_F(AsyncFetchTest, LackOfContentLengthPropagatesToFallback) {
  FallbackSharedAsyncFetch fetch(&string_fetch_, &fallback_value_, &handler_);
  fetch.response_headers()->set_status_code(HttpStatus::kOK);
  EXPECT_FALSE(fetch.content_length_known());
  fetch.HeadersComplete();
  EXPECT_FALSE(string_fetch_.content_length_known());
}

TEST_F(AsyncFetchTest, ContentLengthPropagatesToFallback) {
  FallbackSharedAsyncFetch fetch(&string_fetch_, &fallback_value_, &handler_);
  fetch.response_headers()->set_status_code(HttpStatus::kOK);
  fetch.set_content_length(42);
  EXPECT_TRUE(fetch.content_length_known());
  fetch.HeadersComplete();
  EXPECT_TRUE(string_fetch_.content_length_known());
  EXPECT_EQ(42, string_fetch_.content_length());
}

TEST_F(AsyncFetchTest, LackOfContentLengthPropagatesToConditional) {
  ConditionalSharedAsyncFetch fetch(&string_fetch_, &fallback_value_,
                                    &handler_);
  fetch.response_headers()->set_status_code(HttpStatus::kOK);
  EXPECT_FALSE(fetch.content_length_known());
  fetch.HeadersComplete();
  EXPECT_FALSE(string_fetch_.content_length_known());
}

TEST_F(AsyncFetchTest, ContentLengthPropagatesToConditional) {
  ConditionalSharedAsyncFetch fetch(&string_fetch_, &fallback_value_,
                                    &handler_);
  fetch.response_headers()->set_status_code(HttpStatus::kOK);
  fetch.set_content_length(42);
  EXPECT_TRUE(fetch.content_length_known());
  fetch.HeadersComplete();
  EXPECT_TRUE(string_fetch_.content_length_known());
  EXPECT_EQ(42, string_fetch_.content_length());
}

}  // namespace

}  // namespace net_instaweb
