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

// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test the lru cache

#include "net/instaweb/http/public/http_value.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/http/public/response_headers.h"

namespace {
const int kMaxSize = 100;
}

namespace net_instaweb {

class HTTPValueTest : public testing::Test {
 protected:
  HTTPValueTest() { }

  void FillResponseHeaders(ResponseHeaders* meta_data) {
    meta_data->SetStatusAndReason(HttpStatus::kOK);
    meta_data->set_major_version(1);
    meta_data->set_minor_version(0);
    meta_data->set_reason_phrase("OK");
    meta_data->Add("Cache-control", "max-age=300");
  }

  void CheckResponseHeaders(const ResponseHeaders& meta_data) {
    ResponseHeaders expected;
    FillResponseHeaders(&expected);
    EXPECT_EQ(expected.ToString(), meta_data.ToString());
  }

  GoogleMessageHandler message_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HTTPValueTest);
};

TEST_F(HTTPValueTest, Empty) {
  HTTPValue value;
  EXPECT_TRUE(value.Empty());
}

TEST_F(HTTPValueTest, HeadersFirst) {
  HTTPValue value;
  ResponseHeaders headers, check_headers;
  FillResponseHeaders(&headers);
  value.SetHeaders(&headers);
  value.Write("body", &message_handler_);
  StringPiece body;
  ASSERT_TRUE(value.ExtractContents(&body));
  EXPECT_EQ("body", body.as_string());
  ASSERT_TRUE(value.ExtractHeaders(&check_headers, &message_handler_));
  CheckResponseHeaders(check_headers);
}

TEST_F(HTTPValueTest, ContentsFirst) {
  HTTPValue value;
  ResponseHeaders headers, check_headers;
  FillResponseHeaders(&headers);
  value.Write("body", &message_handler_);
  value.SetHeaders(&headers);
  StringPiece body;
  ASSERT_TRUE(value.ExtractContents(&body));
  EXPECT_EQ("body", body.as_string());
  ASSERT_TRUE(value.ExtractHeaders(&check_headers, &message_handler_));
  CheckResponseHeaders(check_headers);
}

TEST_F(HTTPValueTest, EmptyContentsFirst) {
  HTTPValue value;
  ResponseHeaders headers, check_headers;
  FillResponseHeaders(&headers);
  value.Write("", &message_handler_);
  value.SetHeaders(&headers);
  StringPiece body;
  ASSERT_TRUE(value.ExtractContents(&body));
  EXPECT_EQ("", body.as_string());
  ASSERT_TRUE(value.ExtractHeaders(&check_headers, &message_handler_));
  CheckResponseHeaders(check_headers);
}

TEST_F(HTTPValueTest, TestCopyOnWrite) {
  HTTPValue v1;
  v1.Write("Hello", &message_handler_);
  StringPiece v1_contents, v2_contents, v3_contents;
  ASSERT_TRUE(v1.ExtractContents(&v1_contents));
  EXPECT_TRUE(v1.unique());

  // Test Link sharing
  HTTPValue v2;
  v2.Link(&v1);
  EXPECT_FALSE(v1.unique());
  EXPECT_FALSE(v2.unique());
  ASSERT_TRUE(v2.ExtractContents(&v2_contents));
  EXPECT_EQ(v1_contents, v2_contents);
  EXPECT_EQ(v1_contents.data(), v2_contents.data());  // buffer sharing

  HTTPValue v3;
  v3.Link(&v1);
  EXPECT_FALSE(v3.unique());
  ASSERT_TRUE(v3.ExtractContents(&v3_contents));
  EXPECT_EQ(v1_contents, v3_contents);
  EXPECT_EQ(v1_contents.data(), v3_contents.data());  // buffer sharing

  // Now write something into v1.  Due to copy-on-write semantics, v2 and
  // will v3 not see it.
  v1.Write(", World!", &message_handler_);
  ASSERT_TRUE(v1.ExtractContents(&v1_contents));
  ASSERT_TRUE(v2.ExtractContents(&v2_contents));
  ASSERT_TRUE(v3.ExtractContents(&v3_contents));
  EXPECT_EQ("Hello, World!", v1_contents);
  EXPECT_NE(v1_contents, v2_contents);
  EXPECT_NE(v1_contents.data(), v2_contents.data());  // no buffer sharing
  EXPECT_NE(v1_contents, v3_contents);
  EXPECT_NE(v1_contents.data(), v3_contents.data());  // no buffer sharing

  // But v2 and v3 will remain connected to one another
  EXPECT_EQ(v2_contents, v3_contents);
  EXPECT_EQ(v2_contents.data(), v3_contents.data());  // buffer sharing
}

TEST_F(HTTPValueTest, TestShare) {
  SharedString storage;

  {
    HTTPValue value;
    ResponseHeaders headers, check_headers;
    FillResponseHeaders(&headers);
    value.SetHeaders(&headers);
    value.Write("body", &message_handler_);
    storage = *value.share();
  }

  {
    HTTPValue value;
    ResponseHeaders check_headers;
    ASSERT_TRUE(value.Link(&storage, &check_headers, &message_handler_));
    StringPiece body;
    ASSERT_TRUE(value.ExtractContents(&body));
    EXPECT_EQ("body", body.as_string());
    CheckResponseHeaders(check_headers);
  }
}

TEST_F(HTTPValueTest, LinkEmpty) {
  SharedString storage;
  HTTPValue value;
  ResponseHeaders headers;
  ASSERT_FALSE(value.Link(&storage, &headers, &message_handler_));
}

TEST_F(HTTPValueTest, LinkCorrupt) {
  SharedString storage("h");
  HTTPValue value;
  ResponseHeaders headers;
  ASSERT_FALSE(value.Link(&storage, &headers, &message_handler_));
  storage->append("9999");
  ASSERT_FALSE(value.Link(&storage, &headers, &message_handler_));
  storage->append("xyz");
  ASSERT_FALSE(value.Link(&storage, &headers, &message_handler_));
  *storage = "b";
  ASSERT_FALSE(value.Link(&storage, &headers, &message_handler_));
  storage->append("9999");
  ASSERT_FALSE(value.Link(&storage, &headers, &message_handler_));
  storage->append("xyz");
  ASSERT_FALSE(value.Link(&storage, &headers, &message_handler_));
}

}  // namespace net_instaweb
