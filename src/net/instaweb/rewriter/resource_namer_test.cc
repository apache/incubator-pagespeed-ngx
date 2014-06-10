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

  void TestCopyAndSize(int signature_length) {
    MockHasher mock_hasher(full_name_.hash());
    ResourceNamer copy;
    copy.CopyFrom(full_name_);
    EXPECT_STREQ(copy.Encode(), full_name_.Encode());
    EXPECT_EQ(
        full_name_.EventualSize(mock_hasher, signature_length),
        full_name_.Encode().size());
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
  TestCopyAndSize(0);
  EXPECT_STREQ("name.ext.as.many.as.I.like.pagespeed.id.hash.ext",
               full_name_.Encode());
  EXPECT_STREQ("id.name.ext.as.many.as.I.like",
               full_name_.EncodeIdName());
  full_name_.set_experiment("q");
  TestCopyAndSize(0);
  EXPECT_STREQ("name.ext.as.many.as.I.like.pagespeed.q.id.hash.ext",
               full_name_.Encode());
  full_name_.set_options("a=b");
  full_name_.set_experiment("");
  TestCopyAndSize(0);
  EXPECT_STREQ("name.ext.as.many.as.I.like.pagespeed.a=b.id.hash.ext",
               full_name_.Encode());
  full_name_.set_name("name.ext");
  full_name_.set_options("options.with.dots");
  TestCopyAndSize(0);
  EXPECT_STREQ("name.ext.pagespeed.options.with.dots.id.hash.ext",
               full_name_.Encode());
  full_name_.set_options("options/with/slashes");
  EXPECT_STREQ("name.ext.pagespeed.options,_with,_slashes.id.hash.ext",
               full_name_.Encode());
  TestCopyAndSize(0);
}

TEST_F(ResourceNamerTest, TestDecode) {
  EXPECT_TRUE(
      full_name_.Decode("name.ext.as.many.as.I.like.pagespeed.id.hash.ext",
                        4 /*hash size*/, 0 /*signature size*/));
  EXPECT_FALSE(full_name_.has_experiment());
  EXPECT_STREQ("id", full_name_.id());
  EXPECT_STREQ("name.ext.as.many.as.I.like", full_name_.name());
  EXPECT_STREQ("hash", full_name_.hash());
  EXPECT_STREQ("ext", full_name_.ext());
  TestCopyAndSize(0);
}

