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
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"

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

} // css_util

} // net_instaweb
