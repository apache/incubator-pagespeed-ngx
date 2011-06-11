/*
 * Copyright 2010 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"


namespace net_instaweb {

const char HtmlParseTestBaseNoAlloc::kTestDomain[] = "http://test.com/";

HtmlParseTestBaseNoAlloc::~HtmlParseTestBaseNoAlloc() {
}

void HtmlParseTestBaseNoAlloc::ParseUrl(const StringPiece& url,
                                        const StringPiece& html_input) {
  // We don't add the filter in the constructor because it needs to be the
  // last filter added.
  SetupWriter();
  html_parse()->StartParse(url);
  html_parse()->ParseText(doctype_string_ + AddHtmlBody(html_input));
  html_parse()->FinishParse();
}

void HtmlParseTestBaseNoAlloc::ValidateExpected(const StringPiece& case_id,
                                                const GoogleString& html_input,
                                                const GoogleString& expected) {
  Parse(case_id, html_input);
  GoogleString xbody = doctype_string_ + AddHtmlBody(expected);
  EXPECT_EQ(xbody, output_buffer_);
  output_buffer_.clear();
}

void HtmlParseTestBaseNoAlloc::ValidateExpectedUrl(
    const StringPiece& url,
    const GoogleString& html_input,
    const GoogleString& expected) {
  ParseUrl(url, html_input);
  GoogleString xbody = doctype_string_ + AddHtmlBody(expected);
  EXPECT_EQ(xbody, output_buffer_);
  output_buffer_.clear();
}

void HtmlParseTestBaseNoAlloc::ValidateExpectedFail(
    const StringPiece& case_id,
    const GoogleString& html_input,
    const GoogleString& expected) {
  Parse(case_id, html_input);
  GoogleString xbody = AddHtmlBody(expected);
  EXPECT_NE(xbody, output_buffer_);
  output_buffer_.clear();
}

}  // namespace net_instaweb
