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

#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/custom_rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/null_statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/opt/logging/enums.pb.h"

namespace net_instaweb {

namespace {

const char kRequestUrl[]  = "http://www.example.com";
const char kCohortName[]  = "fix_reflow";
const char kNoscriptUrl[] = "http://www.example.com/?PageSpeed=noscript";

}  // namespace

class FixReflowFilterTest : public CustomRewriteTestBase<RewriteOptions> {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();  // rewrite_driver() is valid after this.
    const PropertyCache::Cohort* cohort = SetupCohort(
        server_context()->page_property_cache(), kCohortName);
    server_context()->set_fix_reflow_cohort(cohort);
    ResetDriver();
    options()->EnableFilter(RewriteOptions::kDeferJavascript);
    options()->EnableFilter(RewriteOptions::kFixReflows);
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
    const PropertyCache::Cohort* cohort = pcache->GetCohort(kCohortName);
    page->UpdateValue(cohort,
                      FixReflowFilter::kElementRenderedHeightPropertyName,
                      result);
  }

  void CheckFilterStatus(RewriterHtmlApplication::Status status) {
    ScopedMutex lock(rewrite_driver()->log_record()->mutex());
    EXPECT_EQ(status, logging_info()->rewriter_stats(0).html_status());
    EXPECT_EQ("fr", logging_info()->rewriter_stats(0).id());
  }

  NullStatistics stats_;
};

TEST_F(FixReflowFilterTest, NotInCache) {
  const GoogleString input_html =
      "<body>"
      "<div id=\"contentContainer\"><h1>Hello 1</h1>"
      "<div id=\"middleFooter\"><h3>Hello 3</h3></div></div>"
      "</body>";
  const GoogleString expected = StrCat(
      "<body>",
      StringPrintf(kNoScriptRedirectFormatter, kNoscriptUrl, kNoscriptUrl),
      "<div id=\"contentContainer\"><h1>Hello 1</h1>"
      "<div id=\"middleFooter\"><h3>Hello 3</h3></div></div>"
      "<script type=\"text/javascript\" src=\"/psajs/js_defer.0.js\"></script>"
      "</body>");

  ValidateExpectedUrl(kRequestUrl, input_html, expected);
  rewrite_driver_->log_record()->WriteLog();
  CheckFilterStatus(RewriterHtmlApplication::PROPERTY_CACHE_MISS);
}

TEST_F(FixReflowFilterTest, Disabled) {
  const GoogleString input_html =
      "<body>"
      "<div id=\"contentContainer\"><h1>Hello 1</h1>"
      "<div id=\"middleFooter\"><h3>Hello 3</h3></div></div>"
      "</body>";
  const GoogleString expected = input_html;

  SetCurrentUserAgent("junk");
  ValidateExpectedUrl(kRequestUrl, input_html, expected);
  rewrite_driver_->log_record()->WriteLog();
  CheckFilterStatus(RewriterHtmlApplication::DISABLED);
}

TEST_F(FixReflowFilterTest, InCache) {
  UpdatePcache("contentContainer:10px,");

  const GoogleString input_html =
      "<body>"
      "<div id=\"contentContainer\"><h1>Hello 1</h1>"
      "<div id=\"middleFooter\"><h3>Hello 3</h3></div></div>"
      "</body>";
  const GoogleString expected = StrCat(
      "<body>",
      StringPrintf(kNoScriptRedirectFormatter, kNoscriptUrl, kNoscriptUrl),
      "<div id=\"contentContainer\" style=\"min-height:10px\" "
      "data-pagespeed-fix-reflow=\"\"><h1>Hello 1</h1>"
      "<div id=\"middleFooter\"><h3>Hello 3</h3></div></div>"
      "<script type=\"text/javascript\" src=\"/psajs/js_defer.0.js\"></script>"
      "</body>");

  ValidateExpectedUrl(kRequestUrl, input_html, expected);
  LoggingInfo* info =rewrite_driver()->log_record()->logging_info();
  EXPECT_EQ(1, info->rewriter_info_size());
  EXPECT_EQ("fr", info->rewriter_info(0).id());
  EXPECT_EQ(RewriterApplication::APPLIED_OK, info->rewriter_info(0).status());
  rewrite_driver_->log_record()->WriteLog();
  CheckFilterStatus(RewriterHtmlApplication::ACTIVE);
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
  const GoogleString expected = StrCat(
      "<body>",
      StringPrintf(kNoScriptRedirectFormatter, kNoscriptUrl, kNoscriptUrl),
      "<div id=\"contentContainer\"><h1>Hello 1</h1>"
      "<div id=\"middleFooter\"><h3>Hello 3</h3></div></div>"
      "<script type=\"text/javascript\" src=\"/psajs/js_defer.0.js\"></script>"
      "</body>");
  ValidateExpectedUrl(kRequestUrl, input_html, expected);
  ResetDriver();

  UpdatePcache("middleFooter:10px,contentContainer:20px,");

  const GoogleString expected2 = StrCat(
      "<body>",
      StringPrintf(kNoScriptRedirectFormatter, kNoscriptUrl, kNoscriptUrl),
      "<div id=\"contentContainer\" style=\"min-height:20px\" "
      "data-pagespeed-fix-reflow=\"\"><h1>Hello 1</h1>"
      "<div id=\"middleFooter\" style=\"min-height:10px\" "
      "data-pagespeed-fix-reflow=\"\"><h3>Hello 3</h3>"
      "</div></div>"
      "<script type=\"text/javascript\" src=\"/psajs/js_defer.0.js\"></script>"
      "</body>");

  ValidateExpectedUrl(kRequestUrl, input_html, expected2);
}

}  // namespace net_instaweb
