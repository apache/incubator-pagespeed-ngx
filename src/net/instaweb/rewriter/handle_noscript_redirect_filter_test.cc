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

#include "net/instaweb/rewriter/public/handle_noscript_redirect_filter.h"

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class HandleNoscriptRedirectFilterTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    options()->EnableFilter(RewriteOptions::kHandleNoscriptRedirect);
    ResourceManagerTestBase::SetUp();
    rewrite_driver()->AddFilters();
  }
};

TEST_F(HandleNoscriptRedirectFilterTest, TestOneHead) {
  GoogleString input_html =
      "<head></head><body><img src=\"http://test.com/1.jpeg\"/></body>";
  GoogleString output_html =
      "<head><link rel=\"canonical\" href=\"http://test.com/one_head.html\"/>"
      "</head><body><img src=\"http://test.com/1.jpeg\"/></body>";
  ValidateExpected("one_head", input_html, output_html);
}

TEST_F(HandleNoscriptRedirectFilterTest, TestMultipleHeads) {
  GoogleString input_html =
      "<head></head><head></head>"
      "<body><img src=\"http://test.com/1.jpeg\"/></body>";
  GoogleString output_html =
      "<head>"
      "<link rel=\"canonical\" href=\"http://test.com/multiple_heads.html\"/>"
      "</head><head></head><body><img src=\"http://test.com/1.jpeg\"/></body>";
  ValidateExpected("multiple_heads", input_html, output_html);
}

TEST_F(HandleNoscriptRedirectFilterTest, TestNoHead) {
  GoogleString input_html =
      "<body><img src=\"http://test.com/1.jpeg\"/></body>";
  GoogleString output_html =
      "<head><link rel=\"canonical\" href=\"http://test.com/no_head.html\"/>"
      "</head><body><img src=\"http://test.com/1.jpeg\"/></body>";
  ValidateExpected("no_head", input_html, output_html);
}

TEST_F(HandleNoscriptRedirectFilterTest, TestOneHeadCanonical) {
  GoogleString input_html =
      "<head><link rel=\"canonical\" href=\"http://test.com/foo.html\"></head>"
      "<body><img src=\"http://test.com/1.jpeg\"/></body>";
  ValidateNoChanges("one_head_canonical", input_html);
}

TEST_F(HandleNoscriptRedirectFilterTest, TestTwoHeadCanonical) {
  GoogleString input_html =
      "<head></head>"
      "<head><link rel=\"canonical\" href=\"http://test.com/foo.html\"/>"
      "</head><body><img src=\"http://test.com/1.jpeg\"/></body>";
  GoogleString output_html =
      "<head><link rel=\"canonical\" "
      "href=\"http://test.com/two_head_canonical.html\"/></head>"
      "<head><link rel=\"canonical\" href=\"http://test.com/foo.html\"/></head>"
      "<body><img src=\"http://test.com/1.jpeg\"/></body>";
  ValidateExpected("two_head_canonical", input_html, output_html);
}

TEST_F(HandleNoscriptRedirectFilterTest, TestTwoLinksInHead) {
  GoogleString input_html =
      "<head><link rel=\"canonical\" href=\"http://test.com/foo.html\">"
      "<link href=special.css rel=stylesheet type=text/css/></head>"
      "<body><img src=\"http://test.com/1.jpeg\"/></body>";
  ValidateNoChanges("two_links_in_head", input_html);
}

}  // namespace net_instaweb
