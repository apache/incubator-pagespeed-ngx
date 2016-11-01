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

#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "testing/base/public/gunit.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/user_agent_matcher_test_base.h"
#include "pagespeed/opt/logging/enums.pb.h"

namespace net_instaweb {

namespace {

const char kUnrelatedNoscriptTags[] =
    "<noscript>This is original noscript tag</noscript>";
const char kUnrelatedTags[] =
    "<div id=\"contentContainer\">"
    "<h1>Hello 1</h1>"
    "<div id=\"middleFooter\"><h3>Hello 3</h3></div>"
    "</div>";
const char kXUACompatibleMetaTag[] =
    "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">";

}  // namespace


class JsDisableFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    options()->EnableFilter(RewriteOptions::kDisableJavascript);
    options()->Disallow("*donotmove*");
  }

  virtual bool AddBody() const {
    return false;
  }

  void ExpectLogRecord(int index, int status, bool has_pagespeed_no_defer) {
    AbstractLogRecord* log_record = rewrite_driver_->log_record();
    const RewriterInfo& rewriter_info =
        log_record->logging_info()->rewriter_info(index);
    EXPECT_EQ("jd", rewriter_info.id());
    EXPECT_EQ(status, rewriter_info.status());
    EXPECT_EQ(has_pagespeed_no_defer,
              rewriter_info.rewrite_resource_info().has_pagespeed_no_defer());
  }
};

TEST_F(JsDisableFilterTest, DisablesScript) {
  const GoogleString input_html = StrCat(
      "<body>",
      kUnrelatedNoscriptTags,
      "<script></script>"
      "<script src=\"blah1\" random=\"true\">hi1</script>",
      kUnrelatedTags,
      "<img src=\"abc.jpg\" onload=\"foo1('abc');foo2();\">"
      "<script src=\"blah2\" random=\"false\">hi2</script>"
      "<script src=\"blah3\" data-pagespeed-no-defer=\"\"></script>"
      "<script src=\"blah4\" pagespeed_no_defer=\"\"></script>"
      "<script src=\"something-donotmove\"></script>"
      "</body>");
  const GoogleString expected = StrCat(
      "<body>",
      kUnrelatedNoscriptTags,
      "<script type=\"text/psajs\" data-pagespeed-orig-index=\"0\"></script>"
      "<script src=\"blah1\" random=\"true\" type=\"text/psajs\""
      " data-pagespeed-orig-index=\"1\">hi1</script>",
      kUnrelatedTags, StrCat(
      "<img src=\"abc.jpg\" data-pagespeed-onload=\"foo1('abc');foo2();\" "
      "onload=\"", JsDisableFilter::kElementOnloadCode, "\">"
      "<script src=\"blah2\" random=\"false\""
      " type=\"text/psajs\" data-pagespeed-orig-index=\"2\">hi2</script>"
      "<script src=\"blah3\" data-pagespeed-no-defer=\"\"></script>"
      "<script src=\"blah4\" pagespeed_no_defer=\"\"></script>"
      "<script src=\"something-donotmove\"></script>"
      "</body>"));

  ValidateExpectedUrl("http://example.com/", input_html, expected);
  ExpectLogRecord(0, RewriterApplication::APPLIED_OK, false);
  ExpectLogRecord(1, RewriterApplication::APPLIED_OK, false);
  ExpectLogRecord(2, RewriterApplication::APPLIED_OK, false);
  ExpectLogRecord(3, RewriterApplication::APPLIED_OK, true);
  ExpectLogRecord(4, RewriterApplication::APPLIED_OK, true);
  ExpectLogRecord(5, RewriterApplication::APPLIED_OK, true);
  rewrite_driver_->log_record()->WriteLog();
  for (int i = 0; i < logging_info()->rewriter_stats_size(); i++) {
    if (logging_info()->rewriter_stats(i).id() == "jd" &&
        logging_info()->rewriter_stats(i).has_html_status()) {
      EXPECT_EQ(RewriterHtmlApplication::ACTIVE,
                logging_info()->rewriter_stats(i).html_status());
      const RewriteStatusCount& count_applied =
          logging_info()->rewriter_stats(i).status_counts(0);
      EXPECT_EQ(RewriterApplication::APPLIED_OK,
                count_applied.application_status());
      EXPECT_EQ(6, count_applied.count());
      return;
    }
  }
  FAIL();
}

