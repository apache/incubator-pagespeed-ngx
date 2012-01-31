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

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class JsDeferDisabledFilterTest : public ResourceManagerTestBase {
 protected:
  JsDeferDisabledFilterTest() {
    JsDeferDisabledFilter::Initialize(NULL);
  }

  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    js_defer_disabled_filter_.reset(
        new JsDeferDisabledFilter(rewrite_driver()));
    rewrite_driver()->AddFilter(js_defer_disabled_filter_.get());
  }

  virtual ~JsDeferDisabledFilterTest() {
    JsDeferDisabledFilter::Terminate();
  }

  virtual bool AddBody() const { return false; }

  scoped_ptr<JsDeferDisabledFilter> js_defer_disabled_filter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(JsDeferDisabledFilterTest);
};

TEST_F(JsDeferDisabledFilterTest, DeferScript) {
  ValidateExpected("defer_script",
      "<head>"
      "<script type='text/psajs' "
      "src='http://www.google.com/javascript/ajax_apis.js'></script>"
      "<script type='text/psajs'"
      "> func();</script>"
      "</head><body>Hello, world!</body>",
      StrCat("<head>"
             "<script type='text/psajs' "
             "src='http://www.google.com/javascript/ajax_apis.js'></script>"
             "<script type='text/psajs'"
             "> func();</script>"
             "</head><body>Hello, world!"
             "<script type=\"text/javascript\">",
             JsDeferDisabledFilter::defer_js_code(),
             "</script></body>"));
}

TEST_F(JsDeferDisabledFilterTest, DeferScriptMultiBody) {
  ValidateExpected("defer_script_multi_body",
      "<head>"
      "<script type='text/psajs' "
      "src='http://www.google.com/javascript/ajax_apis.js'></script>"
      "<script type='text/psajs'> func(); </script>"
      "</head><body>Hello, world!</body><body>"
      "<script type='text/psajs'> func2(); </script></body>",
      StrCat("<head>"
             "<script type='text/psajs' "
             "src='http://www.google.com/javascript/ajax_apis.js'></script>"
             "<script type='text/psajs'> func(); </script>"
             "</head><body>Hello, world!"
             "<script type=\"text/javascript\">",
             JsDeferDisabledFilter::defer_js_code(),
             "</script></body><body><script type='text/psajs'> func2(); "
             "</script></body>"));
}

TEST_F(JsDeferDisabledFilterTest, DeferScriptOptimized) {
  js_defer_disabled_filter_->set_debug(false);
  Parse("optimized",
        "<body><script type='text/psajs' src='foo.js'></script></body>");
  EXPECT_TRUE(output_buffer_.find("/*") == GoogleString::npos)
      << "There should be no comments in the optimized code";
}

TEST_F(JsDeferDisabledFilterTest, DeferScriptDebug) {
  js_defer_disabled_filter_->set_debug(true);
  Parse("optimized",
        "<body><script type='text/psajs' src='foo.js'></script></body>");
  EXPECT_TRUE(output_buffer_.find("/*") != GoogleString::npos)
      << "There should still be some comments in the debug code";
}

TEST_F(JsDeferDisabledFilterTest, InvalidUserAgent) {
  rewrite_driver()->set_user_agent("BlackListUserAgent");
  const char script[] = "<head>"
      "<script type='text/psajs' "
      "src='http://www.google.com/javascript/ajax_apis.js'></script>"
      "<script type='text/psajs'"
      "> func();</script>"
      "</head><body>Hello, world!</body>";

  ValidateNoChanges("defer_script", script);
}

}  // namespace net_instaweb
