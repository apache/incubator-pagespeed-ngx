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

// Author: sriharis@google.com (Srihari Sukumaran)

#include "net/instaweb/rewriter/public/detect_reflow_js_defer_filter.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class DetectReflowJsDeferFilterTest : public RewriteTestBase {
 protected:
  // TODO(matterbury): Delete this method as it should be redundant.
  virtual void SetUp() {
    RewriteTestBase::SetUp();
  }

  void InitDetectReflowJsDeferFilter() {
    detect_reflow_filter_.reset(new DetectReflowJsDeferFilter(
        rewrite_driver()));
    rewrite_driver()->AddFilter(detect_reflow_filter_.get());
  }

  scoped_ptr<DetectReflowJsDeferFilter> detect_reflow_filter_;
};

TEST_F(DetectReflowJsDeferFilterTest, DetectReflow) {
  InitDetectReflowJsDeferFilter();
  StringPiece detect_reflow_code =
      resource_manager()->static_javascript_manager()->GetJsSnippet(
          StaticJavascriptManager::kDetectReflowJs, options());
  ValidateExpected("detect_reflow",
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
             "<script type=\"text/javascript\" pagespeed_no_defer=\"\">",
             detect_reflow_code,
             "</script></head><body>Hello, world!</body>"));
}

TEST_F(DetectReflowJsDeferFilterTest, DetectReflowNoHead) {
  InitDetectReflowJsDeferFilter();
  StringPiece detect_reflow_code =
      resource_manager()->static_javascript_manager()->GetJsSnippet(
          StaticJavascriptManager::kDetectReflowJs, options());
  ValidateExpected("detect_reflow_no_head",
      "<body>Hello, world!</body>"
      "<body><script type='text/psajs'"
      "> func();</script></body>",
      StrCat("<head>"
             "<script type=\"text/javascript\" pagespeed_no_defer=\"\">",
             detect_reflow_code,
             "</script></head>"
             "<body>Hello, world!</body>"
             "<body><script type='text/psajs'"
             "> func();</script></body>"));
}

TEST_F(DetectReflowJsDeferFilterTest, InvalidUserAgent) {
  InitDetectReflowJsDeferFilter();
  rewrite_driver()->set_user_agent("BlackListUserAgent");
  const char script[] = "<head>"
      "<script type='text/psajs' "
      "src='http://www.google.com/javascript/ajax_apis.js'></script>"
      "<script type='text/psajs'"
      "> func();</script>"
      "</head><body>Hello, world!</body>";

  ValidateNoChanges("detect_reflow", script);
}

}  // namespace net_instaweb
