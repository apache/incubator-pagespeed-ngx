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

#include "pagespeed/kernel/html/html_parse_test_base.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_parse.h"


namespace net_instaweb {

const char HtmlParseTestBaseNoAlloc::kTestDomain[] = "http://test.com/";
const char HtmlParseTestBaseNoAlloc::kXhtmlDtd[] =
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" "
    "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">";

HtmlParseTestBaseNoAlloc::~HtmlParseTestBaseNoAlloc() {
}

void HtmlParseTestBaseNoAlloc::ParseUrl(StringPiece url,
                                        StringPiece html_input) {
  // We don't add the filter in the constructor because it needs to be the
  // last filter added.
  SetupWriter();
  html_parse()->StartParse(url);
  html_parse()->ParseText(doctype_string_ + AddHtmlBody(html_input));
  html_parse()->FinishParse();
}

bool HtmlParseTestBaseNoAlloc::ValidateExpected(StringPiece case_id,
                                                StringPiece html_input,
                                                StringPiece expected) {
  Parse(case_id, html_input);
  GoogleString xbody = doctype_string_ + AddHtmlBody(expected);
  EXPECT_EQ(xbody, output_buffer_) << "Test id:" << case_id;
  bool success = (xbody == output_buffer_);
  output_buffer_.clear();
  return success;
}

bool HtmlParseTestBaseNoAlloc::ValidateExpectedUrl(
    StringPiece url, StringPiece html_input, StringPiece expected) {
  ParseUrl(url, html_input);
  GoogleString xbody = doctype_string_ + AddHtmlBody(expected);
  EXPECT_EQ(xbody, output_buffer_) << "Test url:" << url;
  bool success = (xbody == output_buffer_);
  output_buffer_.clear();
  return success;
}

void HtmlParseTestBaseNoAlloc::ValidateExpectedFail(
    StringPiece case_id, StringPiece html_input, StringPiece expected) {
  Parse(case_id, html_input);
  GoogleString xbody = AddHtmlBody(expected);
  EXPECT_NE(xbody, output_buffer_) << "Test id:" << case_id;
  output_buffer_.clear();
}

}  // namespace net_instaweb
