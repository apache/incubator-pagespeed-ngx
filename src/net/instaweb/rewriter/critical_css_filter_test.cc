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

// Author: slamm@google.com (Stephen Lamm)

#include <utility>
#include <vector>

#include "net/instaweb/rewriter/public/critical_css_filter.h"

#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/mock_critical_css_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/util/wildcard.h"

namespace {

const char kRequestUrl[] = "http://test.com";

}  // namespace

namespace net_instaweb {

class CriticalCssFilterTest : public RewriteTestBase {
 public:
  CriticalCssFilterTest() {  }

  virtual bool AddHtmlTags() const { return false; }

 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();

    SetHtmlMimetype();  // Don't wrap scripts in <![CDATA[ ]]>

    finder_ = new MockCriticalCssFinder(rewrite_driver(), statistics());
    server_context()->set_critical_css_finder(finder_);

    filter_.reset(new CriticalCssFilter(rewrite_driver(), finder_));
    rewrite_driver()->AddFilter(filter_.get());

    ResetDriver();

    options_->DisableFilter(RewriteOptions::kDebug);
  }

  void ResetDriver() {
    PropertyCache* pcache = page_property_cache();
    server_context_->set_enable_property_cache(true);
    const PropertyCache::Cohort* dom_cohort =
        SetupCohort(pcache, RewriteDriver::kDomCohort);
    server_context()->set_dom_cohort(dom_cohort);

    MockPropertyPage* page = NewMockPage(kRequestUrl);
    rewrite_driver()->set_property_page(page);
    pcache->set_enabled(true);
    pcache->Read(page);
  }

  typedef std::vector<std::pair<int, int> > ExpApplicationVector;
  void ValidateRewriterLogging(
      RewriterHtmlApplication::Status html_status,
      ExpApplicationVector expected_application_counts) {
    rewrite_driver()->log_record()->WriteLog();

    const char* id = RewriteOptions::FilterId(
        RewriteOptions::kPrioritizeCriticalCss);

    const LoggingInfo& logging_info =
        *rewrite_driver()->log_record()->logging_info();
    ASSERT_EQ(1, logging_info.rewriter_stats_size());
    const RewriterStats& rewriter_stats = logging_info.rewriter_stats(0);
    EXPECT_EQ(id, rewriter_stats.id());

    EXPECT_EQ(html_status, rewriter_stats.html_status());
    EXPECT_EQ(expected_application_counts.size(),
              rewriter_stats.status_counts_size());
    for (int i = 0; i < expected_application_counts.size(); i++) {
      EXPECT_EQ(expected_application_counts[i].first,
                rewriter_stats.status_counts(i).application_status());
      EXPECT_EQ(expected_application_counts[i].second,
                rewriter_stats.status_counts(i).count());
    }
  }

  scoped_ptr<CriticalCssFilter> filter_;
  MockCriticalCssFinder* finder_;  // owned by server_context
};

TEST_F(CriticalCssFilterTest, UnchangedWhenPcacheEmpty) {
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"
      "  <style type='text/css'>a {color: red }</style>\n"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  ValidateExpected("unchanged_when_pcache_empty", input_html, input_html);

  ExpApplicationVector exp_application_counts;
  ValidateRewriterLogging(RewriterHtmlApplication::PROPERTY_CACHE_MISS,
                          exp_application_counts);

  // Validate logging.
  ASSERT_FALSE(
      rewrite_driver()->log_record()->logging_info()->has_critical_css_info());
}

// Similar to empty pcache case above except rewriter logged as "ACTIVE".
TEST_F(CriticalCssFilterTest, UnchangedWithNoCriticalRules) {
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css'>"
      "</body>\n";

  // When WKH returns an empty result, the finder still writes empty
  // critical CSS stats to the property cache. Simulate that.
  finder_->SetCriticalCssStats(0, 0, 0);

  ValidateExpected("unchanged_with_no_critical_rules", input_html, input_html);

  ExpApplicationVector exp_application_counts;
  ValidateRewriterLogging(RewriterHtmlApplication::ACTIVE,
                          exp_application_counts);

  // Validate logging.
  ASSERT_FALSE(
      rewrite_driver()->log_record()->logging_info()->has_critical_css_info());
}

