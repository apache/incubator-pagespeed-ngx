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

#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class AddInstrumentationFilterTest : public HtmlParseTestBase {
 protected:
  AddInstrumentationFilterTest()
      : add_instrumentation_filter_(&html_parse_,
                                    "http://example.com/beacon?ets=") {
    html_parse_.AddFilter(&add_instrumentation_filter_);
  }

  virtual bool AddBody() const { return false; }

 private:
  AddInstrumentationFilter add_instrumentation_filter_;

  DISALLOW_COPY_AND_ASSIGN(AddInstrumentationFilterTest);
};

TEST_F(AddInstrumentationFilterTest, TestScriptInjection) {
  GoogleString expected_str = "<head>"
      "<script type='text/javascript'>"
      "window.mod_pagespeed_start = Number(new Date());"
      "</script>"
      "</head><body>"
      "<script type='text/javascript'>"
      "function g(){new Image().src="
      "'http://example.com/beacon?ets=load:'+"
      "(Number(new Date())-window.mod_pagespeed_start)+'&url='+"
      "encodeURIComponent('";
  expected_str += HtmlParseTestBaseNoAlloc::kTestDomain;
  expected_str += "test_script_injection.html');};"
      "var f=window.addEventListener;if(f){f('load',g,false);}"
      "else{"
      "f=window.attachEvent;if(f){f('onload',g);}}"
      "</script>"
      "</body>";

  ValidateExpected("test_script_injection",
                   "<head></head><body></body>",
                   expected_str);
}
}  // namespace net_instaweb
