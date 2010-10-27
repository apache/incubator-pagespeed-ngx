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
#include "net/instaweb/rewriter/public/elide_attributes_filter.h"

namespace net_instaweb {

class ElideAttributesFilterTest : public HtmlParseTestBase {
 protected:
  ElideAttributesFilterTest()
      : elide_attributes_filter_(&html_parse_) {
    html_parse_.AddFilter(&elide_attributes_filter_);
  }

  virtual bool AddBody() const { return false; }

 private:
  ElideAttributesFilter elide_attributes_filter_;

  DISALLOW_COPY_AND_ASSIGN(ElideAttributesFilterTest);
};

TEST_F(ElideAttributesFilterTest, NoChanges) {
  ValidateNoChanges("no_changes",
                    "<head><script src=\"foo.js\"></script></head>"
                    "<body><form method=\"post\">"
                    "<input type=\"checkbox\" checked>"
                    "</form></body>");
}

TEST_F(ElideAttributesFilterTest, RemoveAttrWithDefaultValue) {
  ValidateExpected("remove_attr_with_default_value",
                   "<head></head><body><form method=get></form></body>",
                   "<head></head><body><form></form></body>");
}

TEST_F(ElideAttributesFilterTest, RemoveAttrWithIgnoredValue) {
  ValidateExpected("remove_attr_with_ignored_value",
                   "<head><script src=\"foo.js\" type=\"bleh\"></script>"
                   "</head><body></body>",
                   "<head><script src=\"foo.js\"></script>"
                   "</head><body></body>");
}

// TODO(mdsteele): Add a test that ensures that this _doesn't_ happen for an
// XHTML document (but HtmlParseTestBase automatically adds <html> around the
// string, so we need to make sure the doctype is in the right place).
TEST_F(ElideAttributesFilterTest, RemoveValueFromAttr) {
  ValidateExpected("remove_value_from_attr",
                   "<head></head><body><form>"
                   "<input type=checkbox checked=checked></form></body>",
                   "<head></head><body><form>"
                   "<input type=checkbox checked></form></body>");
}

TEST_F(ElideAttributesFilterTest, DoNotBreakVBScript) {
  ValidateExpected("do_not_break_vbscript",
                   "<head><script language=\"JavaScript\">var x=1;</script>"
                   "<script language=\"VBScript\">"
                   "Sub foo(ByVal bar)\n  call baz(bar)\nend sub"
                   "</script></head><body></body>",
                   // Remove language="JavaScript", but not the VBScript one:
                   "<head><script>var x=1;</script>"
                   "<script language=\"VBScript\">"
                   "Sub foo(ByVal bar)\n  call baz(bar)\nend sub"
                   "</script></head><body></body>");
}

}  // namespace net_instaweb