TEST_F(CriticalCssFilterTest, DisabledForIE) {
  // Critical CSS is disabed in IE (awaiting conditional comment support).
  rewrite_driver()->SetUserAgent(UserAgentMatcherTestBase::kIe7UserAgent);
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css'>"
      "</body>\n";
  finder_->AddCriticalCss("http://test.com/a.css", "a_used {color: azure }", 1);
  ValidateExpected("disabled_for_ie", input_html, input_html);

  ExpApplicationVector exp_application_counts;
  ValidateRewriterLogging(RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED,
                          exp_application_counts);

  // Validate logging.
  ASSERT_FALSE(
      rewrite_driver()->log_record()->logging_info()->has_critical_css_info());
}

TEST_F(CriticalCssFilterTest, InlineAndMove) {
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  GoogleString expected_html = StrCat(
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <style media=\"print\">a_used {color: azure }</style>"
      "<style>b_used {color: blue }</style>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <style>c_used {color: cyan }</style>\n"
      "</body>\n"
      "<noscript id=\"psa_add_styles\">"
      "<link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>"
      "<style type='text/css'>t {color: turquoise }</style>"
      "<link rel='stylesheet' href='c.css' type='text/css'>"
      "</noscript>"
      "<script pagespeed_no_defer=\"\" type=\"text/javascript\">",
      CriticalCssFilter::kAddStylesScript,
      "window['pagespeed'] = window['pagespeed'] || {};"
      "window['pagespeed']['criticalCss'] = {"
      "  'total_critical_inlined_size': 64,"
      "  'total_original_external_size': 6,"
      "  'total_overhead_size': 85,"
      "  'num_replaced_links': 3,"
      "  'num_unreplaced_links': 0"
      "};"
      "</script>");

  finder_->AddCriticalCss("http://test.com/a.css", "a_used {color: azure }", 1);
  finder_->AddCriticalCss("http://test.com/b.css", "b_used {color: blue }", 2);
  finder_->AddCriticalCss("http://test.com/c.css", "c_used {color: cyan }", 3);

  ValidateExpected("inline_and_move", input_html, expected_html);

  ExpApplicationVector exp_application_counts;
  exp_application_counts.push_back(
      make_pair(RewriterApplication::APPLIED_OK, 3));
  ValidateRewriterLogging(RewriterHtmlApplication::ACTIVE,
                          exp_application_counts);

  // Validate logging.
  const CriticalCssInfo& info =
      rewrite_driver()->log_record()->logging_info()->critical_css_info();
  ASSERT_EQ(64, info.critical_inlined_bytes());
  ASSERT_EQ(6, info.original_external_bytes());
  ASSERT_EQ(85, info.overhead_bytes());
}

TEST_F(CriticalCssFilterTest, InlineAndDontFlushEarlyIfFlagDisabled) {
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  GoogleString expected_html = StrCat(
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <style media=\"print\">a_used {color: azure }</style>"
      "<style>b_used {color: blue }</style>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <style>c_used {color: cyan }</style>\n"
      "</body>\n"
      "<noscript id=\"psa_add_styles\">"
      "<link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>"
      "<style type='text/css'>t {color: turquoise }</style>"
      "<link rel='stylesheet' href='c.css' type='text/css'>"
      "</noscript>"
      "<script pagespeed_no_defer=\"\" type=\"text/javascript\">",
      CriticalCssFilter::kAddStylesScript,
      "window['pagespeed'] = window['pagespeed'] || {};"
      "window['pagespeed']['criticalCss'] = {"
      "  'total_critical_inlined_size': 64,"
      "  'total_original_external_size': 6,"
      "  'total_overhead_size': 85,"
      "  'num_replaced_links': 3,"
      "  'num_unreplaced_links': 0"
      "};"
      "</script>");

  GoogleString a_url = "http://test.com/a.css";
  GoogleString b_url = "http://test.com/b.css";
  GoogleString c_url = "http://test.com/c.css";

  finder_->AddCriticalCss(a_url, "a_used {color: azure }", 1);
  finder_->AddCriticalCss(b_url, "b_used {color: blue }", 2);
  finder_->AddCriticalCss(c_url, "c_used {color: cyan }", 3);

  rewrite_driver()->set_flushed_early(true);
  GoogleString resource_html = StrCat(a_url, b_url, c_url);
  rewrite_driver()->flush_early_info()->set_resource_html(resource_html);
  options()->set_enable_flush_early_critical_css(false);

  ValidateExpected("inline_and_move", input_html, expected_html);

  ExpApplicationVector exp_application_counts;
  exp_application_counts.push_back(
      make_pair(RewriterApplication::APPLIED_OK, 3));
  ValidateRewriterLogging(RewriterHtmlApplication::ACTIVE,
                          exp_application_counts);

  // Validate logging.
  const CriticalCssInfo& info =
      rewrite_driver()->log_record()->logging_info()->critical_css_info();
  ASSERT_EQ(64, info.critical_inlined_bytes());
  ASSERT_EQ(6, info.original_external_bytes());
  ASSERT_EQ(85, info.overhead_bytes());
}

