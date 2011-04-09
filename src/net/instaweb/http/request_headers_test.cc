// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test SimpleUrlData, in particular it's HTTP header parser.

#include "net/instaweb/http/public/request_headers.h"
#include <algorithm>
#include "base/basictypes.h"
#include "base/logging.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

class RequestHeadersTest : public testing::Test {
 protected:

  RequestHeaders::Method CheckMethod(RequestHeaders::Method method) {
    request_headers_.set_method(method);
    return request_headers_.method();
  }

  const char* CheckMethodString(RequestHeaders::Method method) {
    request_headers_.set_method(method);
    return request_headers_.method_string();
  }

  GoogleMessageHandler message_handler_;
  RequestHeaders request_headers_;
};

TEST_F(RequestHeadersTest, TestMethods) {
  EXPECT_EQ(RequestHeaders::kOptions, CheckMethod(RequestHeaders::kOptions));
  EXPECT_EQ(RequestHeaders::kGet, CheckMethod(RequestHeaders::kGet));
  EXPECT_EQ(RequestHeaders::kHead, CheckMethod(RequestHeaders::kHead));
  EXPECT_EQ(RequestHeaders::kPost, CheckMethod(RequestHeaders::kPost));
  EXPECT_EQ(RequestHeaders::kPut, CheckMethod(RequestHeaders::kPut));
  EXPECT_EQ(RequestHeaders::kDelete, CheckMethod(RequestHeaders::kDelete));
  EXPECT_EQ(RequestHeaders::kTrace, CheckMethod(RequestHeaders::kTrace));
  EXPECT_EQ(RequestHeaders::kConnect, CheckMethod(RequestHeaders::kConnect));
}

TEST_F(RequestHeadersTest, TestMethodStrings) {
  EXPECT_STREQ("OPTIONS", CheckMethodString(RequestHeaders::kOptions));
  EXPECT_STREQ("GET", CheckMethodString(RequestHeaders::kGet));
  EXPECT_STREQ("HEAD", CheckMethodString(RequestHeaders::kHead));
  EXPECT_STREQ("POST", CheckMethodString(RequestHeaders::kPost));
  EXPECT_STREQ("PUT", CheckMethodString(RequestHeaders::kPut));
  EXPECT_STREQ("DELETE", CheckMethodString(RequestHeaders::kDelete));
  EXPECT_STREQ("TRACE", CheckMethodString(RequestHeaders::kTrace));
  EXPECT_STREQ("CONNECT", CheckMethodString(RequestHeaders::kConnect));
}

}  // namespace net_instaweb
