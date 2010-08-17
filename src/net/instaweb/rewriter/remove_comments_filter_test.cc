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

// Author: mdsteele@google.com (Matthew D. Steele)

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/rewriter/public/remove_comments_filter.h"

namespace net_instaweb {

class RemoveCommentsFilterTest : public HtmlParseTestBase {
 protected:
  RemoveCommentsFilterTest()
      : remove_comments_filter_(&html_parse_) {
    html_parse_.AddFilter(&remove_comments_filter_);
  }

  virtual bool AddBody() const { return false; }

 private:
  RemoveCommentsFilter remove_comments_filter_;

  DISALLOW_COPY_AND_ASSIGN(RemoveCommentsFilterTest);
};

TEST_F(RemoveCommentsFilterTest, NoComments) {
  ValidateNoChanges("no_comments",
                    "<head><title>Hello</title></head>"
                    "<body>Why, hello there!</body>");
}

TEST_F(RemoveCommentsFilterTest, RemoveComment) {
  ValidateExpected("remove_comment",
                   "<body>hello <!--world--></body>",
                   "<body>hello </body>");
}

TEST_F(RemoveCommentsFilterTest, RemoveMultipleComments) {
  ValidateExpected("remove_multiple_comments",
                   "<head><!--1--><title>Hi<!--2--></title></head>"
                   "<body><!--3-->hello<!--4--><!--5--></body>",
                   "<head><title>Hi</title></head>"
                   "<body>hello</body>");
}

TEST_F(RemoveCommentsFilterTest, DoNotRemoveIEDirective) {
  ValidateNoChanges("do_not_remove_ie_directive",
                    "<body>hello <!--[if IE 8]>world<![endif]--></body>");
}

}  // namespace net_instaweb
