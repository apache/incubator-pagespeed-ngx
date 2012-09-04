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

#include "net/instaweb/rewriter/public/rewrite_test_base.h"
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
const char kPrefetchContainerStartTag[] =
    "<div class=\"psa_prefetch_container\">";
const char kPrefetchContainerEndTag[] =
    "</div>";
const char kPrefetchScriptTag[] =
    "<script src=\"%s\" type=\"psa_prefetch\"></script>";
const char kXUACompatibleMetaTag[] =
    "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">";

}  // namespace


class JsDisableFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    options_->EnableFilter(RewriteOptions::kDisableJavascript);
    RewriteTestBase::SetUp();
    filter_.reset(new JsDisableFilter(rewrite_driver()));
    rewrite_driver()->AddFilter(filter_.get());
  }

  virtual bool AddBody() const {
    return false;
  }

  virtual GoogleString AddHtmlBody(const StringPiece& html) {
    GoogleString ret;
    if (AddHtmlTags()) {
      ret = AddBody() ? "<html><body>" : "<html>";
      StrAppend(&ret, html, (AddBody() ? "</body></html>" : "</html>"));
    } else {
      html.CopyToString(&ret);
    }
    return ret;
  }

  GoogleString GetPrefetchScriptTag(const char* src) {
    return StringPrintf(kPrefetchScriptTag, src);
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
      "<script pagespeed_orig_src=\"blah1\" random=\"true\" type=\"text/psajs\""
      " orig_index=\"0\">hi1</script>",
      StrCat(kUnrelatedTags,
      "<img src=\"abc.jpg\" onload=\"pagespeed.deferJs.addOnloadListeners(this,"
      " function() {foo1();foo2();});\">"
      "<script pagespeed_orig_src=\"blah2\" random=\"false\""
      " type=\"text/psajs\" orig_index=\"1\">hi2</script>",
      kPrefetchContainerStartTag,
      GetPrefetchScriptTag("blah1"),
      GetPrefetchScriptTag("blah2"),
      kPrefetchContainerEndTag),
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
      "<script pagespeed_orig_src=\"blah1\" random=\"true\" type=\"text/psajs\""
      " orig_index=\"0\">hi1</script>",
      StrCat(kUnrelatedTags,
      "<script pagespeed_orig_src=\"blah2\" random=\"false\""
      " type=\"text/psajs\" orig_index=\"1\">hi2</script>",
      kPrefetchContainerStartTag,
      GetPrefetchScriptTag("blah1"),
      GetPrefetchScriptTag("blah2"),
      kPrefetchContainerEndTag),
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
      "<script pagespeed_orig_src=\"x?a=b&amp;c=d\" random=\"true\""
      " type=\"text/psajs\" orig_index=\"0\">hi1</script>",
      StrCat(kUnrelatedTags,
      "<script pagespeed_orig_src=\"y?a=b&amp;c=d\" random=\"false\""
      " type=\"text/psajs\" orig_index=\"1\">hi2</script>",
      kPrefetchContainerStartTag,
      GetPrefetchScriptTag("x?a=b&amp;c=d"),
      GetPrefetchScriptTag("y?a=b&amp;c=d"),
      kPrefetchContainerEndTag));

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
      "<script pagespeed_orig_src=\"x?a=b&c=d\" random=\"true\""
      " type=\"text/psajs\" orig_index=\"0\">hi1</script>",
      StrCat(kUnrelatedTags,
      "<script pagespeed_orig_src=\"y?a=b&c=d\" random=\"false\""
      " type=\"text/psajs\" orig_index=\"1\">hi2</script>",
      kPrefetchContainerStartTag,
      GetPrefetchScriptTag("x?a=b&c=d"),
      GetPrefetchScriptTag("y?a=b&c=d"),
      kPrefetchContainerEndTag));

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
      "<script pagespeed_orig_src random=\"true\" type=\"text/psajs\""
      " orig_index=\"0\">hi1</script>",
      kUnrelatedTags,
      "<script pagespeed_orig_src random=\"false\" type=\"text/psajs\""
      " orig_index=\"1\">hi2</script>");

  ValidateExpected("http://example.com/", input_html, expected);
}