TEST_F(ResourceNamerTest, TestDecodeExperiment) {
  EXPECT_TRUE(
      full_name_.Decode("name.ext.as.many.as.I.like.pagespeed.d.id.hash.ext",
                        4 /*hash size*/, 0 /*signature_size*/));
  TestCopyAndSize(0);
  EXPECT_STREQ("d", full_name_.experiment());
  EXPECT_TRUE(full_name_.has_experiment());
  EXPECT_STREQ("id", full_name_.id());
  EXPECT_STREQ("name.ext.as.many.as.I.like", full_name_.name());
  EXPECT_STREQ("hash", full_name_.hash());
  EXPECT_STREQ("ext", full_name_.ext());
  EXPECT_TRUE(
      full_name_.Decode("name.ext.as.many.as.I.like.pagespeed.de.id.hash.ext",
                        4 /*hash size*/, 0 /*signature size*/));
  TestCopyAndSize(0);
  EXPECT_STREQ("", full_name_.experiment());
  EXPECT_STREQ("de", full_name_.options());
  EXPECT_FALSE(
      full_name_.Decode("name.ext.as.many.as.I.like.pagespeed..id.hash.ext",
                        4 /*hash size*/, 0 /*signature size*/));
  EXPECT_FALSE(
      full_name_.Decode("name.ext.as.many.as.I.like.pagespeed.D.id.hash.ext",
                        4 /*hash size*/, 0 /*signature size*/));
  EXPECT_FALSE(
      full_name_.Decode("name.ext.as.many.as.I.like.pagespeed.`.id.hash.ext",
                        4 /*hash size*/, 0 /*signature size*/));
  EXPECT_FALSE(
      full_name_.Decode("name.ext.as.many.as.I.like.pagespeed.{.id.hash.ext",
                        4 /*hash size*/, 0 /*signature size*/));
  EXPECT_TRUE(
      full_name_.Decode("name.ext.as.many.as.I.like.pagespeed.a.id.hash.ext",
                        4 /*hash size*/, 0 /*signature size*/));
  TestCopyAndSize(0);
  EXPECT_TRUE(
      full_name_.Decode("name.ext.as.many.as.I.like.pagespeed.z.id.hash.ext",
                        4 /*hash size*/, 0 /*signature size*/));
  TestCopyAndSize(0);
  EXPECT_TRUE(
      full_name_.Decode("name.ext.as.many.as.I.like.pagespeed.z.id.hash.ext",
                        4 /*hash size*/, 0 /*signature size*/));
  TestCopyAndSize(0);
  EXPECT_STREQ("", full_name_.options());
  EXPECT_TRUE(full_name_.Decode("name.ext.pagespeed.opts.id.hash.ext",
                                4 /*hash size*/, 0 /*signature size*/));
  TestCopyAndSize(0);
  EXPECT_STREQ("opts", full_name_.options());

  // Make sure a decode without options clears them.
  EXPECT_TRUE(
      full_name_.Decode("name.ext.as.many.as.I.like.pagespeed.z.id.hash.ext",
                        4 /*hash size*/, 0 /*signature size*/));
  TestCopyAndSize(0);
  EXPECT_STREQ("", full_name_.options());
}

TEST_F(ResourceNamerTest, TestDecodeOptions) {
  EXPECT_TRUE(full_name_.Decode("name.ext.pagespeed.options.id.hash.ext",
                                4 /*hash size*/, 0 /*signature size*/));
  TestCopyAndSize(0);
  EXPECT_STREQ("id", full_name_.id());
  EXPECT_STREQ("options", full_name_.options());
  EXPECT_TRUE(
      full_name_.Decode("name.ext.pagespeed.options,_with,_slashes.id.hash.ext",
                        4 /*hash size*/, 0 /*signature size*/));
  TestCopyAndSize(0);

  EXPECT_STREQ("options/with/slashes", full_name_.options());
  EXPECT_TRUE(
      full_name_.Decode("name.ext.pagespeed.options,_with,_slashes.id.hash.ext",
                        4 /*hash size*/, 0 /*signature size*/));
  EXPECT_STREQ("options/with/slashes", full_name_.options());
  TestCopyAndSize(0);

  EXPECT_TRUE(full_name_.Decode(
      "name.ext.pagespeed.options,qwith,aquery=params.id.hash.ext",
      4 /*hash size*/, 0 /*signature size*/));
  EXPECT_STREQ("options?with&query=params", full_name_.options());
  TestCopyAndSize(0);
}

TEST_F(ResourceNamerTest, TestDecodeTooMany) {
  EXPECT_TRUE(full_name_.Decode("name.extra_dot.pagespeed.id.hash.ext",
                                4 /*hash size*/, 0 /*signature size*/));
}

TEST_F(ResourceNamerTest, TestDecodeNotEnough) {
  EXPECT_FALSE(
      full_name_.Decode("id.name.hash", 4 /*hash size*/, 0 /*signature size*/));
}

TEST_F(ResourceNamerTest, TestLegacyDecode) {
  // Legacydecode doesn't include signature at all, and will not use the same
  // decode function. Putting -1, -1 because that piece of the code should not
  // be hit.
  EXPECT_TRUE(full_name_.Decode("id.0123456789abcdef0123456789ABCDEF.name.js",
                                -1 /*hash size*/, -1 /*signature size*/));
  EXPECT_STREQ("id", full_name_.id());
  EXPECT_STREQ("name", full_name_.name());
  EXPECT_STREQ("0123456789abcdef0123456789ABCDEF", full_name_.hash());
  EXPECT_STREQ("js", full_name_.ext());
}

