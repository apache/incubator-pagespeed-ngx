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
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"

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

  int64 ComputeContentsSize(HTTPValue* value) {
    return value->ComputeContentsSize();
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
  EXPECT_EQ(body.size(), ComputeContentsSize(&value));
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
  EXPECT_EQ(body.size(), ComputeContentsSize(&value));
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
  EXPECT_EQ(body.size(), ComputeContentsSize(&value));
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
  EXPECT_EQ(v1_contents.size(), ComputeContentsSize(&v1));
  EXPECT_EQ(v2_contents.size(), ComputeContentsSize(&v2));
  EXPECT_EQ(v3_contents.size(), ComputeContentsSize(&v3));
}

TEST_F(HTTPValueTest, TestShare) {
  SharedString storage;

  {
    HTTPValue value;
    ResponseHeaders headers, check_headers;
    FillResponseHeaders(&headers);
    value.SetHeaders(&headers);
    value.Write("body", &message_handler_);
    storage = value.share();
  }

  {
    HTTPValue value;
    ResponseHeaders check_headers;
    ASSERT_TRUE(value.Link(storage, &check_headers, &message_handler_));
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
  ASSERT_FALSE(value.Link(storage, &headers, &message_handler_));
}

TEST_F(HTTPValueTest, LinkCorrupt) {
  SharedString storage("h");
  HTTPValue value;
  ResponseHeaders headers;
  ASSERT_FALSE(value.Link(storage, &headers, &message_handler_));
  storage.Append("9999");
  ASSERT_FALSE(value.Link(storage, &headers, &message_handler_));
  storage.Append("xyz");
  ASSERT_FALSE(value.Link(storage, &headers, &message_handler_));
  storage.Assign("b");
  ASSERT_FALSE(value.Link(storage, &headers, &message_handler_));
  storage.Append("9999");
  ASSERT_FALSE(value.Link(storage, &headers, &message_handler_));
  storage.Append("xyz");
  ASSERT_FALSE(value.Link(storage, &headers, &message_handler_));
}

class HTTPValueEncodeTest : public testing::Test {
 public:
  GoogleString Decode(StringPiece in) {
    GoogleString out;
    EXPECT_TRUE(HTTPValue::Decode(in, &out, &handler_));
    return out;
  }

  GoogleString Encode(StringPiece in) {
    GoogleString out;
    EXPECT_TRUE(HTTPValue::Encode(in, &out, &handler_));
    return out;
  }

  GoogleMessageHandler handler_;
};

TEST_F(HTTPValueEncodeTest, EncodeDecode) {
  const char simple_http[] =
      "HTTP/1.1 200 OK\r\n"
      "Host: www.example.com\r\n"
      "\r\n"
      "Hello, world!";
  EXPECT_STREQ(simple_http, Decode(Encode(simple_http)));

  const char error_http[] =
      "HTTP/1.0 0 Internal Server Error\r\n"
      "\r\n";
  EXPECT_STREQ(error_http, Decode(Encode(error_http)));

  const char complex_http[] =
      "HTTP/1.1 200 OK\r\n"
      "Server: nginx/0.5.26\r\n"
      "Date: Tue, 29 Nov 2011 16:21:28 GMT\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n"
      "Connection: keep-alive\r\n"
      "X-Powered-By: PHP/5.2.3-1ubuntu6.5\r\n"
      "Set-Cookie: magento=gv8gxips44qykg76kgwyosgagsk1hl1g; expires=Thu, "
      "29-Dec-2011 16:21:28 GMT; path=/; domain=www.toysdownunder.com\r\n"
      "Set-Cookie: frontend=9bbc4bf255ec10d66245a02b3dda5ba4; expires=Thu, "
      "08 Dec 2011 00:21:28 GMT; path=/; domain=www.toysdownunder.com\r\n"
      "Expires: Thu, 19 Nov 1981 08:52:00 GMT\r\n"
      "Cache-Control: no-store, no-cache, must-revalidate, post-check=0, "
      "pre-check=0\r\n"
      "Pragma: no-cache\r\n"
      "X-Google-Cache-Control: remote-fetch\r\n"
      "Via: HTTP/1.1 GWA\r\n"
      "\r\n"
      "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "
      "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n"
      "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" "
      "lang=\"en\">\n"
      "<head>\n"
      "    <meta http-equiv=\"X-UA-Compatible\" content=\"IE=8\" />\n"
      "<title> Toysdownunder.com  - Arduino and Walkera Helicopters </title>\n"
      "<meta http-equiv=\"Content-Type\" content=\"text/html; "
      "charset=utf-8\" />\n"
      "<meta name=\"verify-v1\" content=\"7sZcArzfEwR1uQyfxrhn4AdJnOcN6OlXf"
      "666LZYnC94=\" />\n";
  EXPECT_STREQ(complex_http, Decode(Encode(complex_http)));
}

TEST_F(HTTPValueEncodeTest, EncodeDecodeGold) {
  const char example_http[] =
      "HTTP/1.1 200 OK\r\n"
      "Server: Apache/2.2.29 (Unix) mod_ssl/2.2.29 OpenSSL/1.0.1j DAV/2 "
      "mod_fcgid/2.3.9\r\n"
      "Last-Modified: Fri, 20 Feb 2015 18:10:04 GMT\r\n"
      "Accept-Ranges: bytes\r\n"
      "Content-Length: 21\r\n"
      "X-Extra-Header: 1\r\n"
      "Cache-Control: public, max-age=600\r\n"
      "Content-Type: text/css\r\n"
      "Etag: W/\"PSA-35DPOkCBal\"\r\n"
      "Date: Fri, 15 May 2015 21:40:32 GMT\r\n"
      "\r\n"
      ".blue {color: blue;}\n";

  const char header_first_golden_value_buf[] =
      "hv\x1\0\0\b\xC8\x1\x12\x2OK\x18\x1 \x1(\xC0\xD8\xBA\xCC\xD5)0\x80\x89"
      "\x96\xCC\xD5)8\x1@\x1JR\n\x6"
      "Server\x12HApache/2.2.29 (Unix) mod_ssl/2.2.29 OpenSSL/1.0.1j DAV/2"
      " mod_fcgid/2.3.9J.\n\r"
      "Last-Modified\x12\x1D" "Fri, 20 Feb 2015 18:10:04 GMTJ\x16\n\r"
      "Accept-Ranges\x12\x5" "bytesJ\x14\n\xE"
      "Content-Length\x12\x2" "21J\x13\n\xE"
      "X-Extra-Header\x12\x1" "1J$\n\r"
      "Cache-Control\x12\x13public, max-age=600J\x18\n\f"
      "Content-Type\x12\btext/cssJ\x1A\n\x4"
      "Etag\x12\x12W/\"PSA-35DPOkCBal\"J%\n\x4"
      "Date\x12\x1D" "Fri, 15 May 2015 21:40:32 GMTP"
      "\xE0\xC8\xBA\xC1\xBA)X\xC0\xCF$h\0p\0."
      "blue {color: blue;}\n";
  StringPiece header_first_golden_value(
      header_first_golden_value_buf,
      STATIC_STRLEN(header_first_golden_value_buf));

  const char body_first_golden_value_buf[] =
      "b\x15\0\0\0.blue {color: blue;}\n\b\xC8\x1\x12\x2OK\x18\x1 \x1(\xC0\xD8"
      "\xBA\xCC\xD5)0\x80\x89\x96\xCC\xD5)8\x1@\x1JR\n\x6"
      "Server\x12HApache/2.2.29 (Unix) mod_ssl/2.2.29 OpenSSL/1.0.1j DAV/2"
      " mod_fcgid/2.3.9J.\n\r"
      "Last-Modified\x12\x1D" "Fri, 20 Feb 2015 18:10:04 GMTJ\x16\n\r"
      "Accept-Ranges\x12\x5" "bytesJ\x14\n\xE"
      "Content-Length\x12\x2" "21J\x13\n\xE"
      "X-Extra-Header\x12\x1" "1J$\n\r"
      "Cache-Control\x12\x13public, max-age=600J\x18\n\f"
      "Content-Type\x12\btext/cssJ\x1A\n\x4"
      "Etag\x12\x12W/\"PSA-35DPOkCBal\"J%\n\x4"
      "Date\x12\x1D" "Fri, 15 May 2015 21:40:32 GMTX\xC0\xCF$h\0p\0";
  StringPiece body_first_golden_value(
      body_first_golden_value_buf, STATIC_STRLEN(body_first_golden_value_buf));

  // These tests should work even if proto formats change.
  EXPECT_STREQ(example_http, Decode(header_first_golden_value));
  EXPECT_STREQ(example_http, Decode(body_first_golden_value));

  // Note: This might change when proto formats change.
  // Note: Can't use STREQ, it doesn't check past embedded nulls.
  EXPECT_EQ(header_first_golden_value, Encode(example_http));
}

TEST_F(HTTPValueEncodeTest, EncodeInvalid) {
  GoogleString out;
  EXPECT_FALSE(HTTPValue::Decode("invalid encoding", &out, &handler_));
  EXPECT_FALSE(HTTPValue::Encode("invalid http", &out, &handler_));
}

}  // namespace net_instaweb
