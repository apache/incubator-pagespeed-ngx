// Copyright 2010-2011 and onwards Google Inc.
// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test StringMultiMap.

#include "pagespeed/kernel/base/string_multi_map.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

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

TEST_F(StringMultiMapTest, AddFromNameValuePairs) {
  StringMultiMapInsensitive string_map;
  ConstStringStarVector v;

  // Test the case that omit_if_no_value is true.
  string_map.AddFromNameValuePairs(
      "iq=22,m=3m3,x= ,oo=,kk", ",", '=', true /* omit_if_no_value */);
  EXPECT_EQ(4, string_map.num_names());

  EXPECT_TRUE(string_map.Lookup("iq", &v));
  EXPECT_EQ(1, v.size());
  EXPECT_STREQ("22", *v[0]);

  EXPECT_TRUE(string_map.Lookup("m", &v));
  EXPECT_EQ(1, v.size());
  EXPECT_STREQ("3m3", *v[0]);

  EXPECT_TRUE(string_map.Lookup("x", &v));
  EXPECT_EQ(1, v.size());
  EXPECT_STREQ(" ", *v[0]);

  EXPECT_TRUE(string_map.Lookup("oo", &v));
  EXPECT_EQ(1, v.size());
  EXPECT_STREQ("", *v[0]);

  EXPECT_FALSE(string_map.Lookup("kk", &v));

  // Test the case that omit_if_no_value is false.
  string_map.AddFromNameValuePairs(
      "a,b;a=:3", ";", ':', false /* omit_if_no_value */);
  EXPECT_EQ(6, string_map.num_names());

  EXPECT_TRUE(string_map.Lookup("a,b", &v));
  EXPECT_EQ(1, v.size());
  EXPECT_EQ(NULL, v[0]);

  EXPECT_TRUE(string_map.Lookup("a=", &v));
  EXPECT_EQ(1, v.size());
  EXPECT_STREQ("3", *v[0]);
}

TEST_F(StringMultiMapTest, TestLookupHas) {
  ConstStringStarVector v;
  EXPECT_TRUE(string_map_.Has("a"));
  ASSERT_TRUE(string_map_.Lookup("a", &v));
  ASSERT_EQ(2, v.size());
  EXPECT_EQ(GoogleString("1"), *(v[0]));
  EXPECT_EQ(GoogleString("3"), *(v[1]));
  EXPECT_EQ(NULL, string_map_.Lookup1("a"));

  ASSERT_TRUE(string_map_.Lookup("B", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(NULL, v[0]);

  ASSERT_TRUE(string_map_.Lookup("c", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(GoogleString("2"), *(v[0]));
  ASSERT_TRUE(string_map_.Lookup("d", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(GoogleString(""), *(v[0]));
  ASSERT_TRUE(string_map_.Lookup("e", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(4, v[0]->size());

  EXPECT_FALSE(string_map_.Has("foo"));
  EXPECT_FALSE(string_map_.Lookup("foo", &v));
  EXPECT_EQ(NULL, string_map_.Lookup1("foo"));

  string_map_.Add("foo", "bar");
  EXPECT_TRUE(string_map_.Has("foo"));
  ASSERT_TRUE(string_map_.Lookup("foo", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_STREQ("bar", *v[0]);
  EXPECT_STREQ("bar", *(string_map_.Lookup1("foo")));
}

TEST_F(StringMultiMapTest, TestRemove) {
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

TEST_F(StringMultiMapTest, TestEmbeddedNulsInKey) {
  static StringPiece a1("a\0_1", 4), a2("a\0_2", 4);
  string_map_.Add(a1, "100");
  string_map_.Add(a2, "200");
  ASSERT_EQ(7, string_map_.num_names());
  ASSERT_EQ(8, string_map_.num_values());

  // Test indexed lookup.
  EXPECT_STREQ(a1, string_map_.name(6));
  EXPECT_STREQ("100", *(string_map_.value(6)));
  EXPECT_STREQ(a2, string_map_.name(7));
  EXPECT_STREQ("200", *(string_map_.value(7)));
  EXPECT_STREQ("a", string_map_.name(0));
  EXPECT_STREQ("1", *(string_map_.value(0)));

  // Now test associative lookup.
  EXPECT_STREQ("100", *string_map_.Lookup1(a1));
  EXPECT_STREQ("200", *string_map_.Lookup1(a2));
  EXPECT_TRUE(string_map_.Has("a"));
  EXPECT_FALSE(string_map_.Has(StringPiece("a\0_3", 4)));
}

TEST_F(StringMultiMapTest, TestRemoveFromSortedArray) {
  static const StringPiece kRemoveVector[] = {"c", "D"};
  EXPECT_TRUE(string_map_.RemoveAllFromSortedArray(
      kRemoveVector, arraysize(kRemoveVector)));
  EXPECT_EQ(3, string_map_.num_names());
  EXPECT_TRUE(string_map_.Has("a"));
  EXPECT_TRUE(string_map_.Has("b"));
  EXPECT_TRUE(string_map_.Has("e"));
  EXPECT_FALSE(string_map_.Has("c"));
  EXPECT_FALSE(string_map_.Has("d"));
}

}  // namespace net_instaweb
