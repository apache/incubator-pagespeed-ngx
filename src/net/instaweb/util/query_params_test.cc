// Copyright 2010 and onwards Google Inc.
// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test QueryParams.

#include "net/instaweb/util/public/query_params.h"
#include <algorithm>
#include "base/basictypes.h"
#include "base/logging.h"
#include "net/instaweb/util/public/gtest.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace {

const char kQueryString[] = "a=1&b&c=2&d=&a=3";

}  // namespace

namespace net_instaweb {

class QueryParamsTest : public testing::Test {
 protected:
  QueryParamsTest() { }

  virtual void SetUp() {
    query_params_.Parse(kQueryString);
  }

  QueryParams query_params_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QueryParamsTest);
};

TEST_F(QueryParamsTest, TestParse) {
  ASSERT_EQ(5, query_params_.size());
  EXPECT_EQ(std::string("a"), query_params_.name(0));
  EXPECT_EQ(std::string("1"), query_params_.value(0));
  EXPECT_EQ(std::string("b"), query_params_.name(1));
  EXPECT_EQ(NULL, query_params_.value(1));
  EXPECT_EQ(std::string("c"), query_params_.name(2));
  EXPECT_EQ(std::string("2"), query_params_.value(2));
  EXPECT_EQ(std::string("d"), query_params_.name(3));
  EXPECT_EQ(std::string(""), query_params_.value(3));
  EXPECT_EQ(std::string("a"), query_params_.name(4));
  EXPECT_EQ(std::string("3"), query_params_.value(4));
  EXPECT_EQ(kQueryString, query_params_.ToString());
}

TEST_F(QueryParamsTest, TestLookup) {
  CharStarVector v;
  ASSERT_TRUE(query_params_.Lookup("a", &v));
  ASSERT_EQ(2, v.size());
  EXPECT_EQ(std::string("1"), v[0]);
  EXPECT_EQ(std::string("3"), v[1]);
  ASSERT_TRUE(query_params_.Lookup("b", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(NULL, v[0]);
  ASSERT_TRUE(query_params_.Lookup("c", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(std::string("2"), v[0]);
  ASSERT_TRUE(query_params_.Lookup("d", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(std::string(""), v[0]);
}

TEST_F(QueryParamsTest, TestRemove) {
  CharStarVector v;
  query_params_.RemoveAll("a");
  EXPECT_EQ("b&c=2&d=", query_params_.ToString());
  EXPECT_EQ(3, query_params_.size());
  query_params_.RemoveAll("b");
  EXPECT_EQ("c=2&d=", query_params_.ToString());
  EXPECT_EQ(2, query_params_.size());
  query_params_.RemoveAll("c");
  EXPECT_EQ("d=", query_params_.ToString());
  EXPECT_EQ(1, query_params_.size());
  query_params_.RemoveAll("d");
  EXPECT_EQ("", query_params_.ToString());
  EXPECT_EQ(0, query_params_.size());
}

TEST_F(QueryParamsTest, TestClear) {
  query_params_.Clear();
  EXPECT_EQ("", query_params_.ToString());
  EXPECT_EQ(0, query_params_.size());
}

TEST_F(QueryParamsTest, TestAEqualsBEquals1) {
  query_params_.Clear();
  query_params_.Parse("a=b=1");
  ASSERT_EQ(1, query_params_.size());
  ASSERT_EQ(std::string("a"), query_params_.name(0));
  ASSERT_EQ(std::string("b=1"), query_params_.value(0));
}

}  // namespace net_instaweb
