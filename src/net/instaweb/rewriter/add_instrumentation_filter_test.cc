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
#include "net/instaweb/http/public/meta_data.h"  // for HttpAttributes, etc
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/statistics.h"

namespace net_instaweb {

class AddInstrumentationFilterTest : public ResourceManagerTestBase {
 protected:
  AddInstrumentationFilterTest() {}

  virtual void SetUp() {
    options()->set_beacon_url("http://example.com/beacon?org=xxx&ets=");
    AddInstrumentationFilter::Initialize(statistics());
    options()->EnableFilter(RewriteOptions::kAddInstrumentation);
    ResourceManagerTestBase::SetUp();
  }

  virtual bool AddBody() const { return false; }

  void RunInjection(bool report_unload_time, bool expect_escaped_ampersand) {
    options()->set_report_unload_time(report_unload_time);
    rewrite_driver()->AddFilters();
    Parse("test_script_injection", "<head></head><body></body>");
    EXPECT_TRUE(output_buffer_.find("/beacon?") != GoogleString::npos);
    EXPECT_TRUE(output_buffer_.find("ets=load") != GoogleString::npos);
    EXPECT_EQ(report_unload_time,
              output_buffer_.find("ets=unload") != GoogleString::npos);

    // All of the ampersands should be suffixed with "amp;" if that's what
    // we are looking for.
    for (int i = 0, n = output_buffer_.size(); i < n; ++i) {
      if (output_buffer_[i] == '&') {
        EXPECT_EQ(expect_escaped_ampersand,
                  output_buffer_.substr(i, 5) == "&amp;");
      }
    }

    EXPECT_TRUE(output_buffer_.find("&") != GoogleString::npos);
    EXPECT_EQ(expect_escaped_ampersand,
              output_buffer_.find("&amp;") != GoogleString::npos);
    EXPECT_EQ(1, statistics()->GetVariable(
        AddInstrumentationFilter::kInstrumentationScriptAddedCount)->Get());
  }

  void SetMimetypeToXhtml() {
    rewrite_driver()->set_response_headers_ptr(&response_headers_);
    response_headers_.Add(HttpAttributes::kContentType,
                          "application/xhtml+xml");
    response_headers_.ComputeCaching();
  }

 private:
  ResponseHeaders response_headers_;
};

TEST_F(AddInstrumentationFilterTest, TestScriptInjection) {
  RunInjection(false, false);
}

TEST_F(AddInstrumentationFilterTest, TestScriptInjectionWithNavigation) {
  RunInjection(true, false);
}

// Note that the DOCTYPE is not signficiant in terms of how the browser
// interprets ampersands in script tags, so we test here that we do not
// expect &amp;.
TEST_F(AddInstrumentationFilterTest, TestScriptInjectionXhtmlDoctype) {
  SetDoctype(kXhtmlDtd);
  RunInjection(false, false);
}

// Same story here: the doctype is ignored and we do not get "&amp;".
TEST_F(AddInstrumentationFilterTest,
       TestScriptInjectionWithNavigationXhtmlDoctype) {
  SetDoctype(kXhtmlDtd);
  RunInjection(true, false);
}

// With Mimetype, we expect "&amp;".
TEST_F(AddInstrumentationFilterTest, TestScriptInjectionXhtmlMimetype) {
  SetMimetypeToXhtml();
  RunInjection(false, true);
}

// With Mimetype, we expect "&amp;".
TEST_F(AddInstrumentationFilterTest,
       TestScriptInjectionWithNavigationXhtmlMimetype) {
  SetMimetypeToXhtml();
  RunInjection(true, true);
}

}  // namespace net_instaweb
