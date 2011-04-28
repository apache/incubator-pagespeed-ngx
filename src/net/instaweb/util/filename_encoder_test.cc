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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/util/public/filename_encoder.h"

#include <vector>
#include <cstddef>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {
// net::kMaximumSubdirectoryLength is defined in url_to_filename_encoder.cc, but
// we cannot link it.
const size_t kMaxLen = 128;
}  // namespace

namespace net_instaweb {

// Note -- the exact behavior of the encoder is tested in
// gfe/tools/loadtesting/spdy_testing/url_to_filename_encoder_test.cc
//
// Here we just test that the names meet certain properties:
//   1. The segments are small
//   2. The URL can be recovered from the filename
//   3. No invalid filename characters are present.
class FilenameEncoderTest : public ::testing::Test {
 public:
  FilenameEncoderTest() { }

 protected:
  void CheckSegmentLength(const StringPiece& escaped_word) {
    std::vector<StringPiece> components;
    SplitStringPieceToVector(escaped_word, "/", &components, false);
    for (size_t i = 0; i < components.size(); ++i) {
      EXPECT_GE(kMaxLen, components[i].size());
    }
  }

  void CheckValidChars(const StringPiece& escaped_word) {
    // These characters are invalid in Windows.  We will
    // ignore / for this test, but add in '.
    // See http://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx
    static const char kInvalidChars[] = "<>:\"\\|?*'";
    for (size_t i = 0; i < escaped_word.size(); ++i) {
      char c = escaped_word[i];
      EXPECT_TRUE(NULL == strchr(kInvalidChars, c));
    }
  }

  void Validate(const GoogleString& in_word) {
    GoogleString escaped_word, decoded_url;
    encoder.Encode("", in_word, &escaped_word);
    CheckSegmentLength(escaped_word);
    CheckValidChars(escaped_word);
    EXPECT_TRUE(encoder.Decode(escaped_word, &decoded_url));
    EXPECT_EQ(in_word, decoded_url);
  }

  FilenameEncoder encoder;

 private:
  DISALLOW_COPY_AND_ASSIGN(FilenameEncoderTest);
};

TEST_F(FilenameEncoderTest, DoesNotEscapeAlphanum) {
  Validate("");
  Validate("abcdefg");
  Validate("abcdefghijklmnopqrstuvwxyz");
  Validate("ZYXWVUT");
  Validate("ZYXWVUTSRQPONMLKJIHGFEDCBA");
  Validate("01234567689");
  Validate("/-_");
  Validate("abcdefghijklmnopqrstuvwxyzZYXWVUTSRQPONMLKJIHGFEDCBA"
           "01234567689/-_");
}

TEST_F(FilenameEncoderTest, DoesEscapeNonAlphanum) {
  Validate(".");
  Validate("`~!@#$%^&*()_=+[{]}\\|;:'\",<.>?");
}

TEST_F(FilenameEncoderTest, DoesEscapeCorrectly) {
  Validate("index.html");
  Validate("search?q=dogs&go=&form=QBLH&qs=n");
  Validate("~joebob/my_neeto-website+with_stuff.asp?id=138&content=true");
}

TEST_F(FilenameEncoderTest, LongTail) {
  static char long_word[] =
      "~joebob/briggs/12345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890";
  Validate(long_word);
}

TEST_F(FilenameEncoderTest, LongTailDots) {
  // Here the '.' in the last path segment expands to x2E, making
  // it hits 128 chars before the input segment gets that big.
  static char long_word[] =
      "~joebob/briggs/1234567.1234567.1234567.1234567.1234567."
      "1234567.1234567.1234567.1234567.1234567.1234567.1234567."
      "1234567.1234567.1234567.1234567.1234567.1234567.1234567."
      "1234567.1234567.1234567.1234567.1234567.1234567.1234567."
      "1234567.1234567.1234567.1234567.1234567.1234567.1234567."
      "1234567.1234567.1234567.1234567.1234567.1234567.1234567.";
  Validate(long_word);
}

TEST_F(FilenameEncoderTest, CornerCasesNearMaxLenNoEscape) {
  // hit corner cases, +/- 4 characters from kMaxLen
  for (int i = -4; i <= 4; ++i) {
    GoogleString input;
    input.append(i + kMaxLen, 'x');
    Validate(input);
  }
}

TEST_F(FilenameEncoderTest, CornerCasesNearMaxLenWithEscape) {
  // hit corner cases, +/- 4 characters from kMaxLen.  This time we
  // leave off the last 'x' and put in a '.', which ensures that we
  // are truncating with '/' *after* the expansion.
  for (int i = -4; i <= 4; ++i) {
    GoogleString input;
    input.append(i + kMaxLen - 1, 'x');
    input.append(1, '.');  // this will expand to 3 characters.
    Validate(input);
  }
}

}  // namespace
