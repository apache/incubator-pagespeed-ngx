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

// Infrastructure for testing html parsing and rewriting.

#ifndef NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_PARSE_TEST_BASE_H_
#define NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_PARSE_TEST_BASE_H_

#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

class HtmlParseTestBaseNoAlloc : public testing::Test {
 protected:
  static const char kTestDomain[];
  static const char kXhtmlDtd[];    // DOCTYPE string for claiming XHTML

  HtmlParseTestBaseNoAlloc()
      : write_to_string_(&output_buffer_),
        added_filter_(false) {
  }
  virtual ~HtmlParseTestBaseNoAlloc();

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

  // If true, prepends "<html>\n" and appends "\n</html>" to input text
  // prior to parsing it.  This was originally done for consistency with
  // libxml2 but that's long since been made irrelevant and we should probably
  // just stop doing it.  Adding the virtual function here should help us
  // incrementally update tests & their gold results.
  virtual bool AddHtmlTags() const { return true; }

  // Set a doctype string (e.g. "<!doctype html>") to be inserted before the
  // rest of the document (for the current test only).  If none is set, it
  // defaults to the empty string.
  void SetDoctype(const StringPiece& directive) {
    directive.CopyToString(&doctype_string_);
  }

  virtual GoogleString AddHtmlBody(const StringPiece& html) {
    GoogleString ret;
    if (AddHtmlTags()) {
      ret = AddBody() ? "<html><body>\n" : "<html>\n";
      StrAppend(&ret, html, (AddBody() ? "\n</body></html>\n" : "\n</html>"));
    } else {
      html.CopyToString(&ret);
    }
    return ret;
  }

  // Check that the output HTML is serialized to string-compare
  // precisely with the input.
  void ValidateNoChanges(const StringPiece& case_id,
                         const GoogleString& html_input) {
    ValidateExpected(case_id, html_input, html_input);
  }

  // Fail to ValidateNoChanges.
  void ValidateNoChangesFail(const StringPiece& case_id,
                             const GoogleString& html_input) {
    ValidateExpectedFail(case_id, html_input, html_input);
  }

  void SetupWriter() {
    SetupWriter(&html_writer_filter_);
  }

  void SetupWriter(scoped_ptr<HtmlWriterFilter>* html_writer_filter) {
    output_buffer_.clear();
    if (html_writer_filter->get() == NULL) {
      html_writer_filter->reset(new HtmlWriterFilter(html_parse()));
      (*html_writer_filter)->set_writer(&write_to_string_);
      html_parse()->AddFilter(html_writer_filter->get());
    }
  }

  // Parse html_input, the result is stored in output_buffer_.
  void Parse(const StringPiece& case_id, const GoogleString& html_input) {
    // HtmlParser needs a valid HTTP URL to evaluate relative paths,
    // so we create a dummy URL.
    GoogleString dummy_url = StrCat(kTestDomain, case_id, ".html");
    ParseUrl(dummy_url, html_input);
  }

  // Parse given an explicit URL rather than an id to build URL around.
  virtual void ParseUrl(const StringPiece& url, const StringPiece& html_input);

  // Validate that the output HTML serializes as specified in
  // 'expected', which might not be identical to the input.
  // Also, returns true if result came out as expected.
  bool ValidateExpected(const StringPiece& case_id,
                        const GoogleString& html_input,
                        const GoogleString& expected);

  // Same as ValidateExpected, but with an explicit URL rather than an id.
  bool ValidateExpectedUrl(const StringPiece& url,
                           const GoogleString& html_input,
                           const GoogleString& expected);

  // Fail to ValidateExpected.
  void ValidateExpectedFail(const StringPiece& case_id,
                            const GoogleString& html_input,
                            const GoogleString& expected);

  virtual HtmlParse* html_parse() = 0;

  MockMessageHandler message_handler_;
  StringWriter write_to_string_;
  GoogleString output_buffer_;
  bool added_filter_;
  scoped_ptr<HtmlWriterFilter> html_writer_filter_;
  GoogleString doctype_string_;

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
