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

// Author: jkarlin@google.com (Josh Karlin)

#include "net/instaweb/rewriter/public/pedantic_filter.h"

#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class PedanticFilterTest : public HtmlParseTestBase {
 protected:
  PedanticFilterTest()
      : pedantic_filter_(&html_parse_) {
    html_parse_.AddFilter(&pedantic_filter_);
  }

  virtual bool AddBody() const { return false; }

 private:
  PedanticFilter pedantic_filter_;

  DISALLOW_COPY_AND_ASSIGN(PedanticFilterTest);
};

TEST_F(PedanticFilterTest, ChangeStyleWithNoType) {
  ValidateExpected("change_style_with_no_type",
                   "<head><style>h1 {color : #ff0000;}</style></head>",
                   "<head><style type=\"text/css\">h1 {color : #ff0000;}"
                   "</style></head>");
}

TEST_F(PedanticFilterTest, DoNotBreakStyleType) {
  ValidateNoChanges("do_not_break_style_type",
                    "<head><style type=\"text/css2\">h1 {color : #ff0000;}"
                    "</style><head>");
}

TEST_F(PedanticFilterTest, DoNotAlterHTML5Style) {
  SetDoctype("<!doctype html>");
  ValidateNoChanges("do_not_alter_html_5_style",
                    "<head><style>h1 {color : #ff0000;}</style></head>");
}

TEST_F(PedanticFilterTest, ChangeScriptWithNoType) {
  ValidateExpected("change_script_with_no_type",
                   "<head><script>var x=1;</script></head>",
                   "<head><script type=\"text/javascript\">var x=1;"
                   "</script></head>");
}

TEST_F(PedanticFilterTest, DoNotBreakScriptType) {
  ValidateNoChanges("do_not_break_script_type",
                    "<head><script type=\"text/ecmascript\">var x=1;</script>"
                    "</head>");
}

TEST_F(PedanticFilterTest, DoNotAlterHTML5Script) {
  SetDoctype("<!doctype html>");
  ValidateNoChanges("do_not_alter_html_5_style",
                    "<head><script>var x=1;</script></head>");
}

}  // namespace net_instaweb
