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

// Author: satyanarayana@google.com (Satyanarayana Manyam)

#include "net/instaweb/rewriter/public/add_instrumentation_filter.h"

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/escaping.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/html/html_keywords.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

class AddInstrumentationFilterTest : public RewriteTestBase {
 protected:
  AddInstrumentationFilterTest() {}

  virtual void SetUp() {
    options()->set_beacon_url("http://example.com/beacon?org=xxx");
    AddInstrumentationFilter::InitStats(statistics());
    options()->EnableFilter(RewriteOptions::kAddInstrumentation);
    RewriteTestBase::SetUp();
    rewrite_driver()->SetUserAgent(
        UserAgentMatcherTestBase::kChrome18UserAgent);
    report_unload_time_ = false;
    xhtml_mode_ = false;
    cdata_mode_ = false;
    https_mode_ = false;
  }

  virtual bool AddBody() const { return false; }

  void RunInjection() {
    options()->set_report_unload_time(report_unload_time_);
    rewrite_driver()->AddFilters();
    ParseUrl(GetTestUrl(),
             "<head></head><head></head><body></body><body></body>");
    EXPECT_EQ(1, statistics()->GetVariable(
        AddInstrumentationFilter::kInstrumentationScriptAddedCount)->Get());
  }

  void SetMimetypeToXhtml() {
    SetXhtmlMimetype();
    xhtml_mode_ = !cdata_mode_;
  }

  void DoNotRelyOnContentType() {
    cdata_mode_ = true;
    server_context()->set_response_headers_finalized(false);
  }

  void AssumeHttps() {
    https_mode_ = true;
  }

  GoogleString GetTestUrl() {
    return StrCat((https_mode_ ? "https://example.com/" : kTestDomain),
                  "index.html?a&b");
  }

  GoogleString CreateInitString(StringPiece beacon_url,
                                StringPiece event,
                                StringPiece extra_params) {
    GoogleString url;
    EscapeToJsStringLiteral(rewrite_driver()->google_url().Spec(), false, &url);
    GoogleString str = "pagespeed.addInstrumentationInit(";
    StrAppend(&str, "'", beacon_url, "', ");
    StrAppend(&str, "'", event, "', ");
    StrAppend(&str, "'", extra_params, "', ");
    StrAppend(&str, "'", url, "');");
    return str;
  }

  bool report_unload_time_;
  bool xhtml_mode_;
  bool cdata_mode_;
  bool https_mode_;
  ResponseHeaders response_headers_;
};

TEST_F(AddInstrumentationFilterTest, ScriptInjection) {
  RunInjection();
  EXPECT_TRUE(output_buffer_.find(
      CreateInitString(
          options()->beacon_url().http, "load", "")) !=
              GoogleString::npos);
}

TEST_F(AddInstrumentationFilterTest, ScriptInjectionWithNavigation) {
  report_unload_time_ = true;
  RunInjection();
  EXPECT_TRUE(output_buffer_.find(
      CreateInitString(
          options()->beacon_url().http, "beforeunload", "")) !=
              GoogleString::npos);
}

// Test an https fetch.
TEST_F(AddInstrumentationFilterTest, TestScriptInjectionWithHttps) {
  AssumeHttps();
  RunInjection();
  EXPECT_TRUE(output_buffer_.find(
      CreateInitString(
          options()->beacon_url().https, "load", "")) !=
              GoogleString::npos);
}

// Test an https fetch, reporting unload and using Xhtml
TEST_F(AddInstrumentationFilterTest,
       TestScriptInjectionWithHttpsUnloadAndXhtml) {
  SetMimetypeToXhtml();
  AssumeHttps();
  report_unload_time_ = true;
  RunInjection();
  EXPECT_TRUE(output_buffer_.find(
      CreateInitString(
          options()->beacon_url().https, "beforeunload", "")) !=
              GoogleString::npos);
}

// Test that experiment id reporting is done correctly.
TEST_F(AddInstrumentationFilterTest, TestExperimentIdReporting) {
  NullMessageHandler handler;
  options()->set_running_experiment(true);
  options()->AddExperimentSpec("id=2;percent=10;slot=4;", &handler);
  options()->AddExperimentSpec("id=7;percent=10;level=CoreFilters;slot=4;",
                               &handler);
  options()->SetExperimentState(2);
  RunInjection();
  EXPECT_TRUE(output_buffer_.find(
      CreateInitString(
          options()->beacon_url().http, "load", "&exptid=2")) !=
              GoogleString::npos);
}

