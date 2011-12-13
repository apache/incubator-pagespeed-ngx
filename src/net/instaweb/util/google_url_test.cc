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

// Unit-test the string-splitter.

#include "net/instaweb/util/public/google_url.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kUrl[] = "http://a.com/b/c/d.ext?f=g/h";
const char kUrlWithPort[] = "http://a.com:8080/b/c/d.ext?f=g/h";

}  // namespace

namespace net_instaweb {

class GoogleUrlTest : public testing::Test {
 protected:
  GoogleUrlTest()
  : gurl_(kUrl),
    gurl_with_port_(kUrlWithPort)
  {}

  void TestCopyAndAddQueryParamCase(const char* before, const char* after) {
    GoogleUrl before_url(before);
    StringPiece before_url_original(before_url.UncheckedSpec());
    scoped_ptr<GoogleUrl> after_url(before_url.CopyAndAddQueryParam("r", "s"));
    EXPECT_EQ(after_url->UncheckedSpec(), after);
    EXPECT_TRUE(after_url->is_valid());
    EXPECT_EQ(before_url_original, before_url.UncheckedSpec());
  }

  void TestAllExceptQueryCase(const char* before, const char* after) {
    GoogleUrl before_url(before);
    EXPECT_EQ(before_url.AllExceptQuery(), GoogleString(after));
  }

  void TestAllAfterQueryCase(const char* before, const char* after) {
    GoogleUrl before_url(before);
    EXPECT_EQ(before_url.AllAfterQuery(), GoogleString(after));
  }