TEST_F(JsDisableFilterTest, DisablesScriptOnlyFromFirstSrc) {
  options()->set_enable_defer_js_experimental(true);
  options_->EnableFilter(RewriteOptions::kDeferJavascript);
  const GoogleString input_html = StrCat(
      kUnrelatedNoscriptTags,
      "<script random=\"true\">hi1</script>",
      kUnrelatedTags,
      "<script random=\"false\">hi2</script>"
      "<script src=\"1.js?a#12296;=en\"></script>");
  const GoogleString expected = StrCat(
      kUnrelatedNoscriptTags,
      "<script random=\"true\">hi1</script>",
      kUnrelatedTags,
      "<script random=\"false\">hi2</script>"
      "<script pagespeed_orig_src=\"1.js?a#12296;=en\" type=\"text/psajs\""
      " orig_index=\"0\"></script>",
      StrCat(kPrefetchContainerStartTag,
      GetPrefetchScriptTag("1.js?a#12296;=en"),
      kPrefetchContainerEndTag));

  ValidateExpected("http://example.com/", input_html, expected);
}

TEST_F(JsDisableFilterTest, DisablesScriptInMultipleBodies) {
  options()->set_enable_defer_js_experimental(true);
  options_->EnableFilter(RewriteOptions::kDeferJavascript);
  const GoogleString input_html = StrCat(
      "<body>",
      kUnrelatedNoscriptTags,
      "<script random=\"true\">hi1</script>"
      "</body>"
      "<body>"
      "<script src=\"1.js\"></script>"
      "</body>",
      kUnrelatedTags,
      "<script random=\"false\">hi2</script>"
      "<script src=\"2.js\"></script>");
  const GoogleString expected = StrCat(
      "<head><script type=\"text/javascript\" pagespeed_no_defer=\"\">",
      JsDisableFilter::kEnableJsExperimental,
      "</script></head>"
      "<body>",
      StrCat(kUnrelatedNoscriptTags,
      "<script random=\"true\">hi1</script>"
      "</body>"
      "<body>"
      "<script pagespeed_orig_src=\"1.js\" type=\"text/psajs\""
      " orig_index=\"0\"></script>",
      kPrefetchContainerStartTag,
      GetPrefetchScriptTag("1.js"),
      kPrefetchContainerEndTag),
      "</body>",
      StrCat(kUnrelatedTags,
      "<script random=\"false\" type=\"text/psajs\" orig_index=\"1\">"
      "hi2</script>"
      "<script pagespeed_orig_src=\"2.js\" type=\"text/psajs\""
      " orig_index=\"2\"></script>",
      kPrefetchContainerStartTag,
      GetPrefetchScriptTag("2.js"),
      kPrefetchContainerEndTag));

  ValidateExpected("http://example.com/", input_html, expected);
}

TEST_F(JsDisableFilterTest, AddsMetaTagForIE) {
  rewrite_driver()->set_user_agent("Mozilla/5.0 ( MSIE 9.0; Trident/5.0)");
  const GoogleString input_html = StrCat(
      "<body>",
      kUnrelatedNoscriptTags,
      "<script src=\"blah1\" random=\"true\">hi1</script>",
      kUnrelatedTags,
      "</body>");
  const GoogleString expected = StrCat(
      StrCat("<head><script type=\"text/javascript\" pagespeed_no_defer=\"\">",
      JsDisableFilter::kDisableJsExperimental,
      "</script>",
      kXUACompatibleMetaTag,
      "</head>"
      "<body>"),
      StrCat(kUnrelatedNoscriptTags,
      "<script pagespeed_orig_src=\"blah1\" random=\"true\" type=\"text/psajs\""
      " orig_index=\"0\">hi1</script>",
      kUnrelatedTags,
      kPrefetchContainerStartTag,
      GetPrefetchScriptTag("blah1"),
      kPrefetchContainerEndTag),
      "</body>");

  ValidateExpectedUrl("http://example.com/", input_html, expected);
}

}  // namespace net_instaweb
