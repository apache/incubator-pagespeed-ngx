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
// Author: sligocki@google.com (Shawn Ligocki),
//         jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/util/public/url_to_filename_encoder.h"

#include <vector>

#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

#ifdef WIN32
char kDirSeparator = '\\';
char kOtherDirSeparator = '/';
#else
char kDirSeparator = '/';
char kOtherDirSeparator = '\\';
#endif

class UrlToFilenameEncoderTest : public ::testing::Test {
 protected:
  UrlToFilenameEncoderTest() : escape_(1, UrlToFilenameEncoder::kEscapeChar),
                               dir_sep_(1, kDirSeparator) {
  }

  void CheckSegmentLength(const StringPiece& escaped_word) {
    std::vector<StringPiece> components;
    SplitStringPieceToVector(escaped_word, "/", &components, false);
    for (size_t i = 0; i < components.size(); ++i) {
      EXPECT_GE(UrlToFilenameEncoder::kMaximumSubdirectoryLength,
                components[i].size());
    }
  }

  void CheckValidChars(const StringPiece& escaped_word, char invalid_slash) {
    // These characters are invalid in Windows.  We add in ', as that's pretty
    // inconvenient in a Unix filename.
    //
    // See http://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx
    const GoogleString kInvalidChars = "<>:\"|?*'";
    for (size_t i = 0; i < escaped_word.size(); ++i) {
      char c = escaped_word[i];
      EXPECT_EQ(GoogleString::npos, kInvalidChars.find(c));
      EXPECT_NE(invalid_slash, c);
      EXPECT_NE('\0', c);  // only invalid character in Posix
      EXPECT_GT(0x7E, c);  // only English printable characters
    }
  }

  void Validate(const GoogleString& in_word, const GoogleString& gold_word) {
    GoogleString escaped_word, url;
    UrlToFilenameEncoder::EncodeSegment("", in_word, '/', &escaped_word);
    EXPECT_EQ(gold_word, escaped_word);
    CheckSegmentLength(escaped_word);
    CheckValidChars(escaped_word, '\\');
    UrlToFilenameEncoder::Decode(escaped_word, '/', &url);
    EXPECT_EQ(in_word, url);
  }

  void ValidateAllSegmentsSmall(const GoogleString& in_word) {
    GoogleString escaped_word, url;
    UrlToFilenameEncoder::EncodeSegment("", in_word, '/', &escaped_word);
    CheckSegmentLength(escaped_word);
    CheckValidChars(escaped_word, '\\');
    UrlToFilenameEncoder::Decode(escaped_word, '/', &url);
    EXPECT_EQ(in_word, url);
  }

  void ValidateNoChange(const GoogleString& word) {
    // We always suffix the leaf with kEscapeChar, unless the leaf is empty.
    Validate(word, word + escape_);
  }

  void ValidateEscaped(unsigned char ch) {
    // We always suffix the leaf with kEscapeChar, unless the leaf is empty.
    char escaped[100];
    const char escape = UrlToFilenameEncoder::kEscapeChar;
    snprintf(escaped, sizeof(escaped), "%c%02X%c", escape, ch, escape);
    Validate(GoogleString(1, ch), escaped);
  }

  void ValidateUrl(const GoogleString& url, const GoogleString& base_path,
                   bool legacy_escape, const GoogleString& gold_filename) {
    GoogleString encoded_filename = UrlToFilenameEncoder::Encode(
        url, base_path, legacy_escape);
    EXPECT_EQ(gold_filename, encoded_filename);
    if (!legacy_escape) {
      CheckSegmentLength(encoded_filename);
      CheckValidChars(encoded_filename, kOtherDirSeparator);
      GoogleString decoded_url;
      UrlToFilenameEncoder::Decode(encoded_filename, kDirSeparator,
                                   &decoded_url);
      if (url != decoded_url) {
        EXPECT_EQ(url, "http://" + decoded_url);
      }
    }
  }

