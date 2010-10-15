/**
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

#include "net/instaweb/rewriter/public/resource_encoder.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

namespace {

class ResourceEncoderTest : public testing::Test {
 protected:
  ResourceEncoder encoder_;
};

TEST_F(ResourceEncoderTest, TestEncode) {
  encoder_.set_id("id");
  encoder_.set_name("name");
  encoder_.set_hash("hash");
  encoder_.set_ext("ext");
  EXPECT_EQ(std::string("id.hash.name.ext"), encoder_.Encode());
  EXPECT_EQ(std::string("id.name"), encoder_.EncodeNameKey());
  EXPECT_EQ(std::string("hash.ext"), encoder_.EncodeHashExt());
}

TEST_F(ResourceEncoderTest, TestDecode) {
  EXPECT_TRUE(encoder_.Decode("id.hash.name.ext"));
  EXPECT_EQ("id", encoder_.id());
  EXPECT_EQ("name", encoder_.name());
  EXPECT_EQ("hash", encoder_.hash());
  EXPECT_EQ("ext", encoder_.ext());
}

TEST_F(ResourceEncoderTest, TestDecodeTooMany) {
  EXPECT_FALSE(encoder_.Decode("id.name.hash.ext.extra_dot"));
  EXPECT_FALSE(encoder_.DecodeHashExt("id.hash.ext"));
}

TEST_F(ResourceEncoderTest, TestDecodeNotEnough) {
  EXPECT_FALSE(encoder_.Decode("id.name.hash"));
  EXPECT_FALSE(encoder_.DecodeHashExt("ext"));
}

TEST_F(ResourceEncoderTest, TestDecodeHashExt) {
  EXPECT_TRUE(encoder_.DecodeHashExt("hash.ext"));
  EXPECT_EQ("", encoder_.id());
  EXPECT_EQ("", encoder_.name());
  EXPECT_EQ("hash", encoder_.hash());
  EXPECT_EQ("ext", encoder_.ext());
}

}  // namespace

}  // namespace net_instaweb
