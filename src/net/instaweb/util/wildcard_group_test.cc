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

#include "net/instaweb/util/public/wildcard_group.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class WildcardGroupTest : public testing::Test {
 protected:
  virtual void SetUp() {
    group_.Allow("*.cc");
    group_.Allow("*.h");
    group_.Disallow("a*.h");
    group_.Allow("ab*.h");
    group_.Disallow("c*.cc");
  }

  void TestGroup(const WildcardGroup& group) {
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
    EXPECT_TRUE(group.Match("not a match", true));
    EXPECT_FALSE(group.Match("not a match", false));
  }

  WildcardGroup group_;
};

TEST_F(WildcardGroupTest, Sequence) {
  TestGroup(group_);
}

TEST_F(WildcardGroupTest, CopySequence) {
  WildcardGroup copy;
  copy.CopyFrom(group_);
  TestGroup(copy);
}

TEST_F(WildcardGroupTest, AppendSequence) {
  WildcardGroup appended;
  appended.Allow("cb*.cc");
  group_.AppendFrom(appended);
  EXPECT_TRUE(group_.Match("cb.cc", false));
  EXPECT_FALSE(group_.Match("ca.cc", true));
}

}  // namespace net_instaweb