  void ValidateUrlOldNew(const GoogleString& url,
                         const GoogleString& gold_old_filename,
                         const GoogleString& gold_new_filename) {
    ValidateUrl(url, "", true, gold_old_filename);
    ValidateUrl(url, "", false, gold_new_filename);
  }

  void ValidateEncodeSame(const GoogleString& url1, const GoogleString& url2) {
    GoogleString filename1 = UrlToFilenameEncoder::Encode(url1, "", false);
    GoogleString filename2 = UrlToFilenameEncoder::Encode(url2, "", false);
    EXPECT_EQ(filename1, filename2);
  }

  GoogleString escape_;
  GoogleString dir_sep_;
};

TEST_F(UrlToFilenameEncoderTest, DoesNotEscape) {
  ValidateNoChange("");
  ValidateNoChange("abcdefg");
  ValidateNoChange("abcdefghijklmnopqrstuvwxyz");
  ValidateNoChange("ZYXWVUT");
  ValidateNoChange("ZYXWVUTSRQPONMLKJIHGFEDCBA");
  ValidateNoChange("01234567689");
  ValidateNoChange("_.=+-");
  ValidateNoChange("abcdefghijklmnopqrstuvwxyzZYXWVUTSRQPONMLKJIHGFEDCBA"
                   "01234567689_.=+-");
  ValidateNoChange("index.html");
  ValidateNoChange("/");
  ValidateNoChange("/.");
  ValidateNoChange(".");
  ValidateNoChange("..");
}

TEST_F(UrlToFilenameEncoderTest, Escapes) {
  const GoogleString bad_chars =
      "<>:\"\\|?*"      // Illegal on Windows
      "~`!$^&(){}[]';"  // Bad for Unix shells
      "^@"              // Blaze doesn't like
      "#%"              // Perforce doesn't like
      ",";              // The escape char has to be escaped

  for (size_t i = 0; i < bad_chars.size(); ++i) {
    ValidateEscaped(bad_chars[i]);
  }

  // Check non-printable characters.
  ValidateEscaped('\0');
  for (size_t i = 127; i < 256; ++i) {
    ValidateEscaped(static_cast<char>(i));
  }
}

TEST_F(UrlToFilenameEncoderTest, DoesEscapeCorrectly) {
  Validate("mysite.com&x", "mysite.com" + escape_ + "26x" + escape_);
  Validate("/./", "/" + escape_ + "./" + escape_);
  Validate("/../", "/" + escape_ + "../" + escape_);
  Validate("//", "/" + escape_ + "2F" + escape_);
  Validate("/./leaf", "/" + escape_ + "./leaf" + escape_);
  Validate("/../leaf", "/" + escape_ + "../leaf" + escape_);
  Validate("//leaf", "/" + escape_ + "2Fleaf" + escape_);
  Validate("mysite/u?param1=x&param2=y",
           "mysite/u" + escape_ + "3Fparam1=x" + escape_ + "26param2=y" +
           escape_);
  Validate("search?q=dogs&go=&form=QBLH&qs=n",
           "search" + escape_ + "3Fq=dogs" + escape_ + "26go=" + escape_ +
           "26form=QBLH" + escape_ + "26qs=n" + escape_);
  Validate("~joebob/my_neeto-website+with_stuff.asp?id=138&content=true",
           "" + escape_ + "7Ejoebob/my_neeto-website+with_stuff.asp" + escape_ +
           "3Fid=138" + escape_ + "26content=true" + escape_);
}

