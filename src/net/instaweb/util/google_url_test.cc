/**
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
  GoogleUrlTest() : gurl_(kUrl), gurl_with_port_(kUrlWithPort) {}

  GURL gurl_;
  GURL gurl_with_port_;
};

TEST_F(GoogleUrlTest, TestSpec) {
  EXPECT_EQ(std::string(kUrl), GoogleUrl::Spec(gurl_));
  EXPECT_EQ(std::string("http://a.com/b/c/"), GoogleUrl::AllExceptLeaf(gurl_));
  EXPECT_EQ(std::string("d.ext?f=g/h"), GoogleUrl::Leaf(gurl_));
  EXPECT_EQ(std::string("http://a.com"), GoogleUrl::Origin(gurl_));
  EXPECT_EQ(std::string("/b/c/d.ext?f=g/h"), GoogleUrl::PathAndLeaf(gurl_));
  EXPECT_EQ(std::string("/b/c/d.ext"), GoogleUrl::Path(gurl_));
}

TEST_F(GoogleUrlTest, TestSpecWithPort) {
  EXPECT_EQ(std::string(kUrlWithPort), GoogleUrl::Spec(gurl_with_port_));
  EXPECT_EQ(std::string("http://a.com:8080/b/c/"),
            GoogleUrl::AllExceptLeaf(gurl_with_port_));
  EXPECT_EQ(std::string("d.ext?f=g/h"), GoogleUrl::Leaf(gurl_with_port_));
  EXPECT_EQ(std::string("http://a.com:8080"), GoogleUrl::Origin(gurl_with_port_));
  EXPECT_EQ(std::string("/b/c/d.ext?f=g/h"),
            GoogleUrl::PathAndLeaf(gurl_with_port_));
  EXPECT_EQ(std::string("/b/c/d.ext"), GoogleUrl::Path(gurl_));
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

TEST_F(GoogleUrlTest, ResolveAbsolute) {
  GURL base = GoogleUrl::Create(StringPiece("http://www.google.com"));
  ASSERT_TRUE(base.is_valid());
  GURL resolved = GoogleUrl::Resolve(base, "http://www.google.com");
  ASSERT_TRUE(resolved.is_valid());
  EXPECT_EQ(std::string("http://www.google.com/"),
            GoogleUrl::Spec(resolved));
  EXPECT_EQ(std::string("/"), GoogleUrl::Path(resolved));
}

}  // namespace net_instaweb