TEST_F(JsDisableFilterTest, InvalidUserAgent) {
  SetCurrentUserAgent("BlackListUserAgent");
  const char script[] = "<head>"
      "<script "
      "src='http://www.google.com/javascript/ajax_apis.js'></script>"
      "<script"
      "> func();</script>"
      "</head><body>Hello, world!</body>";

  ValidateNoChanges("defer_script", script);
  rewrite_driver_->log_record()->WriteLog();
  for (int i = 0; i < logging_info()->rewriter_stats_size(); i++) {
    if (logging_info()->rewriter_stats(i).id() == "jd" &&
        logging_info()->rewriter_stats(i).has_html_status()) {
      EXPECT_EQ(RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED,
                logging_info()->rewriter_stats(i).html_status());
      return;
    }
  }
  FAIL();
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
      "<body>",
      kUnrelatedNoscriptTags,
      "<script src=\"blah1\" random=\"true\" type=\"text/psajs\""
      " data-pagespeed-orig-index=\"0\">hi1</script>",
      kUnrelatedTags,
      "<script src=\"blah2\" random=\"false\""
      " type=\"text/psajs\" data-pagespeed-orig-index=\"1\">hi2</script>"
      "<script type=\"text/javascript\" data-pagespeed-no-defer>",
      JsDisableFilter::kEnableJsExperimental,
      "</script></body>");

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
      "<script src=\"x?a=b&amp;c=d\" random=\"true\""
      " type=\"text/psajs\" data-pagespeed-orig-index=\"0\">hi1</script>",
      kUnrelatedTags,
      "<script src=\"y?a=b&amp;c=d\" random=\"false\""
      " type=\"text/psajs\" data-pagespeed-orig-index=\"1\">hi2</script>");

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
      "<script src=\"x?a=b&c=d\" random=\"true\""
      " type=\"text/psajs\" data-pagespeed-orig-index=\"0\">hi1</script>",
      kUnrelatedTags,
      "<script src=\"y?a=b&c=d\" random=\"false\""
      " type=\"text/psajs\" data-pagespeed-orig-index=\"1\">hi2</script>");

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
      "<script src random=\"true\" type=\"text/psajs\""
      " data-pagespeed-orig-index=\"0\">hi1</script>",
      kUnrelatedTags,
      "<script src random=\"false\" type=\"text/psajs\""
      " data-pagespeed-orig-index=\"1\">hi2</script>");

  ValidateExpected("http://example.com/", input_html, expected);
}

TEST_F(JsDisableFilterTest, DisablesScriptOnlyFromFirstSrc) {
  options()->set_enable_defer_js_experimental(true);
  options_->EnableFilter(RewriteOptions::kDeferJavascript);
  options_->DisableFilter(RewriteOptions::kDisableJavascript);
  const GoogleString input_html = StrCat(
      "<body>",
      kUnrelatedNoscriptTags,
      "<script random=\"true\">hi1</script>",
      kUnrelatedTags,
      "<script random=\"false\">hi2</script>"
      "<script src=\"1.js?a#12296;=en\"></script></body>");
  // TODO(jmarantz): this URL is sure ugly.  find out why.
  static const char kUrl[] =
      "http://test.com/http://example.com/.html?PageSpeed=noscript";
  const GoogleString expected = StrCat(
      "<body>",
      StringPrintf(kNoScriptRedirectFormatter, kUrl, kUrl),
      kUnrelatedNoscriptTags,
      "<script random=\"true\" type=\"text/psajs\" "
      "data-pagespeed-orig-index=\"0\">hi1</script>",
      kUnrelatedTags,
      "<script random=\"false\" type=\"text/psajs\" "
      "data-pagespeed-orig-index=\"1\">hi2</script>"
      "<script src=\"1.js?a#12296;=en\" type=\"text/psajs\""
      " data-pagespeed-orig-index=\"2\"></script>"
      "<script type=\"text/javascript\" data-pagespeed-no-defer>",
      JsDisableFilter::kEnableJsExperimental,
      "</script><script type=\"text/javascript\" src=\"/psajs/js_defer.0.js\">"
      "</script></body>");

  ValidateExpected("http://example.com/", input_html, expected);
}

