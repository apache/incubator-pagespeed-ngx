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

#include "net/instaweb/util/public/url_escaper.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_multipart_encoder.h"

namespace net_instaweb {

class UrlMultipartEncoderTest : public testing::Test {
 protected:
  UrlMultipartEncoder encoder_;
  GoogleMessageHandler handler_;
};

TEST_F(UrlMultipartEncoderTest, EscapeSeparatorsAndEscapes) {
  encoder_.AddUrl("abc");
  encoder_.AddUrl("def");
  encoder_.AddUrl("a=b+c");  // escape and separate characters
  std::string encoding = encoder_.Encode();
  encoder_.clear();
  ASSERT_TRUE(encoder_.Decode(encoding, &handler_));
  ASSERT_EQ(3, encoder_.num_urls());
  EXPECT_EQ(std::string("abc"), encoder_.url(0));
  EXPECT_EQ(std::string("def"), encoder_.url(1));
  EXPECT_EQ(std::string("a=b+c"), encoder_.url(2));
}

TEST_F(UrlMultipartEncoderTest, Empty) {
  ASSERT_TRUE(encoder_.Decode("", &handler_));
  EXPECT_EQ(0, encoder_.num_urls());
}

TEST_F(UrlMultipartEncoderTest, LastIsEmpty) {
  ASSERT_TRUE(encoder_.Decode("a+b+", &handler_));
  ASSERT_EQ(3, encoder_.num_urls());
  EXPECT_EQ(std::string("a"), encoder_.url(0));
  EXPECT_EQ(std::string("b"), encoder_.url(1));
  EXPECT_EQ(std::string(""), encoder_.url(2));
}

TEST_F(UrlMultipartEncoderTest, One) {
  ASSERT_TRUE(encoder_.Decode("a", &handler_));
  ASSERT_EQ(1, encoder_.num_urls());
  EXPECT_EQ(std::string("a"), encoder_.url(0));
}

}  // namespace net_instaweb
