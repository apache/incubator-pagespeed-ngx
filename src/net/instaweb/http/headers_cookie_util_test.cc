/*
 * Copyright 2012 Google Inc.
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

// Author: bharathbhushan@google.com (Bharath Bhushan)

// Unit tests for remove cookie operations.

#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class HeadersCookieUtilTest : public ::testing::Test {
};

TEST_F(HeadersCookieUtilTest, OnlyOne) {
  RequestHeaders headers;
  headers.Add(HttpAttributes::kCookie, "_GFURIOUS=1");

  headers.RemoveCookie("_GFURIOUS");

  const char kExpectedHeaders[] =
      "GET  HTTP/1.0\r\n"
      "\r\n";
  EXPECT_EQ(kExpectedHeaders, headers.ToString());
}

TEST_F(HeadersCookieUtilTest, OnlyUnrelatedCookies_1) {
  RequestHeaders headers;
  headers.Add(HttpAttributes::kCookie, "A=1");

  headers.RemoveCookie("_GFURIOUS");

  const char kExpectedHeaders[] =
      "GET  HTTP/1.0\r\n"
      "Cookie: A=1\r\n"
      "\r\n";
  EXPECT_EQ(kExpectedHeaders, headers.ToString());
}

TEST_F(HeadersCookieUtilTest, OnlyUnrelatedCookies_2) {
  RequestHeaders headers;
  headers.Add(HttpAttributes::kCookie, "A=1;  B=2;  C; D  ; E = ; F");

  headers.RemoveCookie("_GFURIOUS");

  const char kExpectedHeaders[] =
      "GET  HTTP/1.0\r\n"
      "Cookie: A=1;  B=2;  C; D  ; E = ; F\r\n"
      "\r\n";
  EXPECT_EQ(kExpectedHeaders, headers.ToString());
}

TEST_F(HeadersCookieUtilTest, OnlyOneWithUnrelatedCookie) {
  RequestHeaders headers;
  headers.Add(HttpAttributes::kCookie, "_GFURIOUS=1; B=2");

  headers.RemoveCookie("_GFURIOUS");

  const char kExpectedHeaders[] =
      "GET  HTTP/1.0\r\n"
      "Cookie: B=2\r\n"
      "\r\n";
  EXPECT_EQ(kExpectedHeaders, headers.ToString());
}

TEST_F(HeadersCookieUtilTest, OnlyOneAtEndWithUnrelatedCookie) {
  RequestHeaders headers;
  headers.Add(HttpAttributes::kCookie, "A=1; _GFURIOUS=1");

  headers.RemoveCookie("_GFURIOUS");

  const char kExpectedHeaders[] =
      "GET  HTTP/1.0\r\n"
      "Cookie: A=1\r\n"
      "\r\n";
  EXPECT_EQ(kExpectedHeaders, headers.ToString());
}

TEST_F(HeadersCookieUtilTest, MultipleInOneLine) {
  RequestHeaders headers;
  headers.Add(HttpAttributes::kCookie, "_GFURIOUS=1; _GFURIOUS=1; _GFURIOUS=1");

  headers.RemoveCookie("_GFURIOUS");

  const char kExpectedHeaders[] =
      "GET  HTTP/1.0\r\n"
      "\r\n";
  EXPECT_EQ(kExpectedHeaders, headers.ToString());
}

TEST_F(HeadersCookieUtilTest, MultipleInOneLineWithUnrelatedCookie) {
  RequestHeaders headers;
  headers.Add(HttpAttributes::kCookie,
              "A=1; _GFURIOUS=1; B=2; _GFURIOUS=1; C=3; _GFURIOUS=1");

  headers.RemoveCookie("_GFURIOUS");

  const char kExpectedHeaders[] =
      "GET  HTTP/1.0\r\n"
      "Cookie: A=1; B=2; C=3\r\n"
      "\r\n";
  EXPECT_EQ(kExpectedHeaders, headers.ToString());
}

TEST_F(HeadersCookieUtilTest, RemovePreviewCookie) {
  RequestHeaders headers;
  headers.Add(HttpAttributes::kCookie, "_GFURIOUS=1; B=2; C=3");
  headers.Add(HttpAttributes::kCookie, "A=x; _GFURIOUS=1; B=2; C=3");
  headers.Add(HttpAttributes::kCookie, "A=x; B=2; C=3;     _GFURIOUS=2");
  headers.Add(HttpAttributes::kCookie, "_GFURIOUS=1");
  headers.Add(HttpAttributes::kCookie, "    _GFURIOUS=1    ");
  headers.Add(HttpAttributes::kCookie, "A=b");
  headers.Add(HttpAttributes::kCookie, "    A=b; _GFURIOUS=");
  headers.Add(HttpAttributes::kCookie,
              "_GFURIOUS=1; _GFURIOUS=2; _GFURIOUS=3; A=1; _GFURIOUS=4;");

  headers.RemoveCookie("_GFURIOUS");

  const char kExpectedHeaders[] =
      "GET  HTTP/1.0\r\n"
      "Cookie: B=2; C=3\r\n"
      "Cookie: A=x; B=2; C=3\r\n"
      "Cookie: A=x; B=2; C=3\r\n"
      "Cookie: A=b\r\n"
      "Cookie: A=b\r\n"
      "Cookie: A=1\r\n"
      "\r\n";
  EXPECT_EQ(kExpectedHeaders, headers.ToString());
}

TEST_F(HeadersCookieUtilTest, InvalidCase_1) {
  RequestHeaders headers;
  headers.Add(HttpAttributes::kCookie,
              "A; _GFURIOUS=1;");

  headers.RemoveCookie("_GFURIOUS");

  const char kExpectedHeaders[] =
      "GET  HTTP/1.0\r\n"
      "Cookie: A\r\n"
      "\r\n";
  EXPECT_EQ(kExpectedHeaders, headers.ToString());
}

TEST_F(HeadersCookieUtilTest, InvalidCase_2) {
  RequestHeaders headers;
  headers.Add(HttpAttributes::kCookie,
              "A=1; B _GFURIOUS=1;");

  headers.RemoveCookie("_GFURIOUS");

  const char kExpectedHeaders[] =
      "GET  HTTP/1.0\r\n"
      "Cookie: A=1; B _GFURIOUS=1;\r\n"
      "\r\n";
  EXPECT_EQ(kExpectedHeaders, headers.ToString());
}

TEST_F(HeadersCookieUtilTest, InvalidCase_3) {
  RequestHeaders headers;
  headers.Add(HttpAttributes::kCookie,
              "A=1; _GFURIOUS=xyz 1;");

  headers.RemoveCookie("_GFURIOUS");

  const char kExpectedHeaders[] =
      "GET  HTTP/1.0\r\n"
      "Cookie: A=1\r\n"
      "\r\n";
  EXPECT_EQ(kExpectedHeaders, headers.ToString());
}

TEST_F(HeadersCookieUtilTest, QuotedValues) {
  RequestHeaders headers;
  headers.Add(HttpAttributes::kCookie, "A=\"12;23;\"");

  headers.RemoveCookie("_GFURIOUS");

  const char kExpectedHeaders[] =
      "GET  HTTP/1.0\r\n"
      "Cookie: A=\"12;23;\"\r\n"
      "\r\n";
  EXPECT_EQ(kExpectedHeaders, headers.ToString());
}

// This test case illustrates a flaw in our implementation. Quoted cookie values
// are not treated as one token because of which we end up with broken cookie
// values.
// TODO(nforman): Fix this.
TEST_F(HeadersCookieUtilTest, QuotedValues_BrokenCase) {
  RequestHeaders headers;
  headers.Add(HttpAttributes::kCookie, "_GFURIOUS=\"12;23;\"");

  headers.RemoveCookie("_GFURIOUS");

  const char kExpectedHeaders[] =
      "GET  HTTP/1.0\r\n"
      "Cookie: 23;\"\r\n"
      "\r\n";
  EXPECT_EQ(kExpectedHeaders, headers.ToString());
}

TEST_F(HeadersCookieUtilTest, QuotedValues_BrokenCase_2) {
  RequestHeaders headers;
  GoogleString header_string = "X_GFURIOUS=1; A=\"_B_GFURIOUS\"";
  headers.Add(HttpAttributes::kCookie, header_string);

  headers.RemoveCookie("_GFURIOUS");

  const char kExpectedHeadersFormat[] =
      "GET  HTTP/1.0\r\n"
      "Cookie: %s"
      "\r\n\r\n";
  GoogleString expected_headers = StringPrintf(kExpectedHeadersFormat,
                                               header_string.c_str());
  EXPECT_EQ(expected_headers, headers.ToString());
}

}  // namespace net_instaweb
