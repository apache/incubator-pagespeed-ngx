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

#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

namespace {

class ResourceNamerTest : public testing::Test {
 protected:
  ResourceNamerTest() { }

  ResourceNamer full_name_;
};

TEST_F(ResourceNamerTest, TestEncode) {
  // Stand up a minimal resource manager that only has the
  // resources we should need to encode.
  full_name_.set_id("id");
  full_name_.set_name("name.ext.as.many.as.I.like");
  full_name_.set_hash("hash");
  full_name_.set_ext("ext");
  EXPECT_EQ(std::string("name.ext.as.many.as.I.like.pagespeed.id.hash.ext"),
            full_name_.Encode());
  EXPECT_EQ(std::string("id.name.ext.as.many.as.I.like"),
            full_name_.EncodeIdName());
}

TEST_F(ResourceNamerTest, TestDecode) {
  EXPECT_TRUE(full_name_.Decode(
      "name.ext.as.many.as.I.like.pagespeed.id.hash.ext"));
  EXPECT_EQ("id", full_name_.id());
  EXPECT_EQ("name.ext.as.many.as.I.like", full_name_.name());
  EXPECT_EQ("hash", full_name_.hash());
  EXPECT_EQ("ext", full_name_.ext());
}

TEST_F(ResourceNamerTest, TestDecodeTooMany) {
  EXPECT_TRUE(full_name_.Decode("name.extra_dot.pagespeed.id.hash.ext"));
}

TEST_F(ResourceNamerTest, TestDecodeNotEnough) {
  EXPECT_FALSE(full_name_.Decode("id.name.hash"));
}

TEST_F(ResourceNamerTest, TestLegacyDecode) {
  EXPECT_TRUE(full_name_.Decode("id.0123456789abcdef0123456789ABCDEF.name.js"));
  EXPECT_EQ("id", full_name_.id());
  EXPECT_EQ("name", full_name_.name());
  EXPECT_EQ("0123456789abcdef0123456789ABCDEF", full_name_.hash());
  EXPECT_EQ("js", full_name_.ext());
}

}  // namespace

}  // namespace net_instaweb
