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

#include "net/instaweb/rewriter/public/resource_namer.h"

#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/mock_hasher.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

class ResourceNamerTest : public testing::Test {
 protected:
  ResourceNamerTest() { }

  void TestCopyAndSize() {
    MockHasher mock_hasher(full_name_.hash());
    ResourceNamer copy;
    copy.CopyFrom(full_name_);
    EXPECT_STREQ(copy.Encode(), full_name_.Encode());
    EXPECT_EQ(full_name_.EventualSize(mock_hasher), full_name_.Encode().size());
  }

  ResourceNamer full_name_;
};

TEST_F(ResourceNamerTest, TestEncode) {
  // Stand up a minimal resource manager that only has the
  // resources we should need to encode.
  full_name_.set_id("id");
  full_name_.set_name("name.ext.as.many.as.I.like");
  full_name_.set_hash("hash");
  full_name_.set_ext("ext");
  TestCopyAndSize();
  EXPECT_STREQ("name.ext.as.many.as.I.like.pagespeed.id.hash.ext",
               full_name_.Encode());
  EXPECT_STREQ("id.name.ext.as.many.as.I.like",
               full_name_.EncodeIdName());
  full_name_.set_experiment("q");
  TestCopyAndSize();
  EXPECT_STREQ("name.ext.as.many.as.I.like.pagespeed.q.id.hash.ext",
               full_name_.Encode());
  full_name_.set_options("a=b");
  full_name_.set_experiment("");
  TestCopyAndSize();
  EXPECT_STREQ("name.ext.as.many.as.I.like.pagespeed.a=b.id.hash.ext",
               full_name_.Encode());
  full_name_.set_name("name.ext");
  full_name_.set_options("options.with.dots");
  TestCopyAndSize();
  EXPECT_STREQ("name.ext.pagespeed.options.with.dots.id.hash.ext",
               full_name_.Encode());
  full_name_.set_options("options/with/slashes");
  EXPECT_STREQ("name.ext.pagespeed.options,_with,_slashes.id.hash.ext",
               full_name_.Encode());
  TestCopyAndSize();
}

TEST_F(ResourceNamerTest, TestDecode) {
  EXPECT_TRUE(full_name_.Decode(
      "name.ext.as.many.as.I.like.pagespeed.id.hash.ext"));
  EXPECT_FALSE(full_name_.has_experiment());
  EXPECT_STREQ("id", full_name_.id());
  EXPECT_STREQ("name.ext.as.many.as.I.like", full_name_.name());
  EXPECT_STREQ("hash", full_name_.hash());
  EXPECT_STREQ("ext", full_name_.ext());
  TestCopyAndSize();
}

TEST_F(ResourceNamerTest, TestDecodeExperiment) {
  EXPECT_TRUE(full_name_.Decode(
      "name.ext.as.many.as.I.like.pagespeed.d.id.hash.ext"));
  TestCopyAndSize();
  EXPECT_STREQ("d", full_name_.experiment());
  EXPECT_TRUE(full_name_.has_experiment());
  EXPECT_STREQ("id", full_name_.id());
  EXPECT_STREQ("name.ext.as.many.as.I.like", full_name_.name());
  EXPECT_STREQ("hash", full_name_.hash());
  EXPECT_STREQ("ext", full_name_.ext());
  EXPECT_TRUE(full_name_.Decode(
      "name.ext.as.many.as.I.like.pagespeed.de.id.hash.ext"));
  TestCopyAndSize();
  EXPECT_STREQ("", full_name_.experiment());
  EXPECT_STREQ("de", full_name_.options());
  EXPECT_FALSE(full_name_.Decode(
      "name.ext.as.many.as.I.like.pagespeed..id.hash.ext"));
  EXPECT_FALSE(full_name_.Decode(
      "name.ext.as.many.as.I.like.pagespeed.D.id.hash.ext"));
  EXPECT_FALSE(full_name_.Decode(
      "name.ext.as.many.as.I.like.pagespeed.`.id.hash.ext"));
  EXPECT_FALSE(full_name_.Decode(
      "name.ext.as.many.as.I.like.pagespeed.{.id.hash.ext"));
  EXPECT_TRUE(full_name_.Decode(
      "name.ext.as.many.as.I.like.pagespeed.a.id.hash.ext"));
  TestCopyAndSize();
  EXPECT_TRUE(full_name_.Decode(
      "name.ext.as.many.as.I.like.pagespeed.z.id.hash.ext"));
  TestCopyAndSize();
  EXPECT_TRUE(full_name_.Decode(
      "name.ext.as.many.as.I.like.pagespeed.z.id.hash.ext"));
  TestCopyAndSize();
  EXPECT_STREQ("", full_name_.options());
  EXPECT_TRUE(full_name_.Decode(
      "name.ext.pagespeed.opts.id.hash.ext"));
  TestCopyAndSize();
  EXPECT_STREQ("opts", full_name_.options());

  // Make sure a decode without options clears them.
  EXPECT_TRUE(full_name_.Decode(
      "name.ext.as.many.as.I.like.pagespeed.z.id.hash.ext"));
  TestCopyAndSize();
  EXPECT_STREQ("", full_name_.options());
}

