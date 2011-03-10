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
#include "net/instaweb/util/public/gtest.h"

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
  EXPECT_EQ(std::string(kUrl), gurl_.Spec());
  EXPECT_EQ(std::string("http://a.com/b/c/"), gurl_.AllExceptLeaf());
  EXPECT_EQ(std::string("d.ext?f=g/h"), gurl_.LeafWithQuery());
  EXPECT_EQ(std::string("d.ext"), gurl_.LeafSansQuery());
  EXPECT_EQ(std::string("http://a.com"), gurl_.Origin());
  EXPECT_EQ(std::string("/b/c/d.ext?f=g/h"), gurl_.PathAndLeaf());
  EXPECT_EQ(std::string("/b/c/d.ext"), gurl_.Path());
}


TEST_F(GoogleUrlTest, TestSpecWithPort) {
  EXPECT_EQ(std::string(kUrlWithPort), gurl_with_port_.Spec());
  EXPECT_EQ(std::string("http://a.com:8080/b/c/"),
            gurl_with_port_.AllExceptLeaf());
  EXPECT_EQ(std::string("d.ext?f=g/h"),
            gurl_with_port_.LeafWithQuery());
  EXPECT_EQ(std::string("d.ext"), gurl_with_port_.LeafSansQuery());
  EXPECT_EQ(std::string("http://a.com:8080"),
            gurl_with_port_.Origin());
  EXPECT_EQ(std::string("/b/c/d.ext?f=g/h"),
            gurl_with_port_.PathAndLeaf());
  EXPECT_EQ(std::string("/b/c/d.ext"), gurl_.Path());
  EXPECT_EQ(std::string("/b/c/"), gurl_.PathSansLeaf());
}

TEST_F(GoogleUrlTest, TestTrivialLeafSansQuery) {
  GoogleUrl queryless("http://a.com/b/c/d.ext");
  EXPECT_EQ(std::string("d.ext"), queryless.LeafSansQuery());
}

TEST_F(GoogleUrlTest, ResolveRelative) {
  GoogleUrl base(StringPiece("http://www.google.com"));
  ASSERT_TRUE(base.is_valid());
  GoogleUrl resolved(base, "test.html");
  ASSERT_TRUE(resolved.is_valid());
  EXPECT_EQ(std::string("http://www.google.com/test.html"),
            resolved.Spec());
  EXPECT_EQ(std::string("/test.html"), resolved.Path());
}

TEST_F(GoogleUrlTest, ResolveAbsolute) {
  GoogleUrl base(StringPiece("http://www.google.com"));
  ASSERT_TRUE(base.is_valid());
  GoogleUrl resolved(base, "http://www.google.com");
  ASSERT_TRUE(resolved.is_valid());
  EXPECT_EQ(std::string("http://www.google.com/"),
            resolved.Spec());
  EXPECT_EQ(std::string("/"), resolved.Path());
}

}  // namespace net_instaweb
