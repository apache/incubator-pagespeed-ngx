/*
 * Copyright 2012 Google Inc.
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
// Author: nikhilmadan@google.com (Nikhil Madan)

#include "net/instaweb/rewriter/public/redirect_on_size_limit_filter.h"

#include <algorithm>

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

namespace {

static const char kScript[] = "<script type=\"text/javascript\">"
    "window.location=\"http://test.com/in.html?ModPagespeed=off\";"
    "</script>";

}  // namespace

class RedirectOnSizeLimitFilterTest : public RewriteTestBase {
 public:
  RedirectOnSizeLimitFilterTest() : writer_(&output_) {}

 protected:
  virtual void SetUp() {}

  void SetupDriver(int size_limit) {
    options_->set_max_html_parse_bytes(size_limit);
    options_->EnableFilter(RewriteOptions::kHtmlWriterFilter);
    RewriteTestBase::SetUp();
    rewrite_driver()->AddFilters();
    rewrite_driver()->SetWriter(&writer_);
  }

  void ClearAndResetDriver(int size_limit) {
    delete rewrite_driver();
    output_.clear();
    options_ = new RewriteOptions();
    SetupDriver(size_limit);
  }

  void CheckOutput(int start_index, int end_index,
                   bool should_flush_before_size,
                   const GoogleString& input,
                   const GoogleString& expected_output) {
    for (int i = start_index; i < end_index; ++i) {
      ClearAndResetDriver(i);
      if (should_flush_before_size && i > 2) {
        html_parse()->StartParse("http://test.com/in.html");
        int split = std::min(static_cast<int>(input.size()) - 1, i);
        html_parse()->ParseText(input.substr(0, split));
        html_parse()->Flush();
        html_parse()->ParseText(input.substr(split, input.size() - 1));
        html_parse()->FinishParse();
      } else {
        Parse("in", input);
      }
      EXPECT_EQ(expected_output, output_);
    }
  }

  virtual bool AddHtmlTags() const { return false; }
  virtual bool AddBody() const { return false; }

  GoogleString output_;

 private:
  StringWriter writer_;

  DISALLOW_COPY_AND_ASSIGN(RedirectOnSizeLimitFilterTest);
};

TEST_F(RedirectOnSizeLimitFilterTest, TestOneFlushWindow) {
  static const char input[] =
      "<html>"  // 6 chars
      "<input type=\"text\"/>"  // 20 chars
      "<script type=\"text/javascript\">alert('123');</script>"  // 53 chars
      "<!--[if IE]>...<![endif]-->"  // 27 chars
      "<table><tr><td>blah</td></tr></table>"  // 37 chars
      "</html>";  // 7 chars

  SetupDriver(-1);
  Parse("in", input);
  EXPECT_EQ(input, output_);

  CheckOutput(0, 1, false, input, input);

  CheckOutput(1, 149, false, input,  StringPrintf("<html>%s</html>", kScript));

  CheckOutput(150, 180, false, input, input);
}

TEST_F(RedirectOnSizeLimitFilterTest, TestFlushBeforeLimit) {
  const char input[] =
      "<html>"  // 6 chars
      "<input type=\"text\"/>"  // 20 chars
      "<script type=\"text/javascript\">alert('123');</script>"  // 53 chars
      "<!--[if IE]>...<![endif]-->"  // 27 chars
      "<table><tr><td>blah</td></tr></table>"  // 37 chars
      "</html>";  // 7 chars

  SetupDriver(-1);
  Parse("in", input);
  EXPECT_EQ(input, output_);

  CheckOutput(0, 1, true, input, input);

  CheckOutput(1, 6, true, input,  StringPrintf("<html>%s</html>", kScript));

  CheckOutput(6, 26, true, input,
              StringPrintf("<html>%s<input type=\"text\"/></html>",  kScript));

  CheckOutput(26, 57, true, input,
      StringPrintf("<html><input type=\"text\"/>%s"
                   "<script type=\"text/javascript\"></script></html>",
                   kScript));

  CheckOutput(57, 79, true, input,
      StringPrintf("<html><input type=\"text\"/>"
                   "<script type=\"text/javascript\">alert('123');</script>%s"
                   "</html>", kScript));

  CheckOutput(79, 113, true, input,
      StringPrintf("<html><input type=\"text\"/>"
                   "<script type=\"text/javascript\">alert('123');</script>"
                   "<!--[if IE]>...<![endif]-->%s<table></table></html>",
                   kScript));

  CheckOutput(113, 117, true, input,
      StringPrintf("<html><input type=\"text\"/>"
      "<script type=\"text/javascript\">alert('123');</script>"
      "<!--[if IE]>...<![endif]-->"
      "<table>%s<tr></tr></table></html>", kScript));

  CheckOutput(117, 121, true, input,
      StringPrintf("<html><input type=\"text\"/>"
                   "<script type=\"text/javascript\">alert('123');</script>"
                   "<!--[if IE]>...<![endif]-->"
                   "<table><tr>%s<td></td></tr></table></html>", kScript));

  CheckOutput(121, 130, true, input,
      StringPrintf("<html><input type=\"text\"/>"
                   "<script type=\"text/javascript\">alert('123');</script>"
                   "<!--[if IE]>...<![endif]-->"
                   "<table><tr><td>blah</td>%s</tr></table></html>", kScript));

  CheckOutput(130, 135, true, input,
      StringPrintf("<html><input type=\"text\"/>"
                   "<script type=\"text/javascript\">alert('123');</script>"
                   "<!--[if IE]>...<![endif]-->"
                   "<table><tr><td>blah</td></tr>%s</table></html>", kScript));

  CheckOutput(135, 150, true, input,
      StringPrintf("<html><input type=\"text\"/>"
                   "<script type=\"text/javascript\">alert('123');</script>"
                   "<!--[if IE]>...<![endif]-->"
                   "<table><tr><td>blah</td></tr></table>%s</html>", kScript));

  CheckOutput(150, 160, true, input, input);
}

TEST_F(RedirectOnSizeLimitFilterTest, TestEscapingAndFlush) {
  SetupDriver(100);
  GoogleString output = StringPrintf(
      "<html>"
      "<input type=\"text\"/>"
      "<script type=\"text/javascript\">alert('123');</script>"
      "<script type=\"text/javascript\">"
      "window.location=\"http://test.com/in.html?\\'(&ModPagespeed=off\";"
      "</script></html>");

  html_parse()->StartParse("http://test.com/in.html?'(");
  html_parse()->ParseText(
      "<html><input type=\"text\"/>"
      "<script type=\"text/javascript\">");
  html_parse()->Flush();
  html_parse()->ParseText(
      "alert('123');</script>"
      "<!--[if IE]>...<![endif]-->"
      "<table><tr><td>blah</td></tr></table></html>");
  html_parse()->FinishParse();

  EXPECT_EQ(output, output_);
}

}  // namespace net_instaweb
