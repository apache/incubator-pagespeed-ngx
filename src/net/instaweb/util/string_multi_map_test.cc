// Copyright 2010-2011 and onwards Google Inc.
// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test StringMultiMap.

#include "net/instaweb/util/public/string_multi_map.h"
#include <algorithm>
#include "base/logging.h"
#include "net/instaweb/util/public/gtest.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

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
  }

  StringMultiMapInsensitive string_map_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StringMultiMapTest);
};

TEST_F(StringMultiMapTest, TestAdd) {
  ASSERT_EQ(4, string_map_.num_names());
  ASSERT_EQ(5, string_map_.num_values());
  EXPECT_EQ(std::string("a"), string_map_.name(0));
  EXPECT_EQ(std::string("1"), string_map_.value(0));
  EXPECT_EQ(std::string("b"), string_map_.name(1));
  EXPECT_EQ(NULL, string_map_.value(1));
  EXPECT_EQ(std::string("C"), string_map_.name(2));
  EXPECT_EQ(std::string("2"), string_map_.value(2));
  EXPECT_EQ(std::string("d"), string_map_.name(3));
  EXPECT_EQ(std::string(""), string_map_.value(3));
  EXPECT_EQ(std::string("a"), string_map_.name(4));
  EXPECT_EQ(std::string("3"), string_map_.value(4));
}

TEST_F(StringMultiMapTest, TestLookup) {
  CharStarVector v;
  ASSERT_TRUE(string_map_.Lookup("a", &v));
  ASSERT_EQ(2, v.size());
  EXPECT_EQ(std::string("1"), v[0]);
  EXPECT_EQ(std::string("3"), v[1]);
  ASSERT_TRUE(string_map_.Lookup("b", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(NULL, v[0]);
  ASSERT_TRUE(string_map_.Lookup("C", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(std::string("2"), v[0]);
  ASSERT_TRUE(string_map_.Lookup("d", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(std::string(""), v[0]);
}

TEST_F(StringMultiMapTest, TestRemove) {
  CharStarVector v;
  string_map_.RemoveAll("a");
  EXPECT_EQ(3, string_map_.num_names());
  EXPECT_EQ(3, string_map_.num_values());
  string_map_.RemoveAll("b");
  EXPECT_EQ(2, string_map_.num_names());
  string_map_.RemoveAll("c");
  EXPECT_EQ(1, string_map_.num_names());
  string_map_.RemoveAll("D");
  EXPECT_EQ(0, string_map_.num_names());
}

TEST_F(StringMultiMapTest, TestClear) {
  string_map_.Clear();
  EXPECT_EQ(0, string_map_.num_names());
  EXPECT_EQ(0, string_map_.num_values());
}

}  // namespace net_instaweb