TEST_F(UrlToFilenameEncoderTest, EncodeUrlCorrectly) {
  ValidateUrlOldNew("http://www.google.com/index.html",
                    "www.google.com" + dir_sep_ + "indexx2Ehtml",
                    "www.google.com" + dir_sep_ + "index.html" + escape_);
  ValidateUrlOldNew("http://www.google.com/x/search?hl=en&q=dogs&oq=",
                    "www.google.com" + dir_sep_ + "x" + dir_sep_ +
                    "searchx3Fhlx3Denx26qx3Ddogsx26oqx3D",

                    "www.google.com" + dir_sep_ + "x" + dir_sep_ + "search" +
                    escape_ + "3Fhl=en" + escape_ + "26q=dogs" + escape_ +
                    "26oq=" + escape_);
  ValidateUrlOldNew("http://www.foo.com/a//",
                    "www.foo.com" + dir_sep_ + "ax255Cx255Cindexx2Ehtml",
                    "www.foo.com" + dir_sep_ + "a" + dir_sep_ + escape_ + "2F" +
                    escape_);

  ValidateUrl("http://www.foo.com/u?site=http://www.google.com/index.html",
              "", false,
              "www.foo.com" + dir_sep_ + "u" + escape_ + "3Fsite=http" +
              escape_ + "3A" + dir_sep_ + escape_ + "2Fwww.google.com" +
              dir_sep_ + "index.html" + escape_);
  ValidateUrlOldNew(
      "http://blogutils.net/olct/online.php?"
      "site=http://thelwordfanfics.blogspot.&interval=600",

      "blogutils.net" + dir_sep_ + "olct" + dir_sep_ + "onlinex2Ephpx3F"
      "sitex3Dhttpx3Ax255Cx255Cthelwordfanficsx2Eblogspotx2Ex26intervalx3D600",

      "blogutils.net" + dir_sep_ + "olct" + dir_sep_ + "online.php" + escape_ +
      "3Fsite=http" + escape_ + "3A" + dir_sep_ + escape_ +
      "2Fthelwordfanfics.blogspot." + escape_ + "26interval=600" + escape_);
}

TEST_F(UrlToFilenameEncoderTest, UnescapeUrlsBeforeEncode) {
  for (int i = 0; i < 128; ++i) {
    GoogleString unescaped(1, static_cast<char>(i));
    GoogleString escaped = StringPrintf("%%%02X", i);
    ValidateEncodeSame(unescaped, escaped);
  }

  ValidateEncodeSame(
      "http://www.blogger.com/navbar.g?bName=God!&Mode=FOO&searchRoot"
      "=http%3A%2F%2Fsurvivorscanthrive.blogspot.com%2Fsearch",

      "http://www.blogger.com/navbar.g?bName=God%21&Mode=FOO&searchRoot"
      "=http%3A%2F%2Fsurvivorscanthrive.blogspot.com%2Fsearch");
}

TEST_F(UrlToFilenameEncoderTest, EscapeSecondSlash) {
  Validate("/", "/" + escape_);
  Validate("//", "/" + escape_ + "2F" + escape_);
  Validate("///", "/" + escape_ + "2F" + "/" + escape_);
}

TEST_F(UrlToFilenameEncoderTest, LongTail) {
  static char long_word[] =
      "~joebob/briggs/12345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890";

  // the long lines in the string below are 64 characters, so we can see
  // the slashes every 128.
  GoogleString gold_long_word =
      escape_ + "7Ejoebob/briggs/"
      "1234567890123456789012345678901234567890123456789012345678901234"
      "56789012345678901234567890123456789012345678901234567890123456" +
      escape_ + "-/"
      "7890123456789012345678901234567890123456789012345678901234567890"
      "12345678901234567890123456789012345678901234567890123456789012" +
      escape_ + "-/"
      "3456789012345678901234567890123456789012345678901234567890123456"
      "78901234567890123456789012345678901234567890123456789012345678" +
      escape_ + "-/"
      "9012345678901234567890" + escape_;
  EXPECT_LT(UrlToFilenameEncoder::kMaximumSubdirectoryLength,
            sizeof(long_word));
  Validate(long_word, gold_long_word);
}

