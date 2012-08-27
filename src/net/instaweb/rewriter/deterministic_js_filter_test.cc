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
// Author: mmohabey@google.com (Megha Mohabey)

#include "net/instaweb/rewriter/public/deterministic_js_filter.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class DeterministicJsFilterTest : public ResourceManagerTestBase {
 public:
  DeterministicJsFilterTest() {}
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    deterministic_js_filter_.reset(new DeterministicJsFilter(rewrite_driver()));
    rewrite_driver()->AddFilter(deterministic_js_filter_.get());
  }

 private:
  scoped_ptr<DeterministicJsFilter> deterministic_js_filter_;

  DISALLOW_COPY_AND_ASSIGN(DeterministicJsFilterTest);
};

TEST_F(DeterministicJsFilterTest, DeterministicJsInjection) {
  StringPiece deterministic_js_code =
      resource_manager()->static_javascript_manager()->GetJsSnippet(
          StaticJavascriptManager::kDeterministicJs, options());
  GoogleString expected_str = StrCat("<head><script type=\"text/javascript\" "
                                     "pagespeed_no_defer>",
                                     deterministic_js_code,
                                     "</script></head><body></body>");

  // Check if StaticJavascriptManager populated the script correctly.
  EXPECT_NE(GoogleString::npos, deterministic_js_code.find("Date"));
  EXPECT_NE(GoogleString::npos, deterministic_js_code.find("Math.random"));
  // Check if the deterministic js is inserted correctly.
  ValidateExpected("deterministicJs_injection",
                   "<head></head><body></body>",
                   expected_str);
}

TEST_F(DeterministicJsFilterTest, DeterministicJsInjectionWithSomeHeadContent) {
  StringPiece deterministic_js_code =
      resource_manager()->static_javascript_manager()->GetJsSnippet(
          StaticJavascriptManager::kDeterministicJs, options());
  GoogleString expected_str = StrCat("<head><script type=\"text/javascript\" "
                                     "pagespeed_no_defer>",
                                     deterministic_js_code,
                                     "</script>"
                                     "<link rel=\"stylesheet\" href=\"a.css\">"
                                     "</head><body></body>");

  // Check if StaticJavascriptManager populated the script correctly.
  EXPECT_NE(GoogleString::npos, deterministic_js_code.find("Date"));
  EXPECT_NE(GoogleString::npos, deterministic_js_code.find("Math.random"));
  // Check if the deterministic js is inserted correctly.
  ValidateExpected("deterministicJs_injection",
                   "<head><link rel=\"stylesheet\" href=\"a.css\">"
                   "</head><body></body>",
                   expected_str);
}
}  // namespace net_instaweb
