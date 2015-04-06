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

// Copyright 2007 Google Inc. All Rights Reserved.
// Author: yian@google.com (Yi-An Huang)

#include "webutil/css/util.h"

#include <string>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "webutil/css/parser.h"
#include "webutil/css/string.h"
#include "webutil/html/htmlcolor.h"

namespace {

class CssSystemColorTest : public testing::Test {
 protected:
  virtual void SetUp() {
    color_.reset(new HtmlColor(0, 0, 0));
  }

  virtual void TearDown() {
  }

  void TestColor(const char* name, const char* mapped_to) {
    CHECK(Css::Util::GetSystemColor(name, color_.get()));
    // TODO(sligocki): Chromium CHECK_STREQ appears to be buggy. Fixit.
    //CHECK_STREQ(color_->ToString().c_str(), mapped_to);
    CHECK_EQ(color_->ToString(), string(mapped_to));
  }

  void TestInvalidColor(const char* name) {
    CHECK(!Css::Util::GetSystemColor(name, color_.get()));
  }

 private:
  scoped_ptr<HtmlColor> color_;
};

TEST_F(CssSystemColorTest, common_colors) {
  // test some "intuitive" colors
  TestColor("menu", "#ffffff");         // menu background is white
  TestColor("menutext", "#000000");     // menu foreground is black
  TestColor("buttonface", "#ece9d8");   // button background is grey
  TestColor("captiontext", "#ffffff");  // caption text is white
  TestColor("windowtext", "#000000");   // window text is black
}

TEST_F(CssSystemColorTest, case_sensitiveness) {
  // colors are case insensitive
  TestColor("activeBorder", "#d4d0c8");
  TestColor("BACKGROUND", "#004e98");
}

TEST_F(CssSystemColorTest, invalid_colors) {
  TestInvalidColor("menux");
  TestInvalidColor("text");
  TestInvalidColor("win");
}

}  // namespace

namespace Css {

class MediaAppliesToScreenTest : public testing::Test {
 protected:
  bool ParseMediaAppliesToScreen(const StringPiece& media_string) {
    Css::Parser p(media_string);
    scoped_ptr<Css::MediaQueries> queries(p.ParseMediaQueries());
    return Css::Util::MediaAppliesToScreen(*queries);
  }

  void ExpectMediaAppliesToScreen(const StringPiece& media_string,
                                  bool expected) {
    // Test StringPiece implementation.
    EXPECT_EQ(expected, Css::Util::MediaAppliesToScreen(media_string))
        << "Media string: " << media_string;

    // Test MediaQueries implementation.
    EXPECT_EQ(expected, ParseMediaAppliesToScreen(media_string))
        << "Media string: " << media_string;
  }
};

TEST_F(MediaAppliesToScreenTest, ComplexMediaQueries) {
  ExpectMediaAppliesToScreen("screen", true);
  ExpectMediaAppliesToScreen("all", true);
  ExpectMediaAppliesToScreen("", true);
  ExpectMediaAppliesToScreen("print, braille, screen", true);
  ExpectMediaAppliesToScreen("not screen, screen", true);

  // Note: When parsed, we accept it even though it does not apply to all
  // devices of type screen because we are acting like a CSS2 parser and
  // thus reading "screen" but ignoring the rest.
  EXPECT_TRUE(ParseMediaAppliesToScreen("screen and (color)"));
  // Css::Util::MediaAppliesToScreen(const StringPiece&) does not parse the
  // string, and so it's simple string comparison fails here.
  EXPECT_FALSE(Css::Util::MediaAppliesToScreen("screen and (color)"));

  ExpectMediaAppliesToScreen("screening", false);
  ExpectMediaAppliesToScreen("print", false);
  ExpectMediaAppliesToScreen("not screen", false);
  // Note: We reject these CSS3 queries even though they do apply to all
  // "screen" devices. This is intentional because we are acting like a CSS2
  // parser that does not understand the "not" and "only" qualifiers and
  // it is explicitly stated in the CSS spec that these will be ignored by
  // older CSS parsers.
  ExpectMediaAppliesToScreen("only screen", false);
  ExpectMediaAppliesToScreen("not print", false);
}

}  // namespace Css
