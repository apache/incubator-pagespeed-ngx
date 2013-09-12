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
#include "net/instaweb/rewriter/public/custom_rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/null_statistics.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/abstract_mutex.h"

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

  void CheckFilterStatus(RewriterHtmlApplication::Status status) {
    ScopedMutex lock(rewrite_driver()->log_record()->mutex());
    EXPECT_EQ(status, logging_info()->rewriter_stats(0).html_status());
    EXPECT_EQ("fr", logging_info()->rewriter_stats(0).id());
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

  rewrite_driver_->SetUserAgent("junk");
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
  const GoogleString expected =
      "<body>"
      "<div id=\"contentContainer\" style=\"min-height:10px\" "
      "data-pagespeed-fix-reflow=\"\"><h1>Hello 1</h1>"
      "<div id=\"middleFooter\"><h3>Hello 3</h3></div></div>"
      "</body>";

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
  const GoogleString expected = input_html;
  ValidateExpectedUrl(kRequestUrl, input_html, expected);
  ResetDriver();

  UpdatePcache("middleFooter:10px,contentContainer:20px,");

  const GoogleString expected2 =
      "<body>"
      "<div id=\"contentContainer\" style=\"min-height:20px\" "
      "data-pagespeed-fix-reflow=\"\"><h1>Hello 1</h1>"
      "<div id=\"middleFooter\" style=\"min-height:10px\" "
      "data-pagespeed-fix-reflow=\"\"><h3>Hello 3</h3>"
      "</div></div></body>";

  ValidateExpectedUrl(kRequestUrl, input_html, expected2);
}

}  // namespace net_instaweb