TEST_F(CriticalCssFilterTest, DoNothingUnderNoscript) {
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "<noscript>"
      "  <link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</noscript>"
      "</body>\n";

  GoogleString a_url = "http://test.com/a.css";
  GoogleString b_url = "http://test.com/b.css";
  GoogleString c_url = "http://test.com/c.css";

  finder_->AddCriticalCss(a_url, "a_used {color: azure }", 1);
  finder_->AddCriticalCss(b_url, "b_used {color: blue }", 2);
  finder_->AddCriticalCss(c_url, "c_used {color: cyan }", 3);

  rewrite_driver()->set_flushed_early(true);
  GoogleString resource_html = StrCat(a_url, b_url, c_url);
  rewrite_driver()->flush_early_info()->set_resource_html(resource_html);
  options()->set_enable_flush_early_critical_css(true);

  ValidateExpected("inline_and_move", input_html, input_html);
}

TEST_F(CriticalCssFilterTest, InlineAndAddStyleForFlushingEarly) {
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  static const char expected_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <style media=\"print\" data-pagespeed-flush-style=\"*\">"
      "a_used {color: azure }</style>"
      "<style data-pagespeed-flush-style=\"*\">b_used {color: blue }</style>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <style data-pagespeed-flush-style=\"*\">c_used {color: cyan }"
      "</style>\n</body>\n";

  finder_->AddCriticalCss("http://test.com/a.css", "a_used {color: azure }", 1);
  finder_->AddCriticalCss("http://test.com/b.css", "b_used {color: blue }", 2);
  finder_->AddCriticalCss("http://test.com/c.css", "c_used {color: cyan }", 3);

  rewrite_driver()->set_flushing_early(true);
  options()->set_enable_flush_early_critical_css(true);
  Parse("inline_and_flush_early_with_styleid", input_html);
  GoogleString full_html = doctype_string_ + AddHtmlBody(expected_html);
  EXPECT_TRUE(Wildcard(full_html).Match(output_buffer_)) <<
      "Expected:\n" << full_html << "\n\n Got:\n" << output_buffer_;
}

TEST_F(CriticalCssFilterTest, InlineFlushEarly) {
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  GoogleString a_url = "http://test.com/a.css";
  GoogleString b_url = "http://test.com/b.css";
  GoogleString c_url = "http://test.com/c.css";

  finder_->AddCriticalCss(a_url, "a_used {color: azure }", 1);
  finder_->AddCriticalCss(b_url, "b_used {color: blue }", 2);
  finder_->AddCriticalCss(c_url, "c_used {color: cyan }", 3);

  GoogleString a_styleId =
      rewrite_driver()->server_context()->hasher()->Hash(a_url);
  GoogleString b_styleId =
      rewrite_driver()->server_context()->hasher()->Hash(b_url);
  GoogleString c_styleId =
      rewrite_driver()->server_context()->hasher()->Hash(c_url);

  GoogleString expected_html = StrCat(
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <script id=\"psa_flush_style_early\" pagespeed_no_defer=\"\""
      " type=\"text/javascript\">",
      CriticalCssFilter::kApplyFlushEarlyCssTemplate,
      "</script>"
      "<script pagespeed_no_defer=\"\" type=\"text/javascript\">",
      StringPrintf(CriticalCssFilter::kInvokeFlushEarlyCssTemplate,
                   a_styleId.c_str(), "print"),
      "</script>"
      "<script pagespeed_no_defer=\"\" type=\"text/javascript\">",
      StringPrintf(CriticalCssFilter::kInvokeFlushEarlyCssTemplate,
                   b_styleId.c_str(), ""));

  StrAppend(&expected_html,
      "</script>"
      "\n  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <script pagespeed_no_defer=\"\" type=\"text/javascript\">",
      StringPrintf(CriticalCssFilter::kInvokeFlushEarlyCssTemplate,
                   c_styleId.c_str(), ""),
      "</script>"
      "\n</body>\n"
      "<noscript id=\"psa_add_styles\">"
      "<link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>"
      "<style type='text/css'>t {color: turquoise }</style>"
      "<link rel='stylesheet' href='c.css' type='text/css'>"
      "</noscript>"
      "<script pagespeed_no_defer=\"\" type=\"text/javascript\">",
      CriticalCssFilter::kAddStylesScript,
      "window['pagespeed'] = window['pagespeed'] || {};"
      "window['pagespeed']['criticalCss'] = {"
      "  'total_critical_inlined_size': 64,"
      "  'total_original_external_size': 6,"
      "  'total_overhead_size': 85,"
      "  'num_replaced_links': 3,"
      "  'num_unreplaced_links': 0"
      "};"
      "</script>");

  rewrite_driver()->set_flushed_early(true);
  GoogleString resource_html = StrCat(a_url, b_url, c_url);
  rewrite_driver()->flush_early_info()->set_resource_html(resource_html);
  options()->set_enable_flush_early_critical_css(true);

  ValidateExpected("inline_and_flush_early", input_html, expected_html);

  ExpApplicationVector exp_application_counts;
  exp_application_counts.push_back(
      make_pair(RewriterApplication::APPLIED_OK, 3));
  ValidateRewriterLogging(RewriterHtmlApplication::ACTIVE,
                          exp_application_counts);

  // Validate logging.
  const CriticalCssInfo& info =
      rewrite_driver()->log_record()->logging_info()->critical_css_info();
  ASSERT_EQ(64, info.critical_inlined_bytes());
  ASSERT_EQ(6, info.original_external_bytes());
  ASSERT_EQ(85, info.overhead_bytes());
}

