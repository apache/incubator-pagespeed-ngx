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

// Unit-test the string-splitter.

#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class ContentTypeTest : public testing::Test {
 protected:
  ContentType::Type ExtToType(const char* ext) {
    return NameExtensionToContentType(ext)->type();
  }
  ContentType::Type MimeToType(const char* mime_type) {
    return MimeTypeToContentType(mime_type)->type();
  }
};

TEST_F(ContentTypeTest, TestUnknown) {
  EXPECT_EQ(NULL, NameExtensionToContentType(".unknown"));
  EXPECT_EQ(NULL, MimeTypeToContentType("unknown/unknown"));
}

TEST_F(ContentTypeTest, TestExtensions) {
  EXPECT_EQ(ContentType::kHtml,       ExtToType(".html"));
  EXPECT_EQ(ContentType::kJavascript, ExtToType(".js"));
  EXPECT_EQ(ContentType::kJpeg,       ExtToType(".jpg"));
  EXPECT_EQ(ContentType::kJpeg,       ExtToType(".jpeg"));
  EXPECT_EQ(ContentType::kCss,        ExtToType(".css"));
  EXPECT_EQ(ContentType::kText,       ExtToType(".txt"));
  EXPECT_EQ(ContentType::kXml,        ExtToType(".xml"));
  EXPECT_EQ(ContentType::kPng,        ExtToType(".png"));
  EXPECT_EQ(ContentType::kGif,        ExtToType(".gif"));
}

TEST_F(ContentTypeTest, TestMimeType) {
  EXPECT_EQ(ContentType::kHtml,       MimeToType("text/html"));
  EXPECT_EQ(ContentType::kHtml,       MimeToType("text/html; charset=UTF-8"));
  EXPECT_EQ(ContentType::kXhtml,      MimeToType("application/xhtml+xml"));
  EXPECT_EQ(ContentType::kCeHtml,     MimeToType("application/ce-html+xml"));
  EXPECT_EQ(ContentType::kJavascript, MimeToType("text/javascript"));
  EXPECT_EQ(ContentType::kJavascript, MimeToType("application/x-javascript"));
  EXPECT_EQ(ContentType::kJavascript, MimeToType("application/javascript"));
  EXPECT_EQ(ContentType::kJavascript, MimeToType("text/ecmascript"));
  EXPECT_EQ(ContentType::kJavascript, MimeToType("application/ecmascript"));
  EXPECT_EQ(ContentType::kJpeg,       MimeToType("image/jpeg"));
  EXPECT_EQ(ContentType::kCss,        MimeToType("text/css"));
  EXPECT_EQ(ContentType::kText,       MimeToType("text/plain"));
  EXPECT_EQ(ContentType::kXml,        MimeToType("application/xml"));
  EXPECT_EQ(ContentType::kXml,        MimeToType("text/xml"));
  EXPECT_EQ(ContentType::kPng,        MimeToType("image/png"));
  EXPECT_EQ(ContentType::kGif,        MimeToType("image/gif"));
}

}  // namespace net_instaweb
