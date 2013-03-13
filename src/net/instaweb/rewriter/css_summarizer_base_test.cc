/*
 * Copyright 2013 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/rewriter/public/css_summarizer_base.h"

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/public/css_minify.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

namespace {

// Extracts first 10 characters of minified form of every stylesheet.
class MinifyExcerptFilter : public CssSummarizerBase {
 public:
  explicit MinifyExcerptFilter(RewriteDriver* driver)
      : CssSummarizerBase(driver, "Minify10", "csr"),
        driver_(driver) {}

  virtual void Summarize(const Css::Stylesheet& stylesheet,
                         GoogleString* out) const {
    StringWriter write_out(out);
    CssMinify::Stylesheet(stylesheet, &write_out, driver_->message_handler());
    if (out->length() > 10) {
      out->resize(10);
    }
  }

  virtual void SummariesDone() {
    for (int i = 0; i < NumStyles(); ++i) {
      const GoogleString* sum = GetSummary(i);
      if (sum != NULL) {
        StrAppend(&result_, *sum);
      } else {
        StrAppend(&result_, "(nil)");
      }
      StrAppend(&result_, "|");
    }
  }

  const GoogleString& result() { return result_; }

 private:
  GoogleString result_;
  RewriteDriver* driver_;
};

class CssSummarizerBaseTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    filter_ = new MinifyExcerptFilter(rewrite_driver());
    rewrite_driver()->AppendOwnedPreRenderFilter(filter_);
    server_context()->ComputeSignature(options());

    // Valid CSS.
    SetResponseWithDefaultHeaders("a.css", kContentTypeCss,
                                  "div { display: block; }", 100);

    // Parse error.
    SetResponseWithDefaultHeaders("b.css", kContentTypeCss,
                                  "div { ", 100);

    // Permit testing a 404.
    SetFetchFailOnUnexpected(false);

    // An inline div? evil indeed.
    SetResponseWithDefaultHeaders("http://evil.com/d.css", kContentTypeCss,
                                  "div { display: inline; }", 100);
  }

  MinifyExcerptFilter* filter_;  // owned by the driver;
};

TEST_F(CssSummarizerBaseTest, BasicOperation) {
  ValidateNoChanges("foo",
                    StrCat("<style>* {display: none; }</style>",
                           CssLinkHref("a.css"),  // ok
                           CssLinkHref("b.css"),  // parse error
                           CssLinkHref("404.css"),  // fetch error
                           CssLinkHref("http://evil.com/d.css")));
  EXPECT_EQ("*{display:|div{displa|(nil)|(nil)|(nil)|", filter_->result());
}

}  // namespace

}  // namespace net_instaweb
