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

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class AddInstrumentationFilterTest : public ResourceManagerTestBase {
 protected:
  AddInstrumentationFilterTest() {}

  virtual void SetUp() {
    options()->set_beacon_url("http://example.com/beacon?ets=");
    AddInstrumentationFilter::Initialize(statistics());
    options()->EnableFilter(RewriteOptions::kAddInstrumentation);
    ResourceManagerTestBase::SetUp();
  }

  virtual bool AddBody() const { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(AddInstrumentationFilterTest);
};

TEST_F(AddInstrumentationFilterTest, TestScriptInjection) {
  rewrite_driver()->AddFilters();
  GoogleString expected_str = "<head>"
      "<script type='text/javascript'>"
      "window.mod_pagespeed_start = Number(new Date());"
      "</script>"
      "</head><body>"
      "<script type='text/javascript'>"
      "(function(){function g(){var ifr=0;"
      "if(window.parent != window){ifr=1}"
      "new Image().src="
      "'http://example.com/beacon?ets=load:'+"
      "(Number(new Date())-window.mod_pagespeed_start)+'&amp;ifr='+ifr+'"
      "&amp;url='+encodeURIComponent('";
  expected_str += HtmlParseTestBaseNoAlloc::kTestDomain;
  expected_str += "test_script_injection.html');"
      "window.mod_pagespeed_loaded=true;};"
      "var f=window.addEventListener;if(f){f('load',g,false);}"
      "else{"
      "f=window.attachEvent;if(f){f('onload',g);}}"
      "})();"
      "</script>"
      "</body>";

  ValidateExpected("test_script_injection",
                   "<head></head><body></body>",
                   expected_str);
  EXPECT_EQ(1, statistics()->GetVariable(
      AddInstrumentationFilter::kInstrumentationScriptAddedCount)->Get());
}

TEST_F(AddInstrumentationFilterTest, TestScriptInjectionWithNavigation) {
  options()->set_report_unload_time(true);
  rewrite_driver()->AddFilters();
  GoogleString expected_str = "<head>"
      "<script type='text/javascript'>"
      "window.mod_pagespeed_start = Number(new Date());"
      "</script>"
      "<script type='text/javascript'>"
      "(function(){function g(){"
      "if(window.mod_pagespeed_loaded) {return;}"
      "var ifr=0;if(window.parent != window){ifr=1}"
      "new Image().src="
      "'http://example.com/beacon?ets=unload:'+"
      "(Number(new Date())-window.mod_pagespeed_start)+'&amp;ifr='+ifr+'"
      "&amp;url='+encodeURIComponent('";
  expected_str += HtmlParseTestBaseNoAlloc::kTestDomain;
  expected_str += "test_script_injection.html');};"
      "var f=window.addEventListener;if(f){f('beforeunload',g,false);}"
      "else{"
      "f=window.attachEvent;if(f){f('onbeforeunload',g);}}"
      "})();</script>"
      "</head><body>"
      "<script type='text/javascript'>"
      "(function(){function g(){var ifr=0;"
      "if(window.parent != window){ifr=1}"
      "new Image().src="
      "'http://example.com/beacon?ets=load:'+"
      "(Number(new Date())-window.mod_pagespeed_start)+'&amp;ifr='+ifr+'"
      "&amp;url='+encodeURIComponent('";
  expected_str += HtmlParseTestBaseNoAlloc::kTestDomain;
  expected_str += "test_script_injection.html');"
      "window.mod_pagespeed_loaded=true;};"
      "var f=window.addEventListener;if(f){f('load',g,false);}"
      "else{"
      "f=window.attachEvent;if(f){f('onload',g);}}"
      "})();</script>"
      "</body>";

  ValidateExpected("test_script_injection",
                   "<head></head><body></body>",
                   expected_str);
  EXPECT_EQ(1, statistics()->GetVariable(
      AddInstrumentationFilter::kInstrumentationScriptAddedCount)->Get());
}

}  // namespace net_instaweb
