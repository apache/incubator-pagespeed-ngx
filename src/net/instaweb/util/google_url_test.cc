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
    gurl_with_port_(kUrlWithPort),
    cgurl_(kUrl),
    cgurl_with_port_(kUrlWithPort)
  {}

  GURL gurl_;
  GURL gurl_with_port_;

  GoogleUrl cgurl_;
  GoogleUrl cgurl_with_port_;
};

TEST_F(GoogleUrlTest, TestSpec) {
  EXPECT_EQ(std::string(kUrl), GoogleUrl::Spec(gurl_));
  EXPECT_EQ(std::string("http://a.com/b/c/"), GoogleUrl::AllExceptLeaf(gurl_));
  EXPECT_EQ(std::string("d.ext?f=g/h"), GoogleUrl::LeafWithQuery(gurl_));
  EXPECT_EQ(std::string("d.ext"), GoogleUrl::LeafSansQuery(gurl_));
  EXPECT_EQ(std::string("http://a.com"), GoogleUrl::Origin(gurl_));
  EXPECT_EQ(std::string("/b/c/d.ext?f=g/h"), GoogleUrl::PathAndLeaf(gurl_));
  EXPECT_EQ(std::string("/b/c/d.ext"), GoogleUrl::Path(gurl_));
}

TEST_F(GoogleUrlTest, TestSpecClass) {
  EXPECT_EQ(std::string(kUrl), cgurl_.Spec());
  EXPECT_EQ(std::string("http://a.com/b/c/"), cgurl_.AllExceptLeaf());
  EXPECT_EQ(std::string("d.ext?f=g/h"), cgurl_.LeafWithQuery());
  EXPECT_EQ(std::string("d.ext"), cgurl_.LeafSansQuery());
  EXPECT_EQ(std::string("http://a.com"), cgurl_.Origin());
  EXPECT_EQ(std::string("/b/c/d.ext?f=g/h"), cgurl_.PathAndLeaf());
  EXPECT_EQ(std::string("/b/c/d.ext"), cgurl_.Path());
}


TEST_F(GoogleUrlTest, TestSpecWithPort) {
  EXPECT_EQ(std::string(kUrlWithPort), GoogleUrl::Spec(gurl_with_port_));
  EXPECT_EQ(std::string("http://a.com:8080/b/c/"),
            GoogleUrl::AllExceptLeaf(gurl_with_port_));
  EXPECT_EQ(std::string("d.ext?f=g/h"),
            GoogleUrl::LeafWithQuery(gurl_with_port_));
  EXPECT_EQ(std::string("d.ext"), GoogleUrl::LeafSansQuery(gurl_with_port_));
  EXPECT_EQ(std::string("http://a.com:8080"),
            GoogleUrl::Origin(gurl_with_port_));
  EXPECT_EQ(std::string("/b/c/d.ext?f=g/h"),
            GoogleUrl::PathAndLeaf(gurl_with_port_));
  EXPECT_EQ(std::string("/b/c/d.ext"), GoogleUrl::Path(gurl_));
  EXPECT_EQ(std::string("/b/c/"), GoogleUrl::PathSansLeaf(gurl_));
}

TEST_F(GoogleUrlTest, TestSpecWithPortClass) {
  EXPECT_EQ(std::string(kUrlWithPort), cgurl_with_port_.Spec());
  EXPECT_EQ(std::string("http://a.com:8080/b/c/"),
            cgurl_with_port_.AllExceptLeaf());
  EXPECT_EQ(std::string("d.ext?f=g/h"),
            cgurl_with_port_.LeafWithQuery());
  EXPECT_EQ(std::string("d.ext"), cgurl_with_port_.LeafSansQuery());
  EXPECT_EQ(std::string("http://a.com:8080"),
            cgurl_with_port_.Origin());
  EXPECT_EQ(std::string("/b/c/d.ext?f=g/h"),
            cgurl_with_port_.PathAndLeaf());
  EXPECT_EQ(std::string("/b/c/d.ext"), cgurl_.Path());
  EXPECT_EQ(std::string("/b/c/"), cgurl_.PathSansLeaf());
}

TEST_F(GoogleUrlTest, TestTrivialLeafSansQuery) {
  GURL queryless("http://a.com/b/c/d.ext");
  EXPECT_EQ(std::string("d.ext"), GoogleUrl::LeafSansQuery(queryless));
}

TEST_F(GoogleUrlTest, TestTrivialLeafSansQueryClass) {
  GoogleUrl queryless("http://a.com/b/c/d.ext");
  EXPECT_EQ(std::string("d.ext"), queryless.LeafSansQuery());
}

TEST_F(GoogleUrlTest, ResolveRelative) {
  GURL base = GoogleUrl::Create(StringPiece("http://www.google.com"));
  ASSERT_TRUE(base.is_valid());
  GURL resolved = GoogleUrl::Resolve(base, "test.html");
  ASSERT_TRUE(resolved.is_valid());
  EXPECT_EQ(std::string("http://www.google.com/test.html"),
            GoogleUrl::Spec(resolved));
  EXPECT_EQ(std::string("/test.html"), GoogleUrl::Path(resolved));
}

TEST_F(GoogleUrlTest, ResolveRelativeClass) {
  GoogleUrl base(StringPiece("http://www.google.com"));
  ASSERT_TRUE(base.is_valid());
  GoogleUrl resolved(base, "test.html");
  ASSERT_TRUE(resolved.is_valid());
  EXPECT_EQ(std::string("http://www.google.com/test.html"),
            resolved.Spec());
  EXPECT_EQ(std::string("/test.html"), resolved.Path());
}

TEST_F(GoogleUrlTest, ResolveAbsolute) {
  GURL base = GoogleUrl::Create(StringPiece("http://www.google.com"));
  ASSERT_TRUE(base.is_valid());
  GURL resolved = GoogleUrl::Resolve(base, "http://www.google.com");
  ASSERT_TRUE(resolved.is_valid());
  EXPECT_EQ(std::string("http://www.google.com/"),
            GoogleUrl::Spec(resolved));
  EXPECT_EQ(std::string("/"), GoogleUrl::Path(resolved));
}

TEST_F(GoogleUrlTest, ResolveAbsoluteClass) {
  GoogleUrl base(StringPiece("http://www.google.com"));
  ASSERT_TRUE(base.is_valid());
  GoogleUrl resolved(base, "http://www.google.com");
  ASSERT_TRUE(resolved.is_valid());
  EXPECT_EQ(std::string("http://www.google.com/"),
            resolved.Spec());
  EXPECT_EQ(std::string("/"), resolved.Path());
}

}  // namespace net_instaweb