TEST_F(ResourceNamerTest, TestDecodeOptions) {
  EXPECT_TRUE(full_name_.Decode("name.ext.pagespeed.options.id.hash.ext"));
  TestCopyAndSize();
  EXPECT_STREQ("id", full_name_.id());
  EXPECT_STREQ("options", full_name_.options());
  EXPECT_TRUE(full_name_.Decode(
      "name.ext.pagespeed.options,_with,_slashes.id.hash.ext"));
  TestCopyAndSize();

  EXPECT_STREQ("options/with/slashes", full_name_.options());
  EXPECT_TRUE(full_name_.Decode(
      "name.ext.pagespeed.options,_with,_slashes.id.hash.ext"));
  EXPECT_STREQ("options/with/slashes", full_name_.options());
  TestCopyAndSize();

  EXPECT_TRUE(full_name_.Decode(
      "name.ext.pagespeed.options,qwith,aquery=params.id.hash.ext"));
  EXPECT_STREQ("options?with&query=params", full_name_.options());
  TestCopyAndSize();
}

TEST_F(ResourceNamerTest, TestDecodeTooMany) {
  EXPECT_TRUE(full_name_.Decode("name.extra_dot.pagespeed.id.hash.ext"));
}

TEST_F(ResourceNamerTest, TestDecodeNotEnough) {
  EXPECT_FALSE(full_name_.Decode("id.name.hash"));
}

TEST_F(ResourceNamerTest, TestLegacyDecode) {
  EXPECT_TRUE(full_name_.Decode("id.0123456789abcdef0123456789ABCDEF.name.js"));
  EXPECT_STREQ("id", full_name_.id());
  EXPECT_STREQ("name", full_name_.name());
  EXPECT_STREQ("0123456789abcdef0123456789ABCDEF", full_name_.hash());
  EXPECT_STREQ("js", full_name_.ext());
}

TEST_F(ResourceNamerTest, TestEventualSize) {
  MockHasher mock_hasher;
  GoogleString file = "some_name.pagespeed.idn.0.extension";
  EXPECT_TRUE(full_name_.Decode(file));
  EXPECT_EQ(file.size(), full_name_.EventualSize(mock_hasher));
}

TEST_F(ResourceNamerTest, TestEventualSizeExperiment) {
  MockHasher mock_hasher;
  GoogleString file = "some_name.pagespeed.c.idn.0.extension";
  EXPECT_TRUE(full_name_.Decode(file));
  EXPECT_EQ(file.size(), full_name_.EventualSize(mock_hasher));
}

TEST_F(ResourceNamerTest, TestSizeWithoutHash_HashNotSet) {
  MD5Hasher md5_hasher;
  full_name_.set_name("file.css");
  full_name_.set_id("id");
  full_name_.set_ext("ext");
  EXPECT_EQ(STATIC_STRLEN("file.css") + STATIC_STRLEN("id") +
            STATIC_STRLEN("ext") + ResourceNamer::kOverhead +
            md5_hasher.HashSizeInChars(),
            full_name_.EventualSize(md5_hasher));
}

}  // namespace

}  // namespace net_instaweb
