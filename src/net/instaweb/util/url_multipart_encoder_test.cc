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

  StringVector url_vector_;
};

TEST_F(UrlMultipartEncoderTest, EscapeSeparatorsAndEscapes) {
  url_vector_.push_back("abc");
  url_vector_.push_back("def");
  url_vector_.push_back("a=b+c");  // escape and separate characters
  GoogleString encoding;
  encoder_.Encode(url_vector_, NULL, &encoding);
  url_vector_.clear();
  ASSERT_TRUE(encoder_.Decode(encoding, &url_vector_, NULL, &handler_));
  ASSERT_EQ(3, url_vector_.size());
  EXPECT_EQ(GoogleString("abc"), url_vector_[0]);
  EXPECT_EQ(GoogleString("def"), url_vector_[1]);
  EXPECT_EQ(GoogleString("a=b+c"), url_vector_[2]);
}

TEST_F(UrlMultipartEncoderTest, Empty) {
  StringVector urls;
  ASSERT_TRUE(encoder_.Decode("", &url_vector_, NULL, &handler_));
  EXPECT_EQ(0, url_vector_.size());
}

TEST_F(UrlMultipartEncoderTest, LastIsEmpty) {
  ASSERT_TRUE(encoder_.Decode("a+b+", &url_vector_, NULL, &handler_));
  ASSERT_EQ(3, url_vector_.size());
  EXPECT_EQ(GoogleString("a"), url_vector_[0]);
  EXPECT_EQ(GoogleString("b"), url_vector_[1]);
  EXPECT_EQ(GoogleString(""), url_vector_[2]);
}

TEST_F(UrlMultipartEncoderTest, One) {
  ASSERT_TRUE(encoder_.Decode("a", &url_vector_, NULL, &handler_));
  ASSERT_EQ(1, url_vector_.size());
  EXPECT_EQ(GoogleString("a"), url_vector_[0]);
}

}  // namespace net_instaweb
