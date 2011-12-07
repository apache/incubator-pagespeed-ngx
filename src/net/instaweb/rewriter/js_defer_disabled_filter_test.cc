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

#include "net/instaweb/rewriter/public/js_defer_disabled_filter.h"

#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class JsDeferDisabledFilterTest : public HtmlParseTestBase {
 protected:
  JsDeferDisabledFilterTest()
      : js_defer_disabled_filter_(&html_parse_) {
    html_parse_.AddFilter(&js_defer_disabled_filter_);
  }

  virtual bool AddBody() const { return false; }

 private:
  JsDeferDisabledFilter js_defer_disabled_filter_;

  DISALLOW_COPY_AND_ASSIGN(JsDeferDisabledFilterTest);
};

TEST_F(JsDeferDisabledFilterTest, DeferScript) {
  ValidateExpected("defer_script",
      "<head>"
      "<noscript psa_disabled><script "
      "src='http://www.google.com/javascript/ajax_apis.js'></script></noscript>"
      "<noscript psa_disabled><script> func(); </script></noscript>"
      "</head><body>Hello, world!</body>",
      StrCat("<head>"
             "<noscript psa_disabled><script "
             "src='http://www.google.com/javascript/ajax_apis.js'></script>"
             "</noscript>"
             "<noscript psa_disabled><script> func(); </script></noscript>"
             "</head><body>Hello, world!"
             "<script type=\"text/javascript\">",
             JsDeferDisabledFilter::kDeferJsCode,
             "\npagespeed.deferInit();\n",
             "pagespeed.deferJs.registerNoScriptTags();\n",
             "pagespeed.addOnload(window, function() {\n"
             "  pagespeed.deferJs.run();\n"
             "});\n</script></body>"));
}

TEST_F(JsDeferDisabledFilterTest, DeferScriptMultiBody) {
  ValidateExpected("defer_script_multi_body",
      "<head>"
      "<noscript psa_disabled>"
      "<script src='http://www.google.com/javascript/ajax_apis.js'></script>"
      "</noscript>"
      "<noscript psa_disabled><script> func(); </script></noscript>"
      "</head><body>Hello, world!</body><body>"
      "<noscript psa_disabled><script> func2(); </script></noscript></body>",
      StrCat("<head>"
             "<noscript psa_disabled>"
             "<script src='http://www.google.com/javascript/ajax_apis.js'>"
             "</script></noscript>"
             "<noscript psa_disabled><script> func(); </script></noscript>"
             "</head><body>Hello, world!"
             "<script type=\"text/javascript\">",
             JsDeferDisabledFilter::kDeferJsCode, "\n"
             "pagespeed.deferInit();\n",
             "pagespeed.deferJs.registerNoScriptTags();\n",
             "pagespeed.addOnload(window, function() {\n"
             "  pagespeed.deferJs.run();\n"
             "});\n"
             "</script></body><body><noscript psa_disabled><script> func2(); "
             "</script></noscript></body>"));
}

}  // namespace net_instaweb
