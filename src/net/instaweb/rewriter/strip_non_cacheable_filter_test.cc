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

// Author: rahulbansal@google.com (Rahul Bansal)

#include "net/instaweb/rewriter/public/strip_non_cacheable_filter.h"

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

namespace {

const char kRequestUrl[] = "http://www.test.com";

const char kHtmlInput[] =
    "<html>"
    "<body>\n"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<h2 id=\"beforeItems\"> This is before Items </h2>"
      "<div class=\"item\">"
         "<img src=\"image1\">"
         "<img src=\"image2\">"
      "</div>"
      "<div class=\"item\">"
         "<img src=\"image3\">"
          "<div class=\"item\">"
             "<img src=\"image4\">"
          "</div>"
      "</div>"
    "</body></html>";

}  // namespace

class StripNonCacheableFilterTest : public ResourceManagerTestBase {
 public:
  StripNonCacheableFilterTest() {}

  virtual void SetUp() {
    delete options_;
    options_ = new RewriteOptions();
    options_->EnableFilter(RewriteOptions::kStripNonCacheable);
    options_->set_prioritize_visible_content_non_cacheable_elements(
        "class=item\nid=beforeItems");
    SetUseManagedRewriteDrivers(true);
    ResourceManagerTestBase::SetUp();
  }

  virtual bool AddHtmlTags() const { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(StripNonCacheableFilterTest);
};

TEST_F(StripNonCacheableFilterTest, StripNonCacheable) {
  GoogleString json_expected_output =
      "<html><body>\n"
      "<div id=\"header\"> This is the header </div>"
      "<div id=\"container\" class>"
      "<!--GooglePanel begin panel-id-1.0--><!--GooglePanel end panel-id-1.0-->"
      "<!--GooglePanel begin panel-id-0.0--><!--GooglePanel end panel-id-0.0-->"
      "<!--GooglePanel begin panel-id-0.1--><!--GooglePanel end panel-id-0.1-->"
      "</body></html>";
  ValidateExpectedUrl(kRequestUrl, kHtmlInput, json_expected_output);
}

}  // namespace net_instaweb