TEST_F(CriticalCssFilterTest, InvalidUrl) {
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='Hi there!' type='text/css'>"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "  Hey!\n"
      "  <link rel='stylesheet' href='"
      "http://test.com/I.a.css+b.css.pagespeed.cc.0.css' type='text/css'>\n"
      "</body>\n";

  GoogleString expected_html = StrCat(
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='Hi there!' type='text/css'>"
      "  World!\n"
      "  <style>c_used {color: cyan }</style>\n"
      "  Hey!\n"
      "  <link rel='stylesheet' href='"
      "http://test.com/I.a.css+b.css.pagespeed.cc.0.css' type='text/css'>\n"
      "</body>\n"
      "<noscript id=\"psa_add_styles\">"
      "<link rel='stylesheet' href='Hi there!' type='text/css'>"
      "<link rel='stylesheet' href='c.css' type='text/css'>"
      "<link rel='stylesheet' href='"
      "http://test.com/I.a.css+b.css.pagespeed.cc.0.css' type='text/css'>"
      "</noscript>"
      "<script pagespeed_no_defer=\"\" type=\"text/javascript\">",
      CriticalCssFilter::kAddStylesScript,
      "window['pagespeed'] = window['pagespeed'] || {};"
      "window['pagespeed']['criticalCss'] = {"
      "  'total_critical_inlined_size': 21,"
      "  'total_original_external_size': 33,"
      "  'total_overhead_size': 21,"
      "  'num_replaced_links': 1,"
      "  'num_unreplaced_links': 2"
      "};"
      "</script>");

  finder_->AddCriticalCss("http://test.com/c.css", "c_used {color: cyan }", 33);

  ValidateExpected("invalid_url", input_html, expected_html);

  ExpApplicationVector exp_application_counts;
  exp_application_counts.push_back(
      make_pair(RewriterApplication::APPLIED_OK, 1));
  exp_application_counts.push_back(
      make_pair(RewriterApplication::PROPERTY_NOT_FOUND, 1));
  exp_application_counts.push_back(
      make_pair(RewriterApplication::INPUT_URL_INVALID, 1));
  ValidateRewriterLogging(RewriterHtmlApplication::ACTIVE,
                          exp_application_counts);

  // Validate logging.
  const CriticalCssInfo& info =
      rewrite_driver()->log_record()->logging_info()->critical_css_info();
  ASSERT_EQ(21, info.critical_inlined_bytes());
  ASSERT_EQ(33, info.original_external_bytes());
  ASSERT_EQ(21, info.overhead_bytes());
}

