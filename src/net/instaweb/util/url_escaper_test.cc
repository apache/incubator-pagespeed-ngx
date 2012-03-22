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

#include <cctype>
#include <cstddef>
#include "net/instaweb/util/public/url_escaper.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

// We pass through a few special characters unchanged, and we
// accept those characters, plus ',', as acceptable in the encoded
// URLs.
static const char kAcceptableSpecialChars[] = ",._+-=";
static const char kPassThruChars[]          =  "._+-=";

}  // namespace

namespace net_instaweb {

class UrlEscaperTest : public testing::Test {
 protected:
  void CheckEncoding(const StringPiece& url) {
    GoogleString encoded, decoded;
    UrlEscaper::EncodeToUrlSegment(url, &encoded);

    // Make sure there are only alphanumerics and _+-=%
    for (size_t i = 0; i < encoded.size(); ++i) {
      char c = encoded[i];
      EXPECT_TRUE(isalnum(c) || (strchr(kAcceptableSpecialChars, c) != NULL));
    }

    EXPECT_TRUE(UrlEscaper::DecodeFromUrlSegment(encoded, &decoded));
    EXPECT_EQ(url, decoded) << "\n encoded was " << encoded;
  }

  // Some basic text should be completely unchanged upon encode/decode.
  void CheckUnchanged(const StringPiece& url) {
    GoogleString encoded, decoded;
    UrlEscaper::EncodeToUrlSegment(url, &encoded);
    EXPECT_EQ(url, encoded);
    EXPECT_TRUE(UrlEscaper::DecodeFromUrlSegment(encoded, &decoded));
    EXPECT_EQ(url, decoded);
  }

  GoogleString Decode(const StringPiece& encoding) {
    GoogleString decoded;
    EXPECT_TRUE(UrlEscaper::DecodeFromUrlSegment(encoding, &decoded));
    return decoded;
  }

  GoogleString Encode(const StringPiece& url) {
    GoogleString encoded;
    UrlEscaper::EncodeToUrlSegment(url, &encoded);
    return encoded;
  }
};

TEST_F(UrlEscaperTest, TestUrls) {
  CheckEncoding("http://www.google.com");
  // Test encoding of % and lack of leading http:// (beware of double encoding):
  CheckEncoding("//web.mit.edu/foo.cgi?bar%baz");
  CheckEncoding("http://x.com/images/hacks.js.pagespeed.jm.GSLMcHP-fl.js");
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
  CheckUnchanged("=+-_");
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
  EXPECT_EQ("http://www.myhost.com", Decode(",h,wmyhost,c"));
}

TEST_F(UrlEscaperTest, PercentDecoding) {
  // Test the corner case where browser percent-encoded parts of our url.
  EXPECT_EQ("a.css", Decode("%61%2E%63%73%73"));  // Just %-encode whole url
  EXPECT_EQ("a.js+b.js", Decode("a.js%20b.js"));  // '+' re-encoded as %20 (' ')
  EXPECT_EQ("a%20b", Decode("a%2CP20b"));  // %-encoding of ,
  // TODO(jmaessen): The following never seems to happen in practice
  //  (encoding of character following , in comma encoding)
  // EXPECT_EQ("a/b", Decode("a,%2Fb"));  // %-encoding of character after ,
}

TEST_F(UrlEscaperTest, TestEncoding) {
  // Special case encoding a common sequence that would be long and
  // ugly to escape char-by-char.  We used to encode more than this
  // (e.g. .com -> ,c) but now that we can allow '.' in encoded names,
  // we favor legibility over compactness and have dropped the encoding
  // of ".com" and others.  However http:// requires three characters
  // to be decoded so we'll encode it in one piece.
  EXPECT_EQ(",h", Encode("http://"));

  // These common characters get special-case encodings.
  EXPECT_EQ(",u", Encode("^"));
  EXPECT_EQ(",P", Encode("%"));
  EXPECT_EQ(",_", Encode("/"));
  EXPECT_EQ(",-", Encode("\\"));
  EXPECT_EQ(",,", Encode(","));
  EXPECT_EQ(",q", Encode("?"));
  EXPECT_EQ(",a", Encode("&"));
  EXPECT_EQ(",M", Encode(".pagespeed."));
  EXPECT_EQ(",hx.com,_images,_hacks.js,Mjm.GSLMcHP-fl.js",
            Encode("http://x.com/images/hacks.js.pagespeed.jm.GSLMcHP-fl.js"));

  // Other characters are simply hexified.
  EXPECT_EQ(",3A", Encode(":"));
}

}  // namespace net_instaweb
