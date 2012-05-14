/*
 * Copyright 2012 Google Inc.
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

// Author: matterbury@google.com (Matt Atterbury)

#include "net/instaweb/util/public/charset_util.h"

#include <cstring>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
namespace {

class CharsetUtilTest : public testing::Test {
 public:
  CharsetUtilTest() {}

  // buffer must point to a large enough char array, and target will be set to
  // point to it with the correct length.
  void TestCharsetForBom(const StringPiece bom,
                         const StringPiece contents,
                         const StringPiece charset) {
    GoogleString target = StrCat(bom, contents);
    EXPECT_EQ(charset, GetCharsetForBom(target));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CharsetUtilTest);
};

TEST_F(CharsetUtilTest, ProperBom) {
  EXPECT_EQ(3, std::strlen(kUtf8Bom));
  EXPECT_EQ(0xEF, static_cast<unsigned char>(kUtf8Bom[0]));
  EXPECT_EQ(0xBB, static_cast<unsigned char>(kUtf8Bom[1]));
  EXPECT_EQ(0xBF, static_cast<unsigned char>(kUtf8Bom[2]));
}

TEST_F(CharsetUtilTest, StripUtf8Bom) {
  StringPiece originalContents("<!DOCTYPE yadda yadda>");
  GoogleString contents;

  originalContents.CopyToString(&contents);
  StringPiece noBomContents(contents);
  EXPECT_FALSE(StripUtf8Bom(&noBomContents));
  EXPECT_STREQ(originalContents, noBomContents);

  contents = StrCat(kUtf8Bom, originalContents);
  StringPiece utf8Contents(contents);
  EXPECT_TRUE(StripUtf8Bom(&utf8Contents));
  EXPECT_STREQ(originalContents, utf8Contents);

  contents = StrCat(kUtf16BigEndianBom, originalContents);
  StringPiece utf16beContents(contents);
  EXPECT_FALSE(StripUtf8Bom(&utf16beContents));
  EXPECT_STREQ(StrCat(kUtf16BigEndianBom, originalContents),
               utf16beContents);
}

TEST_F(CharsetUtilTest, GetCharsetForBom) {
  StringPiece contents("<!DOCTYPE yadda yadda>");

  TestCharsetForBom(StringPiece(), contents, StringPiece());

  TestCharsetForBom(StringPiece(kUtf8Bom, STATIC_STRLEN(kUtf8Bom)),
                    contents, kUtf8Charset);

  TestCharsetForBom(StringPiece(kUtf16BigEndianBom,
                                STATIC_STRLEN(kUtf16BigEndianBom)),
                    contents, kUtf16BigEndianCharset);

  TestCharsetForBom(StringPiece(kUtf16LittleEndianBom,
                                STATIC_STRLEN(kUtf16LittleEndianBom)),
                    contents, kUtf16LittleEndianCharset);

  TestCharsetForBom(StringPiece(kUtf32BigEndianBom,
                                STATIC_STRLEN(kUtf32BigEndianBom)),
                    contents, kUtf32BigEndianCharset);

  TestCharsetForBom(StringPiece(kUtf32LittleEndianBom,
                                STATIC_STRLEN(kUtf32LittleEndianBom)),
                    contents, kUtf32LittleEndianCharset);
}

}  // namespace
}  // namespace net_instaweb
