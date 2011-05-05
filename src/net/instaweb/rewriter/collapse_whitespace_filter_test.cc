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

#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class CollapseWhitespaceFilterTest : public HtmlParseTestBase {
 protected:
  CollapseWhitespaceFilterTest()
      : collapse_whitespace_filter_(&html_parse_) {
    html_parse_.AddFilter(&collapse_whitespace_filter_);
  }

  virtual bool AddBody() const { return false; }

 private:
  CollapseWhitespaceFilter collapse_whitespace_filter_;

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

}  // namespace net_instaweb
