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

// Author: mdsteele@google.com (Matthew D. Steele)

#include "net/instaweb/rewriter/public/collapse_whitespace_filter.h"

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CollapseWhitespaceFilterTest : public RewriteTestBase {
 protected:
  CollapseWhitespaceFilterTest() {}
  ~CollapseWhitespaceFilterTest() {}
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    AddFilter(RewriteOptions::kCollapseWhitespace);
    AddOtherFilter(RewriteOptions::kCollapseWhitespace);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CollapseWhitespaceFilterTest);
};

TEST_F(CollapseWhitespaceFilterTest, NoChange) {
  ValidateNoChanges("no_change",
                    "<head><title>Hello</title></head>"
                    "<body>Why, hello there!</body>");
}

TEST_F(CollapseWhitespaceFilterTest, CollapseWhitespace) {
  ValidateExpected("collapse_whitespace",
                   "<body>hello   world,   it\n"
                   "    is good  to     see you   </body>",
                   "<body>hello world, it\n"
                   "is good to see you </body>");
}

TEST_F(CollapseWhitespaceFilterTest, NewlineTakesPrecedence) {
  ValidateExpected("newline_takes_precedence",
                   "<body>hello world, it      \n"
                   "    is good to see you</body>",
                   "<body>hello world, it\n"
                   "is good to see you</body>");
}

TEST_F(CollapseWhitespaceFilterTest, DoNotCollapseWithinPre) {
  ValidateNoChanges("do_not_collapse_within_pre",
                    "<body><pre>hello   world,   it\n"
                    "    is good  to     see you   </pre></body>");
}

TEST_F(CollapseWhitespaceFilterTest, CollapseAfterNestedPre) {
  ValidateExpected("collapse_after_nested_pre",
                   "<body><pre>hello   <pre>world,   it</pre>\n"
                   "    is good</pre>  to     see you   </body>",
                   "<body><pre>hello   <pre>world,   it</pre>\n"
                   "    is good</pre> to see you </body>");
}

TEST_F(CollapseWhitespaceFilterTest, DoNotCollapseWithinScript) {
  ValidateExpected("do_not_collapse_within_script",
                   "<head><script>x = \"don't    collapse\"</script></head>"
                   "<body>do       collapse</body>",
                   "<head><script>x = \"don't    collapse\"</script></head>"
                   "<body>do collapse</body>");
}

TEST_F(CollapseWhitespaceFilterTest, DoNotCollapseWithinStyle) {
  ValidateNoChanges("do_not_collapse_within_style",
                    "<head><style>P{font-family:\"don't   collapse\";}</style>"
                    "</head><body></body>");
}

TEST_F(CollapseWhitespaceFilterTest, DoNotCollapseWithinTextarea) {
  ValidateNoChanges("do_not_collapse_within_textarea",
                    "<body><textarea>hello   world,   it\n"
                    "    is good  to     see you   </textarea></body>");
}

class CollapseWhitespaceGeneralTest : public RewriteTestBase {
  // Don't add any text to our tests.
  virtual bool AddHtmlTags() const { return false; }
};

// Issue 463: Collapse whitespace after other filters have been applied
// for maximum effectiveness.
TEST_F(CollapseWhitespaceGeneralTest, CollapseAfterCombine) {
  // Note: Even though we enable collapse_whitespace first, it should run
  // after combine_css.
  options()->EnableFilter(RewriteOptions::kCollapseWhitespace);
  options()->EnableFilter(RewriteOptions::kCombineCss);
  rewrite_driver()->AddFilters();

  // Setup resources for combine_css.
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(AbsolutifyUrl("a.css"),
                   default_css_header, ".a { color: red; }");
  SetFetchResponse(AbsolutifyUrl("b.css"),
                   default_css_header, ".b { color: green; }");
  SetFetchResponse(AbsolutifyUrl("c.css"),
                   default_css_header, ".c { color: blue; }");

  // Before and expected after text.
  const char before[] =
      "<html>\n"
      "  <head>\n"
      "    <link rel=stylesheet type=text/css href=a.css>\n"
      "    <link rel=stylesheet type=text/css href=b.css>\n"
      "    <link rel=stylesheet type=text/css href=c.css>\n"
      "  </head>\n"
      "</html>\n";
  const char after_template[] =
      "<html>\n"
      "<head>\n"
      "<link rel=stylesheet type=text/css href=%s />\n"
      "</head>\n"
      "</html>\n";
  GoogleString after = StringPrintf(after_template, Encode(
      kTestDomain, "cc", "0", MultiUrl("a.css", "b.css", "c.css"),
      "css").c_str());

  ValidateExpected("collapse_after_combine", before, after);
}

}  // namespace net_instaweb
