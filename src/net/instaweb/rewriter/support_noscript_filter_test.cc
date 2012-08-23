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

#include "net/instaweb/rewriter/public/support_noscript_filter.h"

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class SupportNoscriptFilterTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    rewrite_driver()->AddOwnedPostRenderFilter(
        new SupportNoscriptFilter(rewrite_driver()));
  }
};

TEST_F(SupportNoscriptFilterTest, TestNoscript) {
  GoogleString input_html =
      "<head></head><body>"
      "<img src=\"http://test.com/1.jpeg\"/></body>";
  GoogleString output_html =
      "<head></head><body>"
      "<noscript><meta HTTP-EQUIV=\"refresh\" content=\"0;"
      "url='http://test.com/support_noscript%27%22.html?ModPagespeed=noscript'\">"
      "<style><!--table,div,span,font,p{display:none} --></style>"
      "<div style=\"display:block\">Please click "
      "<a href=\"http://test.com/support_noscript%27%22.html?ModPagespeed=noscript\">"
      "here</a> if you are not redirected within a few seconds.</div>"
      "</noscript><img src=\"http://test.com/1.jpeg\"/></body>";
  ValidateExpected("support_noscript'\"", input_html, output_html);
}

TEST_F(SupportNoscriptFilterTest, TestNoscriptMultipleBodies) {
  GoogleString input_html =
      "<head></head><body>"
      "<img src=\"http://test.com/1.jpeg\"/></body>"
      "<body><img src=\"http://test.com/2.jpeg\"/></body>";
  GoogleString output_html =
      "<head></head><body>"
      "<noscript><meta HTTP-EQUIV=\"refresh\" content=\"0;"
      "url='http://test.com/support_noscript.html?ModPagespeed=noscript'\">"
      "<style><!--table,div,span,font,p{display:none} --></style>"
      "<div style=\"display:block\">Please click "
      "<a href=\"http://test.com/support_noscript.html?ModPagespeed=noscript\">"
      "here</a> if you are not redirected within a few seconds.</div>"
      "</noscript><img src=\"http://test.com/1.jpeg\"/></body>"
      "<body><img src=\"http://test.com/2.jpeg\"/></body>";
  ValidateExpected("support_noscript", input_html, output_html);
}

TEST_F(SupportNoscriptFilterTest, TestNoBody) {
  GoogleString input_html =
      "<head></head>";
  ValidateExpected("support_noscript", input_html, input_html);
}

}  // namespace net_instaweb
