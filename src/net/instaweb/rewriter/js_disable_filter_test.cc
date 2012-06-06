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

// Author: gagansingh@google.com (Gagan Singh)

#include "net/instaweb/rewriter/public/js_disable_filter.h"

#include "base/scoped_ptr.h"

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "testing/base/public/gunit.h"

namespace net_instaweb {

namespace {

const char kUnrelatedNoscriptTags[] =
    "<noscript>This is original noscript tag</noscript>";
const char kUnrelatedTags[] =
    "<div id=\"contentContainer\">"
    "<h1>Hello 1</h1>"
    "<div id=\"middleFooter\"><h3>Hello 3</h3></div>"
    "</div>";

}  // namespace


class JsDisableFilterTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    options_->EnableFilter(RewriteOptions::kDisableJavascript);
    ResourceManagerTestBase::SetUp();
    filter_.reset(new JsDisableFilter(rewrite_driver()));
    rewrite_driver()->AddFilter(filter_.get());
  }

  virtual bool AddBody() const {
    return false;
  }

  scoped_ptr<JsDisableFilter> filter_;
};

TEST_F(JsDisableFilterTest, DisablesScript) {
  const GoogleString input_html = StrCat(
      "<body>",
      kUnrelatedNoscriptTags,
      "<script src=\"blah1\" random=\"true\">hi1</script>",
      kUnrelatedTags,
      "<img src=\"abc.jpg\" onload=\"foo1();foo2();\">"
      "<script src=\"blah2\" random=\"false\">hi2</script>"
      "</body>");
  const GoogleString expected = StrCat(
      "<head><script type=\"text/javascript\" pagespeed_no_defer=\"\">",
      JsDisableFilter::kDisableJsExperimental,
      "</script></head>"
      "<body>",
      kUnrelatedNoscriptTags,
      "<script random=\"true\" orig_src=\"blah1\" type=\"text/psajs\""
      " orig_index=\"0\">hi1</script>",
      kUnrelatedTags,
      "<img src=\"abc.jpg\" onload=\"pagespeed.deferJs.addOnloadListeners(this,"
      " function() {foo1();foo2();});\">"
      "<script random=\"false\" orig_src=\"blah2\" type=\"text/psajs\""
      " orig_index=\"1\">hi2</script>"
      "</body>");

  ValidateExpectedUrl("http://example.com/", input_html, expected);
}

TEST_F(JsDisableFilterTest, DisablesScriptWithExperimental) {
  options()->set_enable_defer_js_experimental(true);

  const char kUnrelatedNoscriptTags[] =
      "<noscript>This is original noscript tag</noscript>";
  const char kUnrelatedTags[] =
      "<div id=\"contentContainer\">"
      "<h1>Hello 1</h1>"
      "<div id=\"middleFooter\"><h3>Hello 3</h3></div>"
      "</div>";

  const GoogleString input_html = StrCat(
      "<body>",
      kUnrelatedNoscriptTags,
      "<script src=\"blah1\" random=\"true\">hi1</script>",
      kUnrelatedTags,
      "<script src=\"blah2\" random=\"false\">hi2</script>",
      "</body>");
  const GoogleString expected = StrCat(
      "<head><script type=\"text/javascript\" pagespeed_no_defer=\"\">",
      JsDisableFilter::kEnableJsExperimental,
      "</script></head>"
      "<body>",
      kUnrelatedNoscriptTags,
      "<script random=\"true\" orig_src=\"blah1\" type=\"text/psajs\""
      " orig_index=\"0\">hi1</script>",
      kUnrelatedTags,
      "<script random=\"false\" orig_src=\"blah2\" type=\"text/psajs\""
      " orig_index=\"1\">hi2</script>"
      "</body>");

  ValidateExpectedUrl("http://example.com/", input_html, expected);
}

TEST_F(JsDisableFilterTest, DisablesScriptWithQueryParam) {
  const GoogleString input_html = StrCat(
      kUnrelatedNoscriptTags,
      "<script src=\"x?a=b&amp;c=d\" random=\"true\">hi1</script>",
      kUnrelatedTags,
      "<script src=\"y?a=b&amp;c=d\" random=\"false\">hi2</script>");
  const GoogleString expected = StrCat(
      kUnrelatedNoscriptTags,
      "<script random=\"true\" orig_src=\"x?a=b&amp;c=d\" type=\"text/psajs\""
      " orig_index=\"0\">hi1</script>",
      kUnrelatedTags,
      "<script random=\"false\" orig_src=\"y?a=b&amp;c=d\" type=\"text/psajs\""
      " orig_index=\"1\">hi2</script>");

  ValidateExpectedUrl("http://example.com/", input_html, expected);
}

TEST_F(JsDisableFilterTest, DisablesScriptWithUnescapedQueryParam) {
  const GoogleString input_html = StrCat(
      kUnrelatedNoscriptTags,
      "<script src=\"x?a=b&c=d\" random=\"true\">hi1</script>",
      kUnrelatedTags,
      "<script src=\"y?a=b&c=d\" random=\"false\">hi2</script>");
  const GoogleString expected = StrCat(
      kUnrelatedNoscriptTags,
      "<script random=\"true\" orig_src=\"x?a=b&amp;c=d\" type=\"text/psajs\""
      " orig_index=\"0\">hi1</script>",
      kUnrelatedTags,
      "<script random=\"false\" orig_src=\"y?a=b&amp;c=d\" type=\"text/psajs\""
      " orig_index=\"1\">hi2</script>");

  ValidateExpectedUrl("http://example.com/", input_html, expected);
}

TEST_F(JsDisableFilterTest, DisablesScriptWithNullSrc) {
  const GoogleString input_html = StrCat(
      kUnrelatedNoscriptTags,
      "<script src random=\"true\">hi1</script>",
      kUnrelatedTags,
      "<script src random=\"false\">hi2</script>");
  const GoogleString expected = StrCat(
      kUnrelatedNoscriptTags,
      "<script src random=\"true\" type=\"text/psajs\" orig_index=\"0\">hi1"
      "</script>",
      kUnrelatedTags,
      "<script src random=\"false\" type=\"text/psajs\" orig_index=\"1\">hi2"
      "</script>");

  ValidateExpected("http://example.com/", input_html, expected);
}

TEST_F(JsDisableFilterTest, DisablesScriptOnlyFromFirstSrc) {
  options()->set_enable_defer_js_experimental(true);
  const GoogleString input_html = StrCat(
      kUnrelatedNoscriptTags,
      "<script random=\"true\">hi1</script>",
      kUnrelatedTags,
      "<script random=\"false\">hi2</script>"
      "<script src=\"1.js\"></script>");
  const GoogleString expected = StrCat(
      kUnrelatedNoscriptTags,
      "<script random=\"true\">hi1</script>",
      kUnrelatedTags,
      "<script random=\"false\">hi2</script>"
      "<script orig_src=\"1.js\" type=\"text/psajs\" orig_index=\"0\">"
      "</script>");

  ValidateExpected("http://example.com/", input_html, expected);
}

}  // namespace net_instaweb
