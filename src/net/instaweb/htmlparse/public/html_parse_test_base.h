/**
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

// Infrastructure for testing html parsing and rewriting.

#ifndef NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_PARSE_TEST_BASE_H_
#define NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_PARSE_TEST_BASE_H_

#include <string>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

class HtmlParseTestBaseNoAlloc : public testing::Test {
 protected:
  HtmlParseTestBaseNoAlloc()
      : write_to_string_(&output_buffer_),
        added_filter_(false) {
  }

  virtual void TearDown() {
    output_buffer_.clear();
  }

  // To make the tests more concise, we generally omit the <html>...</html>
  // tags bracketing the input.  The libxml parser will add those in
  // if we don't have them.  To avoid having that make the test data more
  // verbose, we automatically add them in the test infrastructure, both
  // for stimulus and expected response.
  //
  // This flag controls whether we also add <body>...</body> tags.  In
  // the case html_parse_test, we go ahead and add them in.  In the
  // case of the rewriter tests, we want to explicitly control/observe
  // the head and the body so we don't add the body tags in
  // automatically.  So classes that derive from HtmlParseTestBase must
  // override this variable to indicate which they prefer.
  virtual bool AddBody() const = 0;

  std::string AddHtmlBody(const std::string& html) {
    std::string ret = AddBody() ? "<html><body>\n" : "<html>\n";
    ret += html + (AddBody() ? "\n</body></html>\n" : "\n</html>");
    return ret;
  }

  // Check that the output HTML is serialized to string-compare
  // precisely with the input.
  void ValidateNoChanges(const StringPiece& case_id,
                         const std::string& html_input) {
    ValidateExpected(case_id, html_input, html_input);
  }

  // Fail to ValidateNoChanges.
  void ValidateNoChangesFail(const StringPiece& case_id,
                             const std::string& html_input) {
    ValidateExpectedFail(case_id, html_input, html_input);
  }

  void SetupWriter() {
    output_buffer_.clear();
    if (html_writer_filter_.get() == NULL) {
      html_writer_filter_.reset(new HtmlWriterFilter(html_parse()));
      html_writer_filter_->set_writer(&write_to_string_);
      html_parse()->AddFilter(html_writer_filter_.get());
    }
  }

  // Parse html_input, the result is stored in output_buffer_.
  void Parse(const StringPiece& case_id, const std::string& html_input) {
    // HtmlParser needs a valid HTTP URL to evaluate relative paths,
    // so we create a dummy URL.
    std::string dummy_url = StrCat("http://test.com/", case_id, ".html");
    ParseUrl(dummy_url, html_input);
  }

  // Parse given an explicit URL rather than an id to build URL around.
  void ParseUrl(const StringPiece& url, const std::string& html_input) {
    // We don't add the filter in the constructor because it needs to be the
    // last filter added.
    SetupWriter();
    html_parse()->StartParse(url);
    std::string html_body = AddHtmlBody(html_input);
    html_parse()->ParseText(html_body);
    html_parse()->FinishParse();
  }

  // Validate that the output HTML serializes as specified in
  // 'expected', which might not be identical to the input.
  void ValidateExpected(const StringPiece& case_id,
                        const std::string& html_input,
                        const std::string& expected) {
    Parse(case_id, html_input);
    std::string xbody = AddHtmlBody(expected);
    EXPECT_EQ(xbody, output_buffer_);
    output_buffer_.clear();
  }

  // Fail to ValidateExpected.
  void ValidateExpectedFail(const StringPiece& case_id,
                            const std::string& html_input,
      const std::string& expected) {
    Parse(case_id, html_input);
    std::string xbody = AddHtmlBody(expected);
    EXPECT_NE(xbody, output_buffer_);
    output_buffer_.clear();
  }

  virtual HtmlParse* html_parse() = 0;

  GoogleMessageHandler message_handler_;
  StringWriter write_to_string_;
  std::string output_buffer_;
  bool added_filter_;
  scoped_ptr<HtmlWriterFilter> html_writer_filter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HtmlParseTestBaseNoAlloc);
};

class HtmlParseTestBase : public HtmlParseTestBaseNoAlloc {
 public:
  HtmlParseTestBase() : html_parse_(&message_handler_) {
  };
 protected:
  virtual HtmlParse* html_parse() { return &html_parse_; }

  HtmlParse html_parse_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HtmlParseTestBase);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_PARSE_TEST_BASE_H_
