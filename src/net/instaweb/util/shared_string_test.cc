/*
 * Copyright 2012 Google Inc.
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

#include "net/instaweb/util/public/shared_string.h"

#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class SharedStringTest : public testing::Test {
 protected:
};

TEST_F(SharedStringTest, ConstructFromStringPiece) {
  SharedString ss(StringPiece("hello"));
  EXPECT_STREQ("hello", ss.Value());
}

TEST_F(SharedStringTest, ConstructFromString) {
  SharedString ss(GoogleString("hello"));
  EXPECT_STREQ("hello", ss.Value());
}

TEST_F(SharedStringTest, ConstructFromCharStar) {
  SharedString ss("hello");
  EXPECT_STREQ("hello", ss.Value());
}

TEST_F(SharedStringTest, ConstructFromSharedString) {
  SharedString ss("hello");
  EXPECT_TRUE(ss.unique());
  SharedString ss2(ss);
  EXPECT_FALSE(ss.unique());
  EXPECT_FALSE(ss2.unique());
  EXPECT_STREQ("hello", ss.Value());
  EXPECT_STREQ("hello", ss2.Value());
  EXPECT_TRUE(ss.SharesStorage(ss2))<< "storage is shared";
  EXPECT_FALSE(ss.trimmed());

  // Mutations to ss do not affect ss2.
  ss.Append(", World!");
  EXPECT_STREQ("hello, World!", ss.Value());
  EXPECT_STREQ("hello", ss2.Value()) << "ss2 unaffected by ss.Append";
  EXPECT_TRUE(ss.SharesStorage(ss2)) << "storage is still shared!";
  EXPECT_FALSE(ss.trimmed());

  // Removing a suffix means that we no longer have an accurate
  // GoogleString representation, and trimmed() return false.
  ss.RemoveSuffix(1);  // removes "!"
  EXPECT_STREQ("hello, World", ss.Value());
  EXPECT_TRUE(ss.trimmed());

  // In order to Append on more bytes, we must detach first so that
  // we don't have a suffix.
  ss.DetachRetainingContent();
  ss.Append(".");
  EXPECT_STREQ("hello, World.", ss.Value());
  EXPECT_FALSE(ss.trimmed());

  // Now re-link the two SharedStrings.
  ss2 = ss;
  EXPECT_STREQ("hello, World.", ss2.Value());
  EXPECT_TRUE(ss.SharesStorage(ss2)) << "storage is shared!";

  // Prefix removal is also not shared, although the storage is still linked.
  ss.RemovePrefix(7);  // removes "hello, "
  EXPECT_STREQ("World.", ss.Value());
  EXPECT_STREQ("hello, World.", ss2.Value());
  EXPECT_TRUE(ss.SharesStorage(ss2)) << "storage is shared!";

  EXPECT_FALSE(ss.unique());
  EXPECT_FALSE(ss2.unique());
}

TEST_F(SharedStringTest, Assign) {
  SharedString ss("hello");
  SharedString ss2(ss);
  ss.Assign("Goodbye");
  EXPECT_STREQ("Goodbye", ss.Value());
  EXPECT_STREQ("hello", ss2.Value());  // Detach on assign.

  // It's OK to assign from overlapping bytes.
  ss.Assign(ss.Value().substr(4));
  EXPECT_STREQ("bye", ss.Value());
}

TEST_F(SharedStringTest, SwapWithString) {
  SharedString ss("hello");
  GoogleString buf("Goodbye");
  ss.SwapWithString(&buf);
  EXPECT_STREQ("Goodbye", ss.Value());
  EXPECT_STREQ("hello", buf);

  ss.RemoveSuffix(1);
  ss.RemovePrefix(4);
  EXPECT_STREQ("by", ss.Value());
  ss.SwapWithString(&buf);
  EXPECT_STREQ("hello", ss.Value()) << "1 byte of suffix no longer removed.";
  EXPECT_STREQ("Goodbye", buf) << "string storage intact after removing prefix"
                               << " and truncating.";

  SharedString ss2 = ss;
  ss.SwapWithString(&buf);
  EXPECT_STREQ("Goodbye", ss.Value());
  EXPECT_STREQ("", buf) << "due to ss being detached as part of the swap.";
  EXPECT_STREQ("hello", ss2.Value()) << "detached.";
}

TEST_F(SharedStringTest, Clear) {
  SharedString ss("hello");
  ss.DetachAndClear();
  EXPECT_TRUE(ss.empty());
  EXPECT_EQ(0, ss.size());
  EXPECT_STREQ("", ss.Value());

  // When we remove a prefix, Clear clears that fact too.
  ss.Assign("12345");
  ss.RemovePrefix(1);
  EXPECT_STREQ("2345", ss.Value());
  ss.DetachAndClear();
  EXPECT_EQ(0, ss.size());

  ss.Assign("12345");
  EXPECT_STREQ("12345", ss.Value());

  // When a string is shared, clearing it has no effect.
  SharedString ss2(ss);
  ss2.RemoveSuffix(2);
  EXPECT_STREQ("12345", ss.Value());  // Does not have its suffix removed.
  EXPECT_STREQ("123", ss2.Value());
  ss.DetachAndClear();
  EXPECT_STREQ("", ss.Value());
  EXPECT_STREQ("123", ss2.Value());
  EXPECT_TRUE(ss2.unique());
}

TEST_F(SharedStringTest, DetachRetainingContent) {
  SharedString ss("hello");
  SharedString ss2 = ss;
  EXPECT_TRUE(ss.SharesStorage(ss2));
  ss.DetachRetainingContent();
  EXPECT_FALSE(ss.SharesStorage(ss2));
  EXPECT_TRUE(ss.unique());
  EXPECT_STREQ("hello", ss.Value());
  EXPECT_STREQ("hello", ss2.Value());
}


TEST_F(SharedStringTest, WriteAt) {
  SharedString ss("HELLO");
  SharedString ss2 = ss;
  ss.WriteAt(0, "123", 3);
  EXPECT_STREQ("123LO", ss.Value());
  ss.WriteAt(1, "YZ", 2);
  EXPECT_STREQ("1YZLO", ss.Value());
  EXPECT_STREQ("1YZLO", ss2.Value());

  // Now trim some characters and make sure this all stays sane.
  ss.RemovePrefix(1);
  ss.RemoveSuffix(1);
  EXPECT_STREQ("YZL", ss.Value());
  EXPECT_STREQ("1YZLO", ss2.Value());
  EXPECT_TRUE(ss.SharesStorage(ss2));  // storage still shared.

  // Replace the Z with an A.  This affects both ss and ss2.
  ss.WriteAt(1, "A", 1);
  EXPECT_STREQ("YAL", ss.Value());
  EXPECT_STREQ("1YALO", ss2.Value());
  EXPECT_TRUE(ss.SharesStorage(ss2));  // storage still shared.
}

TEST_F(SharedStringTest, Extend) {
  SharedString ss("x");
  SharedString ss2 = ss;
  ss.Extend(4);  // adds 3 undefined characters.
  ss.WriteAt(1, "123", 3);
  EXPECT_STREQ("x123", ss.Value());
  EXPECT_STREQ("x", ss2.Value()) << "ss2 was not extended";
  EXPECT_TRUE(ss.SharesStorage(ss2)) << "but ss and ss2 still share storage";

  // It's OK to extend a string that has a removed prefix, and storage
  // sharing will still be retained.
  ss.RemovePrefix(1);
  EXPECT_STREQ("123", ss.Value());
  EXPECT_STREQ("x", ss2.Value());
  ss.Extend(4);  // adds one more blank.
  ss.WriteAt(3, "4", 1);
  EXPECT_STREQ("1234", ss.Value());
  EXPECT_STREQ("x", ss2.Value()) << "ss2 still not affected";
  EXPECT_TRUE(ss.SharesStorage(ss2)) << "and storage is still retained";

  ss.RemoveSuffix(2);
  EXPECT_STREQ("12", ss.Value());
  EXPECT_STREQ("x", ss2.Value());
  EXPECT_TRUE(ss.SharesStorage(ss2)) << "and storage is still retained";

  // It's also fine to extend a truncated string, but then it gets its
  // own storage.
  ss.Extend(7);
  ss.WriteAt(2, "abcde", 5);
  EXPECT_STREQ("12abcde", ss.Value());
  EXPECT_STREQ("x", ss2.Value()) << "ss2 still unaffected";
  EXPECT_FALSE(ss.SharesStorage(ss2))<< "finally storage is detached";
}

TEST_F(SharedStringTest, ExtendUniqueTruncated) {
  SharedString ss("abc");
  const GoogleString* original_storage = ss.StringValue();
  ss.RemoveSuffix(1);
  ss.Extend(6);
  ss.WriteAt(2, "1234", 4);
  EXPECT_EQ("ab1234", ss.Value());
  EXPECT_EQ(original_storage, ss.StringValue())
      << "Re-use the same storage across truncate/extend of unique string";
}

}  // namespace net_instaweb
