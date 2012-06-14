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
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AddInstrumentationFilterTest : public ResourceManagerTestBase {
 protected:
  AddInstrumentationFilterTest() {}

  virtual void SetUp() {
    options()->set_beacon_url("http://example.com/beacon?org=xxx&ets=");
    AddInstrumentationFilter::Initialize(statistics());
    options()->EnableFilter(RewriteOptions::kAddInstrumentation);
    ResourceManagerTestBase::SetUp();
    report_unload_time_ = false;
    xhtml_mode_ = false;
    cdata_mode_ = false;
    https_mode_ = false;
  }

  virtual bool AddBody() const { return false; }

  void RunInjection() {
    options()->set_report_unload_time(report_unload_time_);
    rewrite_driver()->AddFilters();
    GoogleString url =
        StrCat((https_mode_ ? "https://example.com/" : kTestDomain),
               "index.html?a&b");
    ParseUrl(url, "<head></head><body></body>");
    EXPECT_EQ(https_mode_,
              output_buffer_.find("https://example.com/beacon?") !=
              GoogleString::npos);
    EXPECT_EQ(https_mode_,
              output_buffer_.find("http://example.com/beacon?") ==
              GoogleString::npos);
    EXPECT_TRUE(output_buffer_.find("ets=load") != GoogleString::npos);
    EXPECT_EQ(report_unload_time_,
              output_buffer_.find("ets=unload") != GoogleString::npos);

    // All of the ampersands should be suffixed with "amp;" if that's what
    // we are looking for.
    int amp_count = 0;
    for (int i = 0, n = output_buffer_.size(); i < n; ++i) {
      if (output_buffer_[i] == '&') {
        ++amp_count;
        EXPECT_EQ(xhtml_mode_, output_buffer_.substr(i, 5) == "&amp;");
      }
    }
    EXPECT_LT(0, amp_count);
    EXPECT_EQ(cdata_mode_,
              output_buffer_.find("//<![CDATA[\n") != GoogleString::npos);
    EXPECT_EQ(cdata_mode_,
              output_buffer_.find("\n//]]>") != GoogleString::npos);
    EXPECT_EQ(1, statistics()->GetVariable(
        AddInstrumentationFilter::kInstrumentationScriptAddedCount)->Get());
  }

  void SetMimetypeToXhtml() {
    SetXhtmlMimetype();
    xhtml_mode_ = !cdata_mode_;
  }

  void DoNotRelyOnContentType() {
    cdata_mode_ = true;
    resource_manager()->set_response_headers_finalized(false);
  }

  void AssumeHttps() {
    https_mode_ = true;
  }

  bool report_unload_time_;
  bool xhtml_mode_;
  bool cdata_mode_;
  bool https_mode_;
};

TEST_F(AddInstrumentationFilterTest, ScriptInjection) {
  RunInjection();
}

TEST_F(AddInstrumentationFilterTest, ScriptInjectionWithNavigation) {
  report_unload_time_ = true;
  RunInjection();
}

// Note that the DOCTYPE is not signficiant in terms of how the browser
// interprets ampersands in script tags, so we test here that we do not
// expect &amp;.
TEST_F(AddInstrumentationFilterTest, ScriptInjectionXhtmlDoctype) {
  SetDoctype(kXhtmlDtd);
  RunInjection();
}

// Same story here: the doctype is ignored and we do not get "&amp;".
TEST_F(AddInstrumentationFilterTest,
       TestScriptInjectionWithNavigationXhtmlDoctype) {
  SetDoctype(kXhtmlDtd);
  report_unload_time_ = true;
  RunInjection();
}

// With Mimetype, we expect "&amp;".
TEST_F(AddInstrumentationFilterTest, ScriptInjectionXhtmlMimetype) {
  SetMimetypeToXhtml();
  RunInjection();
}

// With Mimetype, we expect "&amp;".
TEST_F(AddInstrumentationFilterTest,
       TestScriptInjectionWithNavigationXhtmlMimetype) {
  SetMimetypeToXhtml();
  report_unload_time_ = true;
  RunInjection();
}

// In mod_pagespeed, we cannot currently rely on the content-type
// being set properly prior to running our output filter.
TEST_F(AddInstrumentationFilterTest, ScriptInjectionCdata) {
  DoNotRelyOnContentType();
  RunInjection();
}

TEST_F(AddInstrumentationFilterTest, ScriptInjectionWithNavigationCdata) {
  DoNotRelyOnContentType();
  report_unload_time_ = true;
  RunInjection();
}

// In mod_pagespeed, we cannot currently rely on the content-type
// being set properly prior to running our output filter.
TEST_F(AddInstrumentationFilterTest, ScriptInjectionCdataMime) {
  DoNotRelyOnContentType();
  SetMimetypeToXhtml();
  RunInjection();
}

TEST_F(AddInstrumentationFilterTest,
       TestScriptInjectionWithNavigationCdataMime) {
  DoNotRelyOnContentType();
  SetMimetypeToXhtml();
  report_unload_time_ = true;
  RunInjection();
}

// Test an https fetch.
TEST_F(AddInstrumentationFilterTest,
       TestScriptInjectionWithHttps) {
  AssumeHttps();
  RunInjection();
}

// Test an https fetch, reporting unload and using Xhtml
TEST_F(AddInstrumentationFilterTest,
       TestScriptInjectionWithHttpsUnloadAndXhtml) {
  SetMimetypeToXhtml();
  AssumeHttps();
  report_unload_time_ = true;
  RunInjection();
}

}  // namespace net_instaweb