TEST_F(CriticalCssFilterTest, NullAndEmptyCriticalRules) {
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  GoogleString expected_html = StrCat(
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<style></style>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <style>c_used {color: cyan }</style>\n"
      "</body>\n"
      "<noscript id=\"psa_add_styles\">"
      "<link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>"
      "<style type='text/css'>t {color: turquoise }</style>"
      "<link rel='stylesheet' href='c.css' type='text/css'>"
      "</noscript>"
      "<script pagespeed_no_defer=\"\" type=\"text/javascript\">",
      CriticalCssFilter::kAddStylesScript,
      "window['pagespeed'] = window['pagespeed'] || {};"
      "window['pagespeed']['criticalCss'] = {"
      "  'total_critical_inlined_size': 21,"
      "  'total_original_external_size': 10,"
      "  'total_overhead_size': 42,"
      "  'num_replaced_links': 2,"
      "  'num_unreplaced_links': 1"
      "};"
      "</script>");

  // Skip adding a critical CSS for a.css.
  //     In the filtered html, the original link is left in place and
  //     a duplicate link is added to the full set of CSS at the bottom
  //     to make sure CSS rules are applied in the correct order.

  finder_->AddCriticalCss("http://test.com/b.css", "", 4);  // no critical rules
  finder_->AddCriticalCss("http://test.com/c.css", "c_used {color: cyan }", 6);

  ValidateExpected("null_and_empty_critical_rules", input_html, expected_html);

  ExpApplicationVector exp_application_counts;
  exp_application_counts.push_back(
      make_pair(RewriterApplication::APPLIED_OK, 2));
  exp_application_counts.push_back(
      make_pair(RewriterApplication::PROPERTY_NOT_FOUND, 1));
  ValidateRewriterLogging(RewriterHtmlApplication::ACTIVE,
                          exp_application_counts);

  // Validate logging.
  const CriticalCssInfo& info =
      rewrite_driver()->log_record()->logging_info()->critical_css_info();
  ASSERT_EQ(21, info.critical_inlined_bytes());
  ASSERT_EQ(10, info.original_external_bytes());
  ASSERT_EQ(42, info.overhead_bytes());
}

TEST_F(CriticalCssFilterTest, DebugFilterAddsStats) {
  options_->ForceEnableFilter(RewriteOptions::kDebug);
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  GoogleString expected_html = StrCat(
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<style></style>"
      "<!--Critical CSS applied:\n"
      "critical_size=0\n"
      "original_size=222\n"
      "original_src=http://test.com/b.css\n"
      "-->\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <style>c_used {color:cyan}</style>"
      "<!--Critical CSS applied:\n"
      "critical_size=19\n"
      "original_size=333\n"
      "original_src=http://test.com/c.css\n"
      "-->\n"
      "</body>\n"
      "<noscript id=\"psa_add_styles\">"
      "<link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>"
      "<style type='text/css'>t {color: turquoise }</style>"
      "<link rel='stylesheet' href='c.css' type='text/css'>"
      "</noscript>"
      "<script pagespeed_no_defer=\"\" type=\"text/javascript\">",
      CriticalCssFilter::kAddStylesScript,
      "window['pagespeed'] = window['pagespeed'] || {};"
      "window['pagespeed']['criticalCss'] = {"
      "  'total_critical_inlined_size': 19,"
      "  'total_original_external_size': 555,"
      "  'total_overhead_size': 40,"
      "  'num_replaced_links': 2,"
      "  'num_unreplaced_links': 1"
      "};"
      "</script>"
      "<!--Additional Critical CSS stats:\n"
      "  num_repeated_style_blocks=1\n"
      "  repeated_style_blocks_size=21\n"
      "\n"
      "From computing the critical CSS:\n"
      "  unhandled_import_count=8\n"
      "  unhandled_link_count=11\n"
      "  exception_count=5\n"
      "-->");

  // Skip adding a critical CSS for a.css.
  //     In the filtered html, the original link is left in place and
  //     a duplicate link is added to the full set of CSS at the bottom
  //     to make sure CSS rules are applied in the correct order.

  finder_->AddCriticalCss("http://test.com/b.css",
                          "", 222);  // no critical rules
  finder_->AddCriticalCss("http://test.com/c.css",
                          "c_used {color:cyan}", 333);
  finder_->SetCriticalCssStats(5, 8, 11);

  ValidateExpected("stats_flag_adds_stats", input_html, expected_html);

  ExpApplicationVector exp_application_counts;
  exp_application_counts.push_back(
      make_pair(RewriterApplication::APPLIED_OK, 2));
  exp_application_counts.push_back(
      make_pair(RewriterApplication::PROPERTY_NOT_FOUND, 1));
  ValidateRewriterLogging(RewriterHtmlApplication::ACTIVE,
                          exp_application_counts);

  // Validate logging.
  const CriticalCssInfo& info =
      rewrite_driver()->log_record()->logging_info()->critical_css_info();
  ASSERT_EQ(19, info.critical_inlined_bytes());
  ASSERT_EQ(555, info.original_external_bytes());
  ASSERT_EQ(40, info.overhead_bytes());
}

}  // namespace net_instaweb