TEST_F(ResourceNamerTest, TestEventualSize) {
  MockHasher mock_hasher;
  GoogleString file = "some_name.pagespeed.idn.0.extension";
  EXPECT_TRUE(full_name_.Decode(file, 1 /*hash size*/, 0 /*signature size*/));
  EXPECT_EQ(file.size(),
            full_name_.EventualSize(mock_hasher, 0 /*signature size*/));
}

TEST_F(ResourceNamerTest, TestEventualSizeExperiment) {
  MockHasher mock_hasher;
  GoogleString file = "some_name.pagespeed.c.idn.0.extension";
  EXPECT_TRUE(full_name_.Decode(file, 1 /*hash size*/, 0 /*signature size*/));
  EXPECT_EQ(file.size(),
            full_name_.EventualSize(mock_hasher, 0 /*signature size*/));
}

TEST_F(ResourceNamerTest, TestEventualSizeSignature) {
  MockHasher mock_hasher;
  GoogleString file = "some_name.pagespeed.idn.0sig.extension";
  EXPECT_TRUE(full_name_.Decode(file, 1 /*hash size*/, 3 /*signature size*/));
  EXPECT_EQ(file.size(),
            full_name_.EventualSize(mock_hasher, 3 /*signature size*/));
  EXPECT_STREQ("sig", full_name_.signature());
  EXPECT_STREQ("extension", full_name_.ext());
  EXPECT_STREQ("0", full_name_.hash());
}

TEST_F(ResourceNamerTest, TestAllSignaturePossibilites) {
  // Test with the signature on.
  full_name_.set_id("id");
  full_name_.set_name("name.ext.as.many.as.I.like");
  full_name_.set_hash("hash");
  full_name_.set_ext("ext");
  full_name_.set_signature("sig");
  TestCopyAndSize(3);
  EXPECT_STREQ("name.ext.as.many.as.I.like.pagespeed.id.hashsig.ext",
               full_name_.Encode());
  // Test with the signature off.
  full_name_.set_signature("");
  TestCopyAndSize(0);
  EXPECT_STREQ("name.ext.as.many.as.I.like.pagespeed.id.hash.ext",
               full_name_.Encode());
  // Test with experiment on and signature on.
  full_name_.set_signature("sig");
  full_name_.set_experiment("q");
  EXPECT_STREQ("name.ext.as.many.as.I.like.pagespeed.q.id.hashsig.ext",
               full_name_.Encode());
  // Test with experiment on and signature off.
  full_name_.set_signature("");
  EXPECT_STREQ("name.ext.as.many.as.I.like.pagespeed.q.id.hash.ext",
               full_name_.Encode());
  // Test with experiment and options on and signature on can't be done.
  // Test with experiment and options on and signature off can't be done.
  // Test with options on and signature on.
  full_name_.set_experiment("");
  full_name_.set_options("a=b");
  full_name_.set_signature("sig");
  EXPECT_STREQ("name.ext.as.many.as.I.like.pagespeed.a=b.id.hashsig.ext",
               full_name_.Encode());
  // Test with options on and signature off.
  full_name_.set_signature("");
  EXPECT_STREQ("name.ext.as.many.as.I.like.pagespeed.a=b.id.hash.ext",
               full_name_.Encode());
}

TEST_F(ResourceNamerTest, TestSizeWithoutHash_HashNotSet) {
  MD5Hasher md5_hasher;
  full_name_.set_name("file.css");
  full_name_.set_id("id");
  full_name_.set_ext("ext");
  EXPECT_EQ(STATIC_STRLEN("file.css") + STATIC_STRLEN("id") +
            STATIC_STRLEN("ext") + ResourceNamer::kOverhead +
            md5_hasher.HashSizeInChars(),
            full_name_.EventualSize(md5_hasher, 0  /*signature size*/));
}

}  // namespace

}  // namespace net_instaweb
