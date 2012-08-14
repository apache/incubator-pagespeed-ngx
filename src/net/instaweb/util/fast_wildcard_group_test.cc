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

// Author: jmaessen@google.com (Jan-Willem Maessen)

#include "net/instaweb/util/public/fast_wildcard_group.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {
namespace {

const char kInitialSignature[] = "*.ccA,*.hA,a*.hD,ab*.hA,c*.ccD,";

class FastWildcardGroupTest : public testing::Test {
 protected:
  virtual void SetUp() {
    group_.Allow("*.cc");
    group_.Allow("*.h");
    group_.Disallow("a*.h");
    group_.Allow("ab*.h");
    group_.Disallow("c*.cc");
    signature_.assign(kInitialSignature);
  }

  void MakeLarge() {
    // Insert trivial patterns to match 4-digit integers.  Ensures that the
    // resulting group will trigger non-trivial compilation, to investigate
    // various sources of re-compilation bugs.
    for (int i = 1000; i < 1100; ++i) {
      GoogleString i_string = IntegerToString(i);
      group_.Disallow(i_string);
      StrAppend(&signature_, i_string, "D,");
    }
  }

  void TestMatches(const FastWildcardGroup& group) {
    EXPECT_TRUE(group.Match("x.cc", true));
    EXPECT_TRUE(group.Match("x.cc", false));
    EXPECT_FALSE(group.Match("c.cc", true));
    EXPECT_FALSE(group.Match("c.cc", false));
    EXPECT_TRUE(group.Match("y.h", true));
    EXPECT_TRUE(group.Match("y.h", false));
    EXPECT_FALSE(group.Match("a.h", true));
    EXPECT_FALSE(group.Match("a.h", false));
    EXPECT_TRUE(group.Match("ab.h", true));
    EXPECT_TRUE(group.Match("ab.h", false));
  }

  void TestDefaults(const FastWildcardGroup& group,
                    bool default_to_pass,
                    bool result_to_expect) {
    EXPECT_EQ(result_to_expect, group.Match("", default_to_pass));
    EXPECT_EQ(result_to_expect, group.Match("not a match", default_to_pass));
  }

  void TestGroup(const FastWildcardGroup& group) {
    TestMatches(group);
    TestDefaults(group, true, true);
    TestDefaults(group, false, false);
  }

  void Sequence() {
    TestGroup(group_);
    EXPECT_EQ(signature_, group_.Signature());
  }

  void Copy() {
    FastWildcardGroup copy;
    copy.CopyFrom(group_);
    TestGroup(copy);
    EXPECT_EQ(signature_, copy.Signature());
  }

  void Append() {
    FastWildcardGroup appended;
    appended.Allow("cb*.cc");
    group_.AppendFrom(appended);
    EXPECT_TRUE(group_.Match("cb.cc", false));
    EXPECT_FALSE(group_.Match("ca.cc", true));
    signature_.append("cb*.ccA,");
    EXPECT_EQ(signature_, group_.Signature());
  }

  void HardCodedDefault() {
    FastWildcardGroup group;
    group.Allow("*");
    group.AppendFrom(group_);
    TestMatches(group);
    // Make sure we can compute signature in mid-match.
    GoogleString signature = StrCat("*A,", signature_);
    EXPECT_EQ(signature, group.Signature());
    TestDefaults(group, true, true);
    TestDefaults(group, false, true);
  }

  FastWildcardGroup group_;
  GoogleString signature_;
};

TEST_F(FastWildcardGroupTest, Sequence) {
  Sequence();
}

TEST_F(FastWildcardGroupTest, SequenceLarge) {
  MakeLarge();
  Sequence();
}

TEST_F(FastWildcardGroupTest, CopySequence) {
  Copy();
}

TEST_F(FastWildcardGroupTest, CopySequenceLarge) {
  MakeLarge();
  Copy();
}

TEST_F(FastWildcardGroupTest, AppendSequence) {
  Append();
}

TEST_F(FastWildcardGroupTest, AppendSequenceLarge) {
  MakeLarge();
  Append();
}

TEST_F(FastWildcardGroupTest, HardCodedDefault) {
  HardCodedDefault();
}

TEST_F(FastWildcardGroupTest, HardCodedDefaultLarge) {
  MakeLarge();
  HardCodedDefault();
}

TEST_F(FastWildcardGroupTest, EmptyGroup) {
  FastWildcardGroup group;
  EXPECT_TRUE(group.Match("cb.cc", true));
  EXPECT_FALSE(group.Match("ca.cc", false));
  EXPECT_EQ("", group.Signature());
}

TEST_F(FastWildcardGroupTest, IncrementalUpdate) {
  // Make sure various incremental operations re-compile safely.
  FastWildcardGroup copy;
  copy.CopyFrom(group_);
  MakeLarge();
  TestMatches(group_);
  EXPECT_FALSE(group_.Match("1034", true));
  EXPECT_FALSE(group_.Match("Complicated literal pattern", false));
  EXPECT_TRUE(group_.Match("Just the wrong size..", true));
  EXPECT_TRUE(group_.Match("Another complicated literal pattern", true));
  group_.Allow("Complicated literal pattern");
  TestMatches(group_);
  EXPECT_FALSE(group_.Match("1034", true));
  EXPECT_TRUE(group_.Match("Complicated literal pattern", false));
  EXPECT_TRUE(group_.Match("Just the wrong size..", true));
  EXPECT_TRUE(group_.Match("Another complicated literal pattern", true));
  group_.Disallow("?????????????????????");
  TestMatches(group_);
  EXPECT_FALSE(group_.Match("1034", true));
  EXPECT_TRUE(group_.Match("Complicated literal pattern", false));
  EXPECT_FALSE(group_.Match("Just the wrong size..", true));
  EXPECT_TRUE(group_.Match("Another complicated literal pattern", true));
  FastWildcardGroup group;
  group.Disallow("Another complicated literal pattern");
  group_.AppendFrom(group);
  TestMatches(group_);
  EXPECT_FALSE(group_.Match("1034", true));
  EXPECT_TRUE(group_.Match("Complicated literal pattern", false));
  EXPECT_FALSE(group_.Match("Just the wrong size..", true));
  EXPECT_FALSE(group_.Match("Another complicated literal pattern", true));
  group_.CopyFrom(copy);
  // Make sure we went back to the old state.
  TestMatches(group_);
  EXPECT_TRUE(group_.Match("1034", true));
  EXPECT_FALSE(group_.Match("Complicated literal pattern", false));
  EXPECT_TRUE(group_.Match("Just the wrong size..", true));
  EXPECT_TRUE(group_.Match("Another complicated literal pattern", true));
}

}  // namespace
}  // namespace net_instaweb