  GoogleUrl gurl_;
  GoogleUrl gurl_with_port_;
};

// Document which sorts of strings are and are not valid.
TEST_F(GoogleUrlTest, TestNotValid) {
  GoogleUrl invalid_url("Hello, world!");
  EXPECT_FALSE(invalid_url.is_valid());

  GoogleUrl relative_url1("/foo/bar.html");
  EXPECT_FALSE(relative_url1.is_valid());

  GoogleUrl relative_url2("foo/bar.html");
  EXPECT_FALSE(relative_url2.is_valid());

  GoogleUrl relative_url3("bar.html");
  EXPECT_FALSE(relative_url3.is_valid());

  // Aparently this is close enough to be considered valid, but not standard.
  GoogleUrl proxy_filename("proxy:http://www.example.com/index.html");
  EXPECT_TRUE(proxy_filename.is_valid());
  EXPECT_FALSE(proxy_filename.is_standard());
}

TEST_F(GoogleUrlTest, TestSpec) {
  EXPECT_EQ(GoogleString(kUrl), gurl_.Spec());
  EXPECT_EQ(GoogleString("http://a.com/b/c/"), gurl_.AllExceptLeaf());
  EXPECT_EQ(GoogleString("d.ext?f=g/h"), gurl_.LeafWithQuery());
  EXPECT_EQ(GoogleString("d.ext"), gurl_.LeafSansQuery());
  EXPECT_EQ(GoogleString("http://a.com"), gurl_.Origin());
  EXPECT_EQ(GoogleString("/b/c/d.ext?f=g/h"), gurl_.PathAndLeaf());
  EXPECT_EQ(GoogleString("/b/c/d.ext"), gurl_.PathSansQuery());
}


TEST_F(GoogleUrlTest, TestSpecWithPort) {
  EXPECT_EQ(GoogleString(kUrlWithPort), gurl_with_port_.Spec());
  EXPECT_EQ(GoogleString("http://a.com:8080/b/c/"),
            gurl_with_port_.AllExceptLeaf());
  EXPECT_EQ(GoogleString("d.ext?f=g/h"),
            gurl_with_port_.LeafWithQuery());
  EXPECT_EQ(GoogleString("d.ext"), gurl_with_port_.LeafSansQuery());
  EXPECT_EQ(GoogleString("http://a.com:8080"),
            gurl_with_port_.Origin());
  EXPECT_EQ(GoogleString("/b/c/d.ext?f=g/h"),
            gurl_with_port_.PathAndLeaf());
  EXPECT_EQ(GoogleString("/b/c/d.ext"), gurl_.PathSansQuery());
  EXPECT_EQ(GoogleString("/b/c/"), gurl_.PathSansLeaf());
}

TEST_F(GoogleUrlTest, TestDots) {
  GoogleUrl url1("http://www.example.com/foo/../bar/baz/./../index.html");
  EXPECT_EQ("http://www.example.com/bar/index.html", url1.Spec());

  GoogleUrl url2("http://www.example.com/../../../../..");
  EXPECT_EQ("http://www.example.com/", url2.Spec());

  GoogleUrl url3("http://www.example.com/foo/./bar/index.html");
  EXPECT_EQ("http://www.example.com/foo/bar/index.html", url3.Spec());
}

// Note: These tests are basically gold tests done for documentation more
// than expectation. If they change that should be fine, we just want to
// know what is decoded and what is not.
TEST_F(GoogleUrlTest, TestDecode) {
  // Not "%20" -> " "
  GoogleUrl url1("http://www.example.com/foo%20bar.html");
  EXPECT_EQ("http://www.example.com/foo%20bar.html", url1.Spec());
  // "%2E" -> "."
  GoogleUrl url2("http://www.example.com/foo/%2E%2E/bar.html");
  EXPECT_EQ("http://www.example.com/bar.html", url2.Spec());
  GoogleUrl url2b("http://www.example.com/foo/%2e%2e/bar.html");
  EXPECT_EQ("http://www.example.com/bar.html", url2b.Spec());
  GoogleUrl url2c("http://www.example.com/%2e%2e/%2e%2e/some/file/path/");
  EXPECT_EQ("http://www.example.com/some/file/path/", url2c.Spec());
  // Not "%2F" -> "/"
  GoogleUrl url3("http://www.example.com/foo%2Fbar.html");
  EXPECT_EQ("http://www.example.com/foo%2Fbar.html", url3.Spec());
  GoogleUrl url3b("http://www.example.com/foo%2fbar.html");
  EXPECT_EQ("http://www.example.com/foo%2fbar.html", url3b.Spec());
  // Other
  GoogleUrl url4("http://www.example.com/%53%2D%38%25%32%35");
  EXPECT_EQ("http://www.example.com/S-8%2525", url4.Spec());
}

TEST_F(GoogleUrlTest, TestCopyAndAddQueryParam) {
  TestCopyAndAddQueryParamCase("http://a.com/b/c/d.ext",
                               "http://a.com/b/c/d.ext?r=s");

  TestCopyAndAddQueryParamCase("http://a.com/b/c/d.ext?p=q",
                               "http://a.com/b/c/d.ext?p=q&r=s");

  TestCopyAndAddQueryParamCase("http://a.com",
                               "http://a.com/?r=s");

  TestCopyAndAddQueryParamCase("http://a.com?p=q",
                               "http://a.com/?p=q&r=s");

  TestCopyAndAddQueryParamCase("http://a.com/b/c/d.ext?p=q#ref",
                               "http://a.com/b/c/d.ext?p=q&r=s#ref");
}

TEST_F(GoogleUrlTest, TestAllExceptQuery) {
  TestAllExceptQueryCase("http://a.com/b/c/d.ext",
                         "http://a.com/b/c/d.ext");

  TestAllExceptQueryCase("http://a.com/b/c/d.ext?p=p&q=q",
                         "http://a.com/b/c/d.ext");

  TestAllExceptQueryCase("http://a.com?p=p&q=q",
                         "http://a.com/");

  TestAllExceptQueryCase("invalid_url_string",
                         "");
}

TEST_F(GoogleUrlTest, TestAllAfterQuery) {
  TestAllAfterQueryCase("http://a.com/b/c/d.ext",
                        "");

  TestAllAfterQueryCase("http://a.com/b/c/d.ext?p=p&q=q",
                        "");

  TestAllAfterQueryCase("http://a.com/b/c/d.ext?p=p&q=q#ref",
                        "#ref");

  TestAllAfterQueryCase("http://a.com/b/c/d.ext?p=p&q=q#ref1#ref2",
                        "#ref1#ref2");

  TestAllAfterQueryCase("http://a.com#ref",
                        "#ref");

  TestAllAfterQueryCase("invalid_url_string",
                         "");
}

TEST_F(GoogleUrlTest, TestTrivialAllExceptLeaf) {
  GoogleUrl queryless("http://a.com/b/c/d.ext");
  EXPECT_EQ(GoogleString("http://a.com/b/c/"), queryless.AllExceptLeaf());
  GoogleUrl queryful("http://a.com/b/c/d.ext?p=p&q=q");
  EXPECT_EQ(GoogleString("http://a.com/b/c/"), queryful.AllExceptLeaf());
}

TEST_F(GoogleUrlTest, TestAllExceptLeafIsIdempotent) {
  // In various places the code takes either a full URL or the URL's
  // AllExceptLeaf and depends on the fact that calling AllExceptLeaf on either
  // gives you the same thing. This test is catch any breakage to that.
  GoogleUrl queryless("http://a.com/b/c/d.ext");
  StringPiece all_except_leaf(queryless.AllExceptLeaf());
  GoogleUrl all_except_leaf_url(all_except_leaf);
  EXPECT_TRUE(all_except_leaf_url.is_valid());
  EXPECT_EQ(all_except_leaf, all_except_leaf_url.AllExceptLeaf());
}

TEST_F(GoogleUrlTest, TestTrivialLeafSansQuery) {
  GoogleUrl queryless("http://a.com/b/c/d.ext");
  EXPECT_EQ(GoogleString("d.ext"), queryless.LeafSansQuery());
}

TEST_F(GoogleUrlTest, ResolveRelative) {
  GoogleUrl base(StringPiece("http://www.google.com"));
  ASSERT_TRUE(base.is_valid());
  GoogleUrl resolved(base, "test.html");
  ASSERT_TRUE(resolved.is_valid());
  EXPECT_EQ(GoogleString("http://www.google.com/test.html"),
            resolved.Spec());
  EXPECT_EQ(GoogleString("/test.html"), resolved.PathSansQuery());
}

TEST_F(GoogleUrlTest, ResolveAbsolute) {
  GoogleUrl base(StringPiece("http://www.google.com"));
  ASSERT_TRUE(base.is_valid());
  GoogleUrl resolved(base, "http://www.google.com");
  ASSERT_TRUE(resolved.is_valid());
  EXPECT_EQ(GoogleString("http://www.google.com/"),
            resolved.Spec());
  EXPECT_EQ(GoogleString("/"), resolved.PathSansQuery());
}

TEST_F(GoogleUrlTest, TestReset) {
  GoogleUrl url("http://www.google.com");
  EXPECT_TRUE(url.is_valid());
  url.Clear();
  EXPECT_FALSE(url.is_valid());
  EXPECT_TRUE(url.is_empty());
  EXPECT_TRUE(url.UncheckedSpec().empty());
}

TEST_F(GoogleUrlTest, TestHostAndPort) {
  const char kExpected5[] = "example.com:5";
  EXPECT_EQ(kExpected5, GoogleUrl("http://example.com:5").HostAndPort());
  EXPECT_EQ(kExpected5, GoogleUrl("http://example.com:5/a/b").HostAndPort());
  EXPECT_EQ(kExpected5, GoogleUrl("https://example.com:5").HostAndPort());
  const char kExpected[] = "example.com";
  EXPECT_EQ(kExpected, GoogleUrl("http://example.com").HostAndPort());
  EXPECT_EQ(kExpected, GoogleUrl("http://example.com/a/b").HostAndPort());
  EXPECT_EQ(kExpected, GoogleUrl("https://example.com").HostAndPort());
}

TEST_F(GoogleUrlTest, TestPort) {
  EXPECT_EQ(5, GoogleUrl("http://example.com:5").IntPort());
  EXPECT_EQ(5, GoogleUrl("http://example.com:5").EffectiveIntPort());
  EXPECT_EQ(5, GoogleUrl("https://example.com:5").IntPort());
  EXPECT_EQ(5, GoogleUrl("https://example.com:5").EffectiveIntPort());
  EXPECT_EQ(-1, GoogleUrl("http://example.com").IntPort());
  EXPECT_EQ(80, GoogleUrl("http://example.com").EffectiveIntPort());
  EXPECT_EQ(-1, GoogleUrl("https://example.com").IntPort());
  EXPECT_EQ(443, GoogleUrl("https://example.com").EffectiveIntPort());
}

TEST_F(GoogleUrlTest, TestExtraSlash) {
  GoogleUrl base("http://www.example.com");
  GoogleUrl example_extra_slash(
      base, "http://www.example.com//extra_slash/index.html");
  GoogleUrl a_extra_slash(base, "http://a.com//extra_slash/index.html");
  EXPECT_STREQ("http://www.example.com/extra_slash/index.html",
               example_extra_slash.Spec());
  EXPECT_STREQ("http://a.com/extra_slash/index.html", a_extra_slash.Spec());
}

}  // namespace net_instaweb
