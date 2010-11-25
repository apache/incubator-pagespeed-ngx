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

#include "net/instaweb/util/public/url_escaper.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

// We pass through a few special characters unchanged, and we
// accept those characters, plus ',', as acceptable in the encoded
// URLs.
static const char kAcceptableSpecialChars[] = ",._+-=&?";
static const char kPassThruChars[]          =  "._+-=&?";

}  // namespace

namespace net_instaweb {

class UrlEscaperTest : public testing::Test {
 protected:
  void CheckEncoding(const StringPiece& url) {
    std::string encoded, decoded;
    escaper_.EncodeToUrlSegment(url, &encoded);

    // Make sure there are only alphanumerics and _+-=%
    for (size_t i = 0; i < encoded.size(); ++i) {
      char c = encoded[i];
      EXPECT_TRUE(isalnum(c) || (strchr(kAcceptableSpecialChars, c) != NULL));
    }

    EXPECT_TRUE(escaper_.DecodeFromUrlSegment(encoded, &decoded));
    EXPECT_EQ(url, decoded);
  }

  // Some basic text should be completely unchanged upon encode/decode.
  void CheckUnchanged(const StringPiece& url) {
    std::string encoded, decoded;
    escaper_.EncodeToUrlSegment(url, &encoded);
    EXPECT_EQ(url, encoded);
    EXPECT_TRUE(escaper_.DecodeFromUrlSegment(encoded, &decoded));
    EXPECT_EQ(url, decoded);
  }

  std::string Decode(const StringPiece& encoding) {
    std::string decoded;
    EXPECT_TRUE(escaper_.DecodeFromUrlSegment(encoding, &decoded));
    return decoded;
  }

  UrlEscaper escaper_;
};

TEST_F(UrlEscaperTest, TestUrls) {
  CheckEncoding("http://www.google.com");
  // Test encoding of % and lack of leading http:// (beware of double encoding):
  CheckEncoding("//web.mit.edu/foo.cgi?bar%baz");
  CheckEncoding("http://www.foo.bar/z1234/b_c.d?e=f&g=h");
  CheckEncoding("http://china.com/\u591a\u5e74\u7ecf\u5178\u5361\u7247\u673a");
  CheckEncoding("http://中国 汪 世 孟");
  CheckEncoding("/static/f.1.js?v=120");
  CheckEncoding("!@#$%^&*()_+=-[]{}?><,./");
}

TEST_F(UrlEscaperTest, TestUnchanged) {
  CheckUnchanged("abcdefghijklmnopqrstuvwxyz");
  CheckUnchanged("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  CheckUnchanged("0123456789");
  CheckUnchanged("?&=+-_");
  CheckUnchanged(kPassThruChars);
}

TEST_F(UrlEscaperTest, LegacyDecode) {
  EXPECT_EQ("a.css", Decode("a,s"));
  EXPECT_EQ("b.jpg", Decode("b,j"));
  EXPECT_EQ("c.png", Decode("c,p"));
  EXPECT_EQ("d.gif", Decode("d,g"));
  EXPECT_EQ("e.jpeg", Decode("e,k"));
  EXPECT_EQ("f.js", Decode("f,l"));
  EXPECT_EQ("g.anything", Decode("g,oanything"));
}

}  // namespace net_instaweb
