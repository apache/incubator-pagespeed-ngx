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

const char kQueryUrl[] = "http://test.com/?a=1&b&c=2&d=&a=3";
const char kGoodQueryString[] = "x=1&y&z=&x=2";
const char kBadQueryString[] = "a=1\n2&b&c=\"<! ^>\"\r3&d=\t&a=3#4";
const char kSanitizedBadQueryString[] = "a=12&b&c=%22%3C!%20^%3E%223&d=&a=3";

}  // namespace

namespace net_instaweb {

class QueryParamsTest : public testing::Test {
 protected:
  QueryParamsTest() { }

  virtual void SetUp() {
    gurl_.Reset(kQueryUrl);
    query_params_.ParseFromUrl(gurl_);
  }

  GoogleUrl gurl_;
  QueryParams query_params_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QueryParamsTest);
};

TEST_F(QueryParamsTest, TestParseFromUrl) {
  ASSERT_EQ(5, query_params_.size());
  EXPECT_STREQ("a", query_params_.name(0));
  EXPECT_STREQ("1", *(query_params_.EscapedValue(0)));
  EXPECT_STREQ("b", query_params_.name(1));
  EXPECT_EQ(NULL, query_params_.EscapedValue(1));
  EXPECT_STREQ("c", query_params_.name(2));
  EXPECT_STREQ("2", *(query_params_.EscapedValue(2)));
  EXPECT_STREQ("d", query_params_.name(3));
  EXPECT_STREQ("", *(query_params_.EscapedValue(3)));
  EXPECT_STREQ("a", query_params_.name(4));
  EXPECT_STREQ("3", *(query_params_.EscapedValue(4)));
  EXPECT_STREQ(gurl_.Query(), query_params_.ToEscapedString());
}

TEST_F(QueryParamsTest, TestParseFromUntrustedString) {
  // First try a string that doesn't have anything "unsafe".
  QueryParams query_params1;
  query_params1.ParseFromUntrustedString(kGoodQueryString);
  ASSERT_EQ(4, query_params1.size());
  EXPECT_STREQ("x", query_params1.name(0));
  EXPECT_STREQ("1", *query_params1.EscapedValue(0));
  EXPECT_STREQ("y", query_params1.name(1));
  EXPECT_EQ(NULL, query_params1.EscapedValue(1));
  EXPECT_STREQ("z", query_params1.name(2));
  EXPECT_STREQ("", *query_params1.EscapedValue(2));
  EXPECT_STREQ("x", query_params1.name(3));
  EXPECT_STREQ("2", *query_params1.EscapedValue(3));
  EXPECT_STREQ(kGoodQueryString, query_params1.ToEscapedString());
  // Now try a string that has crazy stuff in it.
  QueryParams query_params2;
  query_params2.ParseFromUntrustedString(kBadQueryString);
  ASSERT_EQ(5, query_params2.size());
  EXPECT_STREQ("a", query_params2.name(0));
  EXPECT_STREQ("12", *query_params2.EscapedValue(0));
  EXPECT_STREQ("b", query_params2.name(1));
  EXPECT_EQ(NULL, query_params2.EscapedValue(1));
  EXPECT_STREQ("c", query_params2.name(2));
  EXPECT_STREQ("%22%3C!%20^%3E%223", *query_params2.EscapedValue(2));
  EXPECT_STREQ("d", query_params2.name(3));
  EXPECT_STREQ("", *query_params2.EscapedValue(3));
  EXPECT_STREQ("a", query_params2.name(4));
  EXPECT_STREQ("3", *query_params2.EscapedValue(4));
  EXPECT_STREQ(kSanitizedBadQueryString, query_params2.ToEscapedString());
}

TEST_F(QueryParamsTest, TestLookup) {
  ConstStringStarVector v;
  ASSERT_TRUE(query_params_.LookupEscaped("a", &v));
  ASSERT_EQ(2, v.size());
  EXPECT_STREQ("1", *(v[0]));
  EXPECT_STREQ("3", *(v[1]));
  ASSERT_TRUE(query_params_.LookupEscaped("b", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(NULL, v[0]);
  ASSERT_TRUE(query_params_.LookupEscaped("c", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_STREQ("2", *(v[0]));
  ASSERT_TRUE(query_params_.LookupEscaped("d", &v));
  ASSERT_EQ(1, v.size());
  EXPECT_STREQ("", *(v[0]));
}

TEST_F(QueryParamsTest, TestRemove) {
  query_params_.RemoveAll("a");
  EXPECT_STREQ("b&c=2&d=", query_params_.ToEscapedString());
  EXPECT_EQ(3, query_params_.size());
  query_params_.RemoveAll("b");
  EXPECT_STREQ("c=2&d=", query_params_.ToEscapedString());
  EXPECT_EQ(2, query_params_.size());
  query_params_.RemoveAll("c");
  EXPECT_STREQ("d=", query_params_.ToEscapedString());
  EXPECT_EQ(1, query_params_.size());
  query_params_.RemoveAll("d");
  EXPECT_STREQ("", query_params_.ToEscapedString());
  EXPECT_EQ(0, query_params_.size());
}

TEST_F(QueryParamsTest, TestClear) {
  query_params_.Clear();
  EXPECT_STREQ("", query_params_.ToEscapedString());
  EXPECT_EQ(0, query_params_.size());
}

TEST_F(QueryParamsTest, TestAEqualsBEquals1) {
  GoogleUrl gurl("http://test.com/?a=b=1");
  ASSERT_TRUE(gurl.IsWebValid());
  query_params_.Clear();
  query_params_.ParseFromUrl(gurl);
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
  query_params_.ParseFromUrl(gurl);
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
  query_params_.ParseFromUrl(gurl);
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
