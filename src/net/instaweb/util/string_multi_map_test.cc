// Copyright 2010-2011 and onwards Google Inc.
// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test StringMultiMap.

#include "net/instaweb/util/public/string_multi_map.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kQueryString[] = "a=1&b&c=2&d=&a=3";

}  // namespace

namespace net_instaweb {

class StringMultiMapTest : public testing::Test {
 protected:
  StringMultiMapTest() { }

  virtual void SetUp() {
    string_map_.Add("a", "1");
    string_map_.Add("b", NULL);
    string_map_.Add("C", "2");
    string_map_.Add("d", "");
    string_map_.Add("A", "3");
    string_map_.Add("e", GoogleString("3\000 4", 4));
  }

  StringMultiMapInsensitive string_map_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StringMultiMapTest);
};

TEST_F(StringMultiMapTest, TestAdd) {
  ASSERT_EQ(5, string_map_.num_names());
  ASSERT_EQ(6, string_map_.num_values());
  EXPECT_EQ(GoogleString("a"), string_map_.name(0));
  EXPECT_EQ(GoogleString("1"), *(string_map_.value(0)));
  EXPECT_EQ(GoogleString("b"), string_map_.name(1));
  EXPECT_EQ(NULL, string_map_.value(1));
  EXPECT_EQ(GoogleString("C"), string_map_.name(2));
  EXPECT_EQ(GoogleString("2"), *(string_map_.value(2)));
  EXPECT_EQ(GoogleString("d"), string_map_.name(3));
  EXPECT_EQ(GoogleString(""), *(string_map_.value(3)));
  EXPECT_EQ(GoogleString("a"), string_map_.name(4));
  EXPECT_EQ(GoogleString("3"), *(string_map_.value(4)));
  EXPECT_EQ(4, string_map_.value(5)->size());
  EXPECT_EQ(1, strlen(string_map_.value(5)->c_str()));
}

TEST_F(StringMultiMapTest, TestLookup) {
  ConstStringStarVector v;
  ASSERT_TRUE(string_map_.Lookup("a", &v));
  ASSERT_EQ(2, v.size());
  EXPECT_EQ(GoogleString("1"), *(v[0]));
  EXPECT_EQ(GoogleString("3"), *(v[1]));
  ASSERT_TRUE(string_map_.Lookup("b", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(NULL, v[0]);
  ASSERT_TRUE(string_map_.Lookup("C", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(GoogleString("2"), *(v[0]));
  ASSERT_TRUE(string_map_.Lookup("d", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(GoogleString(""), *(v[0]));
  ASSERT_TRUE(string_map_.Lookup("e", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(4, v[0]->size());
}

TEST_F(StringMultiMapTest, TestRemove) {
  ConstStringStarVector v;
  EXPECT_TRUE(string_map_.RemoveAll("e"));
  EXPECT_EQ(4, string_map_.num_names());
  EXPECT_TRUE(string_map_.RemoveAll("a"));
  EXPECT_EQ(3, string_map_.num_names());
  EXPECT_EQ(3, string_map_.num_values());
  EXPECT_TRUE(string_map_.RemoveAll("b"));
  EXPECT_EQ(2, string_map_.num_names());
  EXPECT_TRUE(string_map_.RemoveAll("c"));
  EXPECT_EQ(1, string_map_.num_names());
  EXPECT_TRUE(string_map_.RemoveAll("D"));
  EXPECT_EQ(0, string_map_.num_names());
  EXPECT_FALSE(string_map_.RemoveAll("not present"));
}

TEST_F(StringMultiMapTest, TestClear) {
  string_map_.Clear();
  EXPECT_EQ(0, string_map_.num_names());
  EXPECT_EQ(0, string_map_.num_values());
}

}  // namespace net_instaweb
