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

// Unit-test QueryParams.

#include "pagespeed/kernel/http/query_params.h"

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/google_url.h"

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
  EXPECT_EQ(GoogleString("a"), query_params_.name(0));
  EXPECT_EQ(GoogleString("1"), *(query_params_.EscapedValue(0)));
  EXPECT_EQ(GoogleString("b"), query_params_.name(1));
  EXPECT_EQ(NULL, query_params_.EscapedValue(1));
  EXPECT_EQ(GoogleString("c"), query_params_.name(2));
  EXPECT_EQ(GoogleString("2"), *(query_params_.EscapedValue(2)));
  EXPECT_EQ(GoogleString("d"), query_params_.name(3));
  EXPECT_EQ(GoogleString(""), *(query_params_.EscapedValue(3)));
  EXPECT_EQ(GoogleString("a"), query_params_.name(4));
  EXPECT_EQ(GoogleString("3"), *(query_params_.EscapedValue(4)));
  EXPECT_EQ(kQueryString, query_params_.ToEscapedString());
}

TEST_F(QueryParamsTest, TestLookup) {
  ConstStringStarVector v;
  ASSERT_TRUE(query_params_.LookupEscaped("a", &v));
  ASSERT_EQ(2, v.size());
  EXPECT_EQ(GoogleString("1"), *(v[0]));
  EXPECT_EQ(GoogleString("3"), *(v[1]));
  ASSERT_TRUE(query_params_.LookupEscaped("b", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(NULL, v[0]);
  ASSERT_TRUE(query_params_.LookupEscaped("c", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(GoogleString("2"), *(v[0]));
  ASSERT_TRUE(query_params_.LookupEscaped("d", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(GoogleString(""), *(v[0]));
}

TEST_F(QueryParamsTest, TestRemove) {
  query_params_.RemoveAll("a");
  EXPECT_EQ("b&c=2&d=", query_params_.ToEscapedString());
  EXPECT_EQ(3, query_params_.size());
  query_params_.RemoveAll("b");
  EXPECT_EQ("c=2&d=", query_params_.ToEscapedString());
  EXPECT_EQ(2, query_params_.size());
  query_params_.RemoveAll("c");
  EXPECT_EQ("d=", query_params_.ToEscapedString());
  EXPECT_EQ(1, query_params_.size());
  query_params_.RemoveAll("d");
  EXPECT_EQ("", query_params_.ToEscapedString());
  EXPECT_EQ(0, query_params_.size());
}

TEST_F(QueryParamsTest, TestClear) {
  query_params_.Clear();
  EXPECT_EQ("", query_params_.ToEscapedString());
  EXPECT_EQ(0, query_params_.size());
}

TEST_F(QueryParamsTest, TestAEqualsBEquals1) {
  query_params_.Clear();
  query_params_.Parse("a=b=1");
  ASSERT_EQ(1, query_params_.size());
  ASSERT_EQ(GoogleString("a"), query_params_.name(0));
  ASSERT_EQ(GoogleString("b=1"), *(query_params_.EscapedValue(0)));
}

TEST_F(QueryParamsTest, EscapedQuery) {
  GoogleUrl gurl("http://example.com/a?b=<value requiring escapes>");
  ASSERT_TRUE(gurl.IsWebValid());

  // Note that GoogleUrl::Escape will turn " " into "+", but the escaper
  // built into the GoogleUrl constructor will turn " " into "%20".
  EXPECT_STREQ("b=%3Cvalue%20requiring%20escapes%3E", gurl.Query());

  query_params_.Clear();
  query_params_.Parse(gurl.Query());
  ASSERT_EQ(1, query_params_.size());
  EXPECT_STREQ("b", query_params_.name(0));
  GoogleString unescaped_value;
  ASSERT_TRUE(query_params_.UnescapedValue(0, &unescaped_value));
  EXPECT_STREQ("<value requiring escapes>", unescaped_value);
}

TEST_F(QueryParamsTest, QueryParamWithAndWithoutValues) {
  GoogleUrl gurl("http://example.com/a?b=c&d&e=&f");
  ASSERT_TRUE(gurl.IsWebValid());
  query_params_.Clear();
  query_params_.Parse(gurl.Query());
  GoogleString unescaped;

  // A normal query param, b=c, we get back "c".
  EXPECT_STREQ("b", query_params_.name(0));
  EXPECT_STREQ("c", *query_params_.EscapedValue(0));
  EXPECT_TRUE(query_params_.UnescapedValue(0, &unescaped));
  EXPECT_STREQ("c", unescaped);

  // "d" does not have an equals sign.  NULL is returned for the value.
  EXPECT_STREQ("d", query_params_.name(1));
  EXPECT_TRUE(NULL == query_params_.EscapedValue(1));
  EXPECT_FALSE(query_params_.UnescapedValue(1, &unescaped));

  // "e" has an equals sign, but no value before the next "&", so
  // we get back an empty string.
  EXPECT_STREQ("e", query_params_.name(2));
  EXPECT_STREQ("", *query_params_.EscapedValue(2));
  EXPECT_TRUE(query_params_.UnescapedValue(2, &unescaped));
  EXPECT_STREQ("", unescaped);
}

}  // namespace net_instaweb
