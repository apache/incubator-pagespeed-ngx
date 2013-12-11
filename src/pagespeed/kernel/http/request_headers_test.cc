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

#include "pagespeed/kernel/http/request_headers.h"

#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http.pb.h"
#include "pagespeed/kernel/http/http_names.h"

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
  EXPECT_EQ(RequestHeaders::kPurge, CheckMethod(RequestHeaders::kPurge));
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
  EXPECT_STREQ("PURGE", CheckMethodString(RequestHeaders::kPurge));
}

TEST_F(RequestHeadersTest, RemoveAllWithPrefix) {
  request_headers_.Add("Prefix-1", "val");
  request_headers_.Add("PreFIX-2", "val");
  request_headers_.Add("prefix-3", "val");
  request_headers_.Add("something-4", "val");
  request_headers_.RemoveAllWithPrefix("Prefix");
  ASSERT_EQ(1, request_headers_.NumAttributes());
  EXPECT_STREQ("something-4", request_headers_.Name(0));
  EXPECT_STREQ("val", request_headers_.Value(0));
}

TEST_F(RequestHeadersTest, CopyFromProto) {
  request_headers_.Add("A", "1");
  ASSERT_EQ(1, request_headers_.NumAttributes());
  request_headers_.set_method(RequestHeaders::kPut);

  HttpRequestHeaders proto;
  NameValue* p = proto.add_header();
  p->set_name("B");
  p->set_value("2");
  request_headers_.CopyFromProto(proto);

  ASSERT_EQ(1, request_headers_.NumAttributes());
  EXPECT_STREQ("B", request_headers_.Name(0));
  EXPECT_STREQ("2", request_headers_.Value(0));
  // Default in proto.
  EXPECT_EQ(RequestHeaders::kGet, request_headers_.method());
}

TEST_F(RequestHeadersTest, AcceptWebp) {
  const StringPiece kWebpMimeType = kContentTypeWebp.mime_type();
  EXPECT_FALSE(request_headers_.HasValue(HttpAttributes::kAccept,
                                         kWebpMimeType));
  request_headers_.Add(HttpAttributes::kAccept, "x, image/webp, y");
  EXPECT_TRUE(request_headers_.HasValue(HttpAttributes::kAccept,
                                         kWebpMimeType));
  RequestHeaders keep;
  keep.Add(HttpAttributes::kAccept, "image/webp");
  keep.Add(HttpAttributes::kAccept, "y");
  EXPECT_TRUE(request_headers_.RemoveIfNotIn(keep));
  EXPECT_STREQ("image/webp, y", request_headers_.Value(0));

  request_headers_.Clear();
  EXPECT_FALSE(request_headers_.HasValue(HttpAttributes::kAccept,
                                         kWebpMimeType));
  request_headers_.Add(HttpAttributes::kAccept, "a");
  request_headers_.Add(HttpAttributes::kAccept, "image/webp");
  request_headers_.Add(HttpAttributes::kAccept, "b");
  EXPECT_TRUE(request_headers_.HasValue(HttpAttributes::kAccept,
                                        kWebpMimeType));
  // Add extra copy of image/webp.
  request_headers_.Add(HttpAttributes::kAccept, "image/webp");
  EXPECT_TRUE(request_headers_.HasValue(HttpAttributes::kAccept,
                                        kWebpMimeType));
  // remove just one of the two copies of the value.
  request_headers_.Remove(HttpAttributes::kAccept, "image/webp");
  EXPECT_TRUE(request_headers_.HasValue(HttpAttributes::kAccept,
                                        kWebpMimeType));
  // remove the last copy.
  request_headers_.Remove(HttpAttributes::kAccept, "image/webp");
  EXPECT_FALSE(request_headers_.HasValue(HttpAttributes::kAccept,
                                        kWebpMimeType));

  request_headers_.Clear();
  request_headers_.Add(HttpAttributes::kAccept,
                       "application/xhtml+xml,application/xml;q=0.9,"
                       "image/webp,*/*;q=0.8");
  EXPECT_TRUE(request_headers_.HasValue(HttpAttributes::kAccept,
                                        kWebpMimeType));

  // We do not currently handle arbitrary modifiers after image/webp.
  // If this becomes an issue in the future then this test should be
  // flipped to an EXPECT_TRUE once the handling is added.
  request_headers_.Clear();
  request_headers_.Add(HttpAttributes::kAccept,
                       "application/xhtml+xml,application/xml;q=0.9,"
                       "image/webp;q=0.9,"
                       "*/*;q=0.8");
  EXPECT_FALSE(request_headers_.HasValue(HttpAttributes::kAccept,
                                         kWebpMimeType));
}

}  // namespace net_instaweb