// Test that extended instrumentation is injected properly.
TEST_F(AddInstrumentationFilterTest, TestExtendedInstrumentation) {
  options()->set_enable_extended_instrumentation(true);
  RunInjection();
  EXPECT_TRUE(output_buffer_.find(
      CreateInitString(
          options()->beacon_url().http, "load", "")) !=
              GoogleString::npos);
  EXPECT_TRUE(output_buffer_.find("getResourceTimingData=function()") !=
              GoogleString::npos);
}

// Test that headers fetch timing reporting is done correctly.
TEST_F(AddInstrumentationFilterTest, TestHeadersFetchTimingReporting) {
  RequestContext::TimingInfo* timing_info = mutable_timing_info();
  timing_info->FetchStarted();
  AdvanceTimeMs(200);
  timing_info->FetchHeaderReceived();
  AdvanceTimeMs(100);
  timing_info->FirstByteReturned();
  AdvanceTimeMs(200);
  timing_info->FetchFinished();
  RunInjection();
  EXPECT_TRUE(output_buffer_.find(
      CreateInitString(
          options()->beacon_url().http, "load", "&hft=200&ft=500&s_ttfb=300"))
              != GoogleString::npos) << output_buffer_;
}


// Test that head script is inserted after title and meta tags.
TEST_F(AddInstrumentationFilterTest, TestScriptAfterTitleAndMeta) {
  rewrite_driver()->AddFilters();
  ParseUrl(GetTestUrl(),
           "<head><meta name='abc' /><title></title></head><body></body>");
  EXPECT_TRUE(output_buffer_.find(
      "<head><meta name='abc' /><title></title><script"));
}

TEST_F(AddInstrumentationFilterTest, TestNon200Response) {
  rewrite_driver()->AddFilters();
  response_headers_.set_status_code(HttpStatus::kForbidden);
  rewrite_driver()->set_response_headers_ptr(&response_headers_);
  ParseUrl(GetTestUrl(),
           "<head></head><head></head><body></body><body></body>");
  EXPECT_EQ(1, statistics()->GetVariable(
      AddInstrumentationFilter::kInstrumentationScriptAddedCount)->Get());
  EXPECT_TRUE(output_buffer_.find(
      CreateInitString(
          options()->beacon_url().http, "load", "&rc=403")) !=
              GoogleString::npos);
}

TEST_F(AddInstrumentationFilterTest, TestRequestId) {
  rewrite_driver()->request_context()->set_request_id(1234567890L);
  RunInjection();
  EXPECT_TRUE(output_buffer_.find(
      CreateInitString(options()->beacon_url().http, "load",
                       "&id=1234567890")) != GoogleString::npos);
}

TEST_F(AddInstrumentationFilterTest, TestNoDeferInstrumentationScript) {
  RunInjection();
  EXPECT_TRUE(output_buffer_.find(
      CreateInitString(
          options()->beacon_url().http, "load", "")) !=
              GoogleString::npos);
  const StringPiece* nodefer =
      HtmlKeywords::KeywordToString(HtmlName::kPagespeedNoDefer);
  EXPECT_TRUE(output_buffer_.find(nodefer->as_string()) != GoogleString::npos);
}

TEST_F(AddInstrumentationFilterTest, TestDeferInstrumentationScript) {
  rewrite_driver()->set_defer_instrumentation_script(true);
  RunInjection();
  EXPECT_TRUE(output_buffer_.find(
      CreateInitString(
          options()->beacon_url().http, "load", "")) !=
              GoogleString::npos);
  const StringPiece* nodefer =
      HtmlKeywords::KeywordToString(HtmlName::kPagespeedNoDefer);
  EXPECT_TRUE(output_buffer_.find(nodefer->as_string()) == GoogleString::npos);
}

TEST_F(AddInstrumentationFilterTest, TestDisableForBots) {
  rewrite_driver()->AddFilters();
  rewrite_driver()->SetUserAgent(UserAgentMatcherTestBase::kGooglebotUserAgent);
  ValidateNoChanges(GetTestUrl(),
                    "<head></head><head></head><body></body><body></body>");
}

}  // namespace net_instaweb