TEST_F(UrlToFilenameEncoderTest, LongTailQuestion) {
  // Here the '?' in the last path segment expands to @3F, making
  // it hit 128 chars before the input segment gets that big.
  static char long_word[] =
      "~joebob/briggs/1234567?1234567?1234567?1234567?1234567?"
      "1234567?1234567?1234567?1234567?1234567?1234567?1234567?"
      "1234567?1234567?1234567?1234567?1234567?1234567?1234567?"
      "1234567?1234567?1234567?1234567?1234567?1234567?1234567?"
      "1234567?1234567?1234567?1234567?1234567?1234567?1234567?"
      "1234567?1234567?1234567?1234567?1234567?1234567?1234567?";

  // Notice that at the end of the third segment, we avoid splitting
  // the (escape_ + "3F") that was generated from the "?", so that segment is
  // only 127 characters.
  GoogleString pattern = "1234567" + escape_ + "3F";  // 10 characters
  GoogleString gold_long_word =
      escape_ + "7Ejoebob/briggs/" +
      pattern + pattern + pattern + pattern + pattern + pattern + "1234"
      "567" + escape_ + "3F" + pattern + pattern + pattern + pattern + pattern +
       "123456" + escape_ + "-/"
      "7" + escape_ + "3F" + pattern + pattern + pattern + pattern + pattern +
      pattern + pattern + pattern + pattern + pattern + pattern + pattern +
      "12" +
      escape_ + "-/"
      "34567" + escape_ + "3F" + pattern + pattern + pattern + pattern + pattern
      + "1234567" + escape_ + "3F" + pattern + pattern + pattern + pattern
      + pattern + "1234567" +
      escape_ + "-/" +
      escape_ + "3F" + pattern + pattern + escape_;
  EXPECT_LT(UrlToFilenameEncoder::kMaximumSubdirectoryLength,
            sizeof(long_word));
  Validate(long_word, gold_long_word);
}

TEST_F(UrlToFilenameEncoderTest, CornerCasesNearMaxLenNoEscape) {
  // hit corner cases, +/- 4 characters from kMaxLen
  for (int i = -4; i <= 4; ++i) {
    GoogleString input;
    input.append(i + UrlToFilenameEncoder::kMaximumSubdirectoryLength, 'x');
    ValidateAllSegmentsSmall(input);
  }
}

TEST_F(UrlToFilenameEncoderTest, CornerCasesNearMaxLenWithEscape) {
  // hit corner cases, +/- 4 characters from kMaxLen.  This time we
  // leave off the last 'x' and put in a '.', which ensures that we
  // are truncating with '/' *after* the expansion.
  for (int i = -4; i <= 4; ++i) {
    GoogleString input;
    input.append(i + UrlToFilenameEncoder::kMaximumSubdirectoryLength - 1, 'x');
    input.push_back('.');  // this will expand to 3 characters.
    ValidateAllSegmentsSmall(input);
  }
}

TEST_F(UrlToFilenameEncoderTest, LeafBranchAlias) {
  Validate("/a/b/c", "/a/b/c" + escape_);        // c is leaf file "c,"
  Validate("/a/b/c/d", "/a/b/c/d" + escape_);    // c is directory "c"
  Validate("/a/b/c/d/", "/a/b/c/d/" + escape_);
}


TEST_F(UrlToFilenameEncoderTest, BackslashSeparator) {
  GoogleString long_word;
  GoogleString escaped_word;
  long_word.append(UrlToFilenameEncoder::kMaximumSubdirectoryLength + 1, 'x');
  UrlToFilenameEncoder::EncodeSegment("", long_word, '\\', &escaped_word);

  // check that one backslash, plus the escape ",-", and the ending , got added.
  EXPECT_EQ(long_word.size() + 4, escaped_word.size());
  ASSERT_LT(UrlToFilenameEncoder::kMaximumSubdirectoryLength,
            escaped_word.size());
  // Check that the backslash got inserted at the correct spot.
  EXPECT_EQ('\\', escaped_word[
      UrlToFilenameEncoder::kMaximumSubdirectoryLength]);
}

}  // namespace

}  // namespace net_instaweb