TEST_F(JsDisableFilterTest, AddsMetaTagForIE) {
  SetCurrentUserAgent("Mozilla/5.0 ( MSIE 10.0; Trident/5.0)");
  const GoogleString input_html = StrCat(
      "<body>",
      kUnrelatedNoscriptTags,
      "<script src=\"blah1\" random=\"true\">hi1</script>",
      kUnrelatedTags,
      "</body>");
  const GoogleString expected = StrCat(
      StrCat("<head>",
      kXUACompatibleMetaTag,
      "</head>"
      "<body>"),
      StrCat(kUnrelatedNoscriptTags,
      "<script src=\"blah1\" random=\"true\" type=\"text/psajs\""
      " data-pagespeed-orig-index=\"0\">hi1</script>",
      kUnrelatedTags),
      "</body>");

  ValidateExpectedUrl("http://example.com/", input_html, expected);
}

TEST_F(JsDisableFilterTest, DisablesScriptWithMultipleTypeAttributes) {
  const GoogleString input_html = StrCat(
      kUnrelatedNoscriptTags,
      "<script src=\"x?a=b&amp;c=d\" type='text/javascript' type='a' type='b'>"
      "hi1</script>",
      kUnrelatedTags);
  const GoogleString expected = StrCat(
      kUnrelatedNoscriptTags,
      "<script src=\"x?a=b&amp;c=d\""
      " data-pagespeed-orig-type='text/javascript'"
      " type=\"text/psajs\" data-pagespeed-orig-index=\"0\">hi1</script>",
      kUnrelatedTags);

  ValidateExpectedUrl("http://example.com/", input_html, expected);
}

TEST_F(JsDisableFilterTest, ScriptWithPagespeedPrioritizeAttribute) {
  options()->set_enable_prioritizing_scripts(true);
  const GoogleString input_html = StrCat(
      "<body>",
      kUnrelatedNoscriptTags,
      "<script src=\"blah1\" random=\"true\">hi1</script>",
      kUnrelatedTags,
      "<img src=\"abc.jpg\" onload=\"foo1();foo2();\">"
      "<script src=\"blah2\" random=\"false\" data-pagespeed-prioritize>hi2"
      "</script>"
      "<script data-pagespeed-prioritize>hi5</script>"
      "</body>");
  const GoogleString expected = StrCat(
      "<body>",
      kUnrelatedNoscriptTags,
      "<script src=\"blah1\" random=\"true\" type=\"text/psajs\""
      " data-pagespeed-orig-index=\"0\">hi1</script>",
      kUnrelatedTags, StrCat(
      "<img src=\"abc.jpg\" data-pagespeed-onload=\"foo1();foo2();\" "
      "onload=\"", JsDisableFilter::kElementOnloadCode, "\">"
      "<script src=\"blah2\" random=\"false\" data-pagespeed-prioritize "
      "type=\"text/prioritypsajs\" data-pagespeed-orig-index=\"1\">"
      "hi2</script>"
      "<script data-pagespeed-prioritize type=\"text/prioritypsajs\" "
      "data-pagespeed-orig-index=\"2\">hi5</script>"
      "</body>"));
  ValidateExpectedUrl("http://example.com/", input_html, expected);
}

}  // namespace net_instaweb
