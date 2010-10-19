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

#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

namespace {

class ResourceNamerTest : public testing::Test {
 protected:
  ResourceNamer full_name_;
};

TEST_F(ResourceNamerTest, TestEncode) {
  full_name_.set_id("id");
  full_name_.set_name("name");
  full_name_.set_hash("hash");
  full_name_.set_ext("ext");
  EXPECT_EQ(std::string("id.hash.name.ext"), full_name_.Encode());
  EXPECT_EQ(std::string("id.name"), full_name_.EncodeIdName());
  EXPECT_EQ(std::string("hash.ext"), full_name_.EncodeHashExt());
}

TEST_F(ResourceNamerTest, TestDecode) {
  EXPECT_TRUE(full_name_.Decode("id.hash.name.ext"));
  EXPECT_EQ("id", full_name_.id());
  EXPECT_EQ("name", full_name_.name());
  EXPECT_EQ("hash", full_name_.hash());
  EXPECT_EQ("ext", full_name_.ext());
}

TEST_F(ResourceNamerTest, TestDecodeTooMany) {
  EXPECT_FALSE(full_name_.Decode("id.name.hash.ext.extra_dot"));
  EXPECT_FALSE(full_name_.DecodeHashExt("id.hash.ext"));
}

TEST_F(ResourceNamerTest, TestDecodeNotEnough) {
  EXPECT_FALSE(full_name_.Decode("id.name.hash"));
  EXPECT_FALSE(full_name_.DecodeHashExt("ext"));
}

TEST_F(ResourceNamerTest, TestDecodeHashExt) {
  EXPECT_TRUE(full_name_.DecodeHashExt("hash.ext"));
  EXPECT_EQ("", full_name_.id());
  EXPECT_EQ("", full_name_.name());
  EXPECT_EQ("hash", full_name_.hash());
  EXPECT_EQ("ext", full_name_.ext());
}

}  // namespace

}  // namespace net_instaweb
