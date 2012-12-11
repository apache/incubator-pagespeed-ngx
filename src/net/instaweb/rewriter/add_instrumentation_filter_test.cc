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
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/escaping.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AddInstrumentationFilterTest : public RewriteTestBase {
 protected:
  AddInstrumentationFilterTest() {}

  virtual void SetUp() {
    options()->set_beacon_url("http://example.com/beacon?org=xxx&ets=");
    AddInstrumentationFilter::InitStats(statistics());
    options()->EnableFilter(RewriteOptions::kAddInstrumentation);
    RewriteTestBase::SetUp();
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
                                StringPiece headers_fetch_time,
                                StringPiece fetch_time,
                                StringPiece expt_id_param) {
    GoogleString url;
    EscapeToJsStringLiteral(rewrite_driver()->google_url().Spec(), false, &url);
    GoogleString str = "pagespeed.addInstrumentationInit(";
    StrAppend(&str, "'", beacon_url, "', ");
    StrAppend(&str, "'", event, "', ");
    StrAppend(&str, "'", headers_fetch_time, "', ");
    StrAppend(&str, "'", fetch_time, "', ");
    StrAppend(&str, "'", expt_id_param, "', ");
    StrAppend(&str, "'", url, "');");
    return str;
  }

  bool report_unload_time_;
  bool xhtml_mode_;
  bool cdata_mode_;
  bool https_mode_;
};

TEST_F(AddInstrumentationFilterTest, ScriptInjection) {
  RunInjection();
  EXPECT_TRUE(output_buffer_.find(
      CreateInitString(
          options()->beacon_url().http, "load",
          "", "", "")) !=
              GoogleString::npos);
}

TEST_F(AddInstrumentationFilterTest, ScriptInjectionWithNavigation) {
  report_unload_time_ = true;
  RunInjection();
  EXPECT_TRUE(output_buffer_.find(
      CreateInitString(
          options()->beacon_url().http, "beforeunload",
          "", "", "")) !=
              GoogleString::npos);
}

// Test an https fetch.
TEST_F(AddInstrumentationFilterTest,
       TestScriptInjectionWithHttps) {
  AssumeHttps();
  RunInjection();
  EXPECT_TRUE(output_buffer_.find(
      CreateInitString(
          options()->beacon_url().https, "load",
          "", "", "")) !=
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
          options()->beacon_url().https, "beforeunload",
          "", "", "")) !=
              GoogleString::npos);
}

// Test that experiment id reporting is done correctly.
TEST_F(AddInstrumentationFilterTest,
       TestFuriousExperimentIdReporting) {
  NullMessageHandler handler;
  options()->set_running_furious_experiment(true);
  options()->AddFuriousSpec("id=2;percent=10;slot=4;", &handler);
  options()->AddFuriousSpec("id=7;percent=10;level=CoreFilters;slot=4;",
                            &handler);
  options()->SetFuriousState(2);
  RunInjection();
  EXPECT_TRUE(output_buffer_.find(
      CreateInitString(
          options()->beacon_url().http, "load",
          "", "", "2")) !=
              GoogleString::npos);
}

// Test that headers fetch timing reporting is done correctly.
TEST_F(AddInstrumentationFilterTest, TestHeadersFetchTimingReporting) {
  NullMessageHandler handler;
  logging_info()->mutable_timing_info()->set_header_fetch_ms(200);
  logging_info()->mutable_timing_info()->set_fetch_ms(500);
  RunInjection();
  EXPECT_TRUE(output_buffer_.find(
      CreateInitString(
          options()->beacon_url().http, "load",
          "200", "500", "")) !=
              GoogleString::npos);
}


}  // namespace net_instaweb
