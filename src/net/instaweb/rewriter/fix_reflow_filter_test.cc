/*
 * Copyright 2013 Google Inc.
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

#include "net/instaweb/rewriter/public/fix_reflow_filter.h"

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/custom_rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/null_statistics.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const char kRequestUrl[] = "http://www.example.com";
const char cohort_name[] = "fix_reflow";

}  // namespace

class FixReflowFilterTest : public CustomRewriteTestBase<RewriteOptions> {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();  // rewrite_driver() is valid after this.
    const PropertyCache::Cohort* cohort = SetupCohort(
        server_context()->page_property_cache(), cohort_name);
    server_context()->set_fix_reflow_cohort(cohort);
    ResetDriver();
    options()->EnableFilter(RewriteOptions::kDeferJavascript);
    fix_reflow_filter_.reset(new FixReflowFilter(rewrite_driver()));
    rewrite_driver()->AddFilter(fix_reflow_filter_.get());
  }

  void ResetDriver() {
    rewrite_driver()->Clear();
    rewrite_driver()->set_request_context(
        RequestContext::NewTestRequestContext(factory()->thread_system()));
    PropertyCache* pcache = server_context_->page_property_cache();
    MockPropertyPage* page = NewMockPage(kRequestUrl);
    rewrite_driver()->set_property_page(page);
    pcache->set_enabled(true);
    pcache->Read(page);
  }

  void UpdatePcache(StringPiece result) {
    PropertyPage* page = rewrite_driver()->property_page();
    PropertyCache* pcache = server_context_->page_property_cache();
    const PropertyCache::Cohort* cohort = pcache->GetCohort(cohort_name);
    page->UpdateValue(cohort,
                      FixReflowFilter::kElementRenderedHeightPropertyName,
                      result);
  }

  NullStatistics stats_;
  scoped_ptr<FixReflowFilter> fix_reflow_filter_;
};

TEST_F(FixReflowFilterTest, NotInCache) {
  const GoogleString input_html =
      "<body>"
      "<div id=\"contentContainer\"><h1>Hello 1</h1>"
      "<div id=\"middleFooter\"><h3>Hello 3</h3></div></div>"
      "</body>";
  const GoogleString expected = input_html;

  ValidateExpectedUrl(kRequestUrl, input_html, expected);
}

TEST_F(FixReflowFilterTest, InCache) {
  UpdatePcache("contentContainer:10px,");

  const GoogleString input_html =
      "<body>"
      "<div id=\"contentContainer\"><h1>Hello 1</h1>"
      "<div id=\"middleFooter\"><h3>Hello 3</h3></div></div>"
      "</body>";
  const GoogleString expected =
      "<body>"
      "<div id=\"contentContainer\" style=\"min-height:10px\"><h1>Hello 1</h1>"
      "<div id=\"middleFooter\"><h3>Hello 3</h3></div></div>"
      "</body>";

  ValidateExpectedUrl(kRequestUrl, input_html, expected);
}

TEST_F(FixReflowFilterTest, InCacheExpires) {
  UpdatePcache("contentContainer:10px,");

  int64 cache_ttl_ms =
      rewrite_driver()->options()->finder_properties_cache_expiration_time_ms();

  AdvanceTimeMs(cache_ttl_ms + 10);

  const GoogleString input_html =
      "<body>"
      "<div id=\"contentContainer\"><h1>Hello 1</h1>"
      "<div id=\"middleFooter\"><h3>Hello 3</h3></div></div>"
      "</body>";
  const GoogleString expected = input_html;
  ValidateExpectedUrl(kRequestUrl, input_html, expected);
  ResetDriver();

  UpdatePcache("middleFooter:10px,contentContainer:20px,");

  const GoogleString expected2 =
      "<body>"
      "<div id=\"contentContainer\" style=\"min-height:20px\"><h1>Hello 1</h1>"
      "<div id=\"middleFooter\" style=\"min-height:10px\"><h3>Hello 3</h3>"
      "</div></div></body>";

  ValidateExpectedUrl(kRequestUrl, input_html, expected2);
}

}  // namespace net_instaweb
