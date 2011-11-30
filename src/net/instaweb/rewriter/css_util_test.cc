/*
 * Copyright 2011 Google Inc.
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

// Author: nforman@google.com (Naomi Forman)

// Unit-test the css utilities.

#include "net/instaweb/rewriter/public/css_util.h"

#include <algorithm>
#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "util/utf8/public/unicodetext.h"

namespace net_instaweb {

namespace css_util {

class CssUtilTest : public testing::Test {
 protected:
  CssUtilTest() { }

  GoogleMessageHandler message_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CssUtilTest);
};

TEST_F(CssUtilTest, TestGetDimensions) {
  HtmlParse html_parse(&message_handler_);
  HtmlElement* img = html_parse.NewElement(NULL, HtmlName::kImg);
  html_parse.AddAttribute(img, HtmlName::kStyle,
                          "height:50px;width:80px;border-width:0px;");

  scoped_ptr<StyleExtractor> extractor(new StyleExtractor(img));
  EXPECT_EQ(kHasBothDimensions, extractor->state());
  EXPECT_EQ(80, extractor->width());
  EXPECT_EQ(50, extractor->height());

  html_parse.DeleteElement(img);
  img = html_parse.NewElement(NULL, HtmlName::kImg);
  html_parse.AddAttribute(img, HtmlName::kStyle,
                          "border-width:0px;");
  extractor.reset(new StyleExtractor(img));
  EXPECT_EQ(kNoDimensions, extractor->state());
  EXPECT_EQ(kNoValue, extractor->width());
  EXPECT_EQ(kNoValue, extractor->height());

  html_parse.DeleteElement(img);
  img = html_parse.NewElement(NULL, HtmlName::kImg);
  html_parse.AddAttribute(img, HtmlName::kStyle,
                          "border-width:0px;width:80px;");

  extractor.reset(new StyleExtractor(img));
  EXPECT_EQ(kHasWidthOnly, extractor->state());
  EXPECT_EQ(kNoValue, extractor->height());
  EXPECT_EQ(80, extractor->width());

  html_parse.DeleteElement(img);
  img = html_parse.NewElement(NULL, HtmlName::kImg);
  html_parse.AddAttribute(img, HtmlName::kStyle,
                          "border-width:0px;height:200px");
  extractor.reset(new StyleExtractor(img));
  EXPECT_EQ(kHasHeightOnly, extractor->state());
  EXPECT_EQ(200, extractor->height());
  EXPECT_EQ(kNoValue, extractor->width());
  html_parse.DeleteElement(img);
}

TEST_F(CssUtilTest, TestAnyDimensions) {
  HtmlParse html_parse(&message_handler_);
  HtmlElement* img = html_parse.NewElement(NULL, HtmlName::kImg);
  html_parse.AddAttribute(img, HtmlName::kStyle,
                          "width:80px;border-width:0px;");
  scoped_ptr<StyleExtractor> extractor(new StyleExtractor(img));
  EXPECT_TRUE(extractor->HasAnyDimensions());
  EXPECT_EQ(kHasWidthOnly, extractor->state());

  html_parse.DeleteElement(img);
  img = html_parse.NewElement(NULL, HtmlName::kImg);
  html_parse.AddAttribute(img, HtmlName::kStyle,
                          "border-width:0px;background-color:blue;");
  extractor.reset(new StyleExtractor(img));
  EXPECT_FALSE(extractor->HasAnyDimensions());

  html_parse.DeleteElement(img);
  img = html_parse.NewElement(NULL, HtmlName::kImg);
  html_parse.AddAttribute(img, HtmlName::kStyle,
                          "border-width:0px;width:30px;height:40px");
  extractor.reset(new StyleExtractor(img));
  EXPECT_TRUE(extractor->HasAnyDimensions());

}

TEST_F(CssUtilTest, VectorizeMediaAttribute) {
  const char kSimpleMedia[] = "screen";
  const char* kSimpleVector[] = { "screen" };
  StringVector simple_expected(kSimpleVector,
                               kSimpleVector + arraysize(kSimpleVector));
  StringVector simple_actual;
  VectorizeMediaAttribute(kSimpleMedia, &simple_actual);
  EXPECT_TRUE(simple_expected == simple_actual);

  const char kUglyMessMedia[] = "screen,, ,printer , screen ";
  const char* kUglyMessVector[] = { "screen", "printer", "screen" };
  StringVector ugly_expected(kUglyMessVector,
                             kUglyMessVector + arraysize(kUglyMessVector));
  StringVector ugly_actual;
  VectorizeMediaAttribute(kUglyMessMedia, &ugly_actual);
  EXPECT_TRUE(ugly_expected == ugly_actual);

  const char kAllSubsumesMedia[] = "screen,, ,printer , all ";
  StringVector subsumes_actual;
  VectorizeMediaAttribute(kAllSubsumesMedia, &subsumes_actual);
  EXPECT_TRUE(subsumes_actual.empty());
}

TEST_F(CssUtilTest, StringifyMediaVector) {
  const char kSimpleMedia[] = "screen";
  const char* kSimpleVector[] = { "screen" };
  StringVector simple_vector(kSimpleVector,
                             kSimpleVector + arraysize(kSimpleVector));
  GoogleString simple_media = StringifyMediaVector(simple_vector);
  EXPECT_EQ(kSimpleMedia, simple_media);

  const char kMultipleMedia[] = "screen,printer,screen";
  const char* kMultipleVector[] = { "screen", "printer", "screen" };
  StringVector multiple_vector(kMultipleVector,
                               kMultipleVector + arraysize(kMultipleVector));
  GoogleString multiple_media = StringifyMediaVector(multiple_vector);
  EXPECT_EQ(kMultipleMedia, multiple_media);

  StringVector all_vector;
  GoogleString all_media = StringifyMediaVector(all_vector);
  EXPECT_EQ(css_util::kAllMedia, all_media);
}

TEST_F(CssUtilTest, ConvertUnicodeVectorToStringVector) {
  const char kScreen[] = "screen";
  const char kPrinter[] = "printer ";
  const char kAll[] = " all";
  const char kEmpty[] = "";
  const char kBlanks[] = "  ";

  std::vector<UnicodeText> unicode_vector;
  UnicodeText element;
  unicode_vector.push_back(element.CopyUTF8(kScreen,  STATIC_STRLEN(kScreen)));
  unicode_vector.push_back(element.CopyUTF8(kEmpty,   STATIC_STRLEN(kEmpty)));
  unicode_vector.push_back(element.CopyUTF8(kBlanks,  STATIC_STRLEN(kBlanks)));
  unicode_vector.push_back(element.CopyUTF8(kPrinter, STATIC_STRLEN(kPrinter)));
  unicode_vector.push_back(element.CopyUTF8(kAll,     STATIC_STRLEN(kAll)));

  const char* kExpectedVector[] = { "screen", "printer", "all" };
  StringVector expected_vector(kExpectedVector,
                               kExpectedVector + arraysize(kExpectedVector));
  StringVector actual_vector;
  ConvertUnicodeVectorToStringVector(unicode_vector, &actual_vector);
  EXPECT_TRUE(expected_vector == actual_vector);
}

TEST_F(CssUtilTest, ConvertStringVectorToUnicodeVector) {
  const char kScreen[] = "screen";
  const char kPrint[] = "print";
  const char kAll[] = "all";

  std::vector<UnicodeText> expected_vector;
  UnicodeText element;
  expected_vector.push_back(element.CopyUTF8(kScreen, STATIC_STRLEN(kScreen)));
  expected_vector.push_back(element.CopyUTF8(kPrint,  STATIC_STRLEN(kPrint)));
  expected_vector.push_back(element.CopyUTF8(kAll,    STATIC_STRLEN(kAll)));

  const char* kInputVector[] = { "screen", "", " ", "print ", " all " };
  StringVector input_vector(kInputVector,
                            kInputVector + arraysize(kInputVector));
  std::vector<UnicodeText> actual_vector;
  ConvertStringVectorToUnicodeVector(input_vector, &actual_vector);
  EXPECT_TRUE(expected_vector == actual_vector);
}

TEST_F(CssUtilTest, ClearVectorIfContainsMediaAll) {
  const char* kInputVector[] = { "screen", "", " ", "print " };
  StringVector input_vector(kInputVector,
                            kInputVector + arraysize(kInputVector));

  // 1. No 'all' in there.
  StringVector output_vector = input_vector;
  ClearVectorIfContainsMediaAll(&output_vector);
  EXPECT_TRUE(input_vector == output_vector);

  // 2. 'all' in there.
  output_vector = input_vector;
  output_vector.push_back(kAllMedia);
  ClearVectorIfContainsMediaAll(&output_vector);
  EXPECT_TRUE(output_vector.empty());
}

TEST_F(CssUtilTest, EliminateElementsNotIn) {
  const char* kSmallVector[] = { "screen", "print", "alternate" };
  StringVector small_vector(kSmallVector,
                            kSmallVector + arraysize(kSmallVector));
  std::sort(small_vector.begin(), small_vector.end());
  const char* kLargeVector[] = { "aural", "visual", "screen",
                                 "tactile", "print", "olfactory" };
  StringVector large_vector(kLargeVector,
                            kLargeVector + arraysize(kLargeVector));
  std::sort(large_vector.begin(), large_vector.end());
  const char* kIntersectVector[] = { "screen", "print" };
  StringVector intersect_vector(kIntersectVector,
                                kIntersectVector + arraysize(kIntersectVector));
  std::sort(intersect_vector.begin(), intersect_vector.end());
  StringVector empty_vector;
  StringVector input_vector;

  // 1. empty + empty => empty
  EliminateElementsNotIn(&input_vector, empty_vector);
  EXPECT_TRUE(input_vector.empty());

  // 2. empty + non-empty => non-empty
  EliminateElementsNotIn(&input_vector, small_vector);
  EXPECT_TRUE(input_vector == small_vector);

  // 3. non-empty + empty => non-empty
  EliminateElementsNotIn(&input_vector, empty_vector);
  EXPECT_TRUE(input_vector == small_vector);

  // 4. non-empty + non-empty => items only in both
  input_vector = small_vector;
  EliminateElementsNotIn(&input_vector, large_vector);
  EXPECT_TRUE(input_vector == intersect_vector);
  input_vector = large_vector;
  EliminateElementsNotIn(&input_vector, small_vector);
  EXPECT_TRUE(input_vector == intersect_vector);
}

} // css_util

} // net_instaweb
