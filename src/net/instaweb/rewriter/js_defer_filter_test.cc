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

// Author: atulvasu@google.com (Atul Vasu)

#include "net/instaweb/rewriter/public/js_defer_filter.h"

#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class JsDeferFilterTest : public HtmlParseTestBase {
 protected:
  JsDeferFilterTest()
      : js_defer_filter_(&html_parse_) {
    html_parse_.AddFilter(&js_defer_filter_);
  }

  virtual bool AddBody() const { return false; }

 private:
  JsDeferFilter js_defer_filter_;

  DISALLOW_COPY_AND_ASSIGN(JsDeferFilterTest);
};

TEST_F(JsDeferFilterTest, DeferScript) {
  ValidateExpected("defer_script",
      "<head>"
      "<script src='http://www.google.com/javascript/ajax_apis.js'></script>"
      "<script> func(); </script>"
      "</head><body>Hello, world!</body>",
      StrCat("<head></head><body>Hello, world!"
             "<script type=\"text/javascript\">",
             JsDeferFilter::kDeferJsCode,
             "\npagespeed.deferInit();\n",
             "pagespeed.deferJs.addUrl("
             "\"http://www.google.com/javascript/ajax_apis.js\");\n"
             "pagespeed.deferJs.addStr(\" func(); \");\n"
             "</script></body>"));
}

TEST_F(JsDeferFilterTest, DeferScriptMultiBody) {
  ValidateExpected("defer_script_multi_body",
      "<head>"
      "<script src='http://www.google.com/javascript/ajax_apis.js'></script>"
      "<script> func(); </script>"
      "</head><body>Hello, world!</body><body>"
      "<script> func2(); </script> </body>",
      StrCat("<head></head><body>Hello, world!"
             "<script type=\"text/javascript\">",
             JsDeferFilter::kDeferJsCode,
             "\npagespeed.deferInit();\n",
             "pagespeed.deferJs.addUrl("
             "\"http://www.google.com/javascript/ajax_apis.js\");\n"
             "pagespeed.deferJs.addStr(\" func(); \");\n"
             "</script></body><body> "
             "<script type=\"text/javascript\">"
             "pagespeed.deferJs.addStr(\" func2(); \");\n"
             "</script></body>"));
}

}  // namespace net_instaweb
