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
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "testing/base/public/gunit.h"
#include "pagespeed/kernel/base/string_writer.h"

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
    options_->EnableFilter(RewriteOptions::kDisableJavascript);
    options_->EnableFilter(RewriteOptions::kDeferJavascript);
    RewriteTestBase::SetUp();
    filter_.reset(new JsDisableFilter(rewrite_driver()));
    rewrite_driver()->AddFilter(filter_.get());
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

  scoped_ptr<JsDisableFilter> filter_;
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
      "<script src=\"blah3\" pagespeed_no_defer=\"\"></script>"
      "</body>");
  const GoogleString expected = StrCat(
      "<body>",
      kUnrelatedNoscriptTags,
      "<script type=\"text/psajs\" orig_index=\"0\"></script>"
      "<script src=\"blah1\" random=\"true\" type=\"text/psajs\""
      " orig_index=\"1\">hi1</script>",
      kUnrelatedTags, StrCat(
      "<img src=\"abc.jpg\" data-pagespeed-onload=\"foo1('abc');foo2();\" "
      "onload=\"", JsDisableFilter::kElementOnloadCode, "\">"
      "<script src=\"blah2\" random=\"false\""
      " type=\"text/psajs\" orig_index=\"2\">hi2</script>"
      "<script src=\"blah3\" pagespeed_no_defer=\"\"></script>"
      "</body>"));

  ValidateExpectedUrl("http://example.com/", input_html, expected);
  ExpectLogRecord(0, RewriterApplication::APPLIED_OK, false);
  ExpectLogRecord(1, RewriterApplication::APPLIED_OK, false);
  ExpectLogRecord(2, RewriterApplication::APPLIED_OK, false);
  ExpectLogRecord(3, RewriterApplication::APPLIED_OK, true);
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
      EXPECT_EQ(4, count_applied.count());
      return;
    }
  }
  FAIL();
}

TEST_F(JsDisableFilterTest, InvalidUserAgent) {
  rewrite_driver()->SetUserAgent("BlackListUserAgent");
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
      " orig_index=\"0\">hi1</script>",
      kUnrelatedTags,
      "<script src=\"blah2\" random=\"false\""
      " type=\"text/psajs\" orig_index=\"1\">hi2</script>"
      "<script type=\"text/javascript\" pagespeed_no_defer=\"\">",
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
      " type=\"text/psajs\" orig_index=\"0\">hi1</script>",
      kUnrelatedTags,
      "<script src=\"y?a=b&amp;c=d\" random=\"false\""
      " type=\"text/psajs\" orig_index=\"1\">hi2</script>");

  ValidateExpectedUrl("http://example.com/", input_html, expected);
}

TEST_F(JsDisableFilterTest, PrefetchScriptWithImageTemplate) {
  rewrite_driver()->SetUserAgent(UserAgentMatcherTestBase::kChrome15UserAgent);
  options()->set_max_prefetch_js_elements(3);
  const GoogleString src_url = "//abc.org/m/load.php?debug=false&amp;"
      "lang=en&amp;modules=startup&amp;only=scripts&amp;*";
  // Verify JS escaping works as expected and not HTML escaping.
  const GoogleString escaped_src = "//abc.org/m/load.php?debug=false&lang=en&"
      "modules=startup&only=scripts&*";

  const GoogleString expected = StrCat(
      "<head>"
      "<script type=\"text/psajs\" orig_index=\"0\"></script>"
      "<script src=\"blah1\" random=\"true\" type=\"text/psajs\""
      " orig_index=\"1\">hi1</script>"
      "<script src=\"blah2\" random=\"false\""
      " type=\"text/psajs\" orig_index=\"2\">hi2</script>"
      "<script src=\"blah3\" pagespeed_no_defer=\"\"></script>"
      "<script src=\"", src_url, "\" type=\"text/psajs\""
      " orig_index=\"3\">hi4</script>"
      "<script src=\"blah5\" type=\"text/psajs\""
      " orig_index=\"4\">Not a prefetch candidate</script>"
      "</head><body>"
      "<script pagespeed_no_defer=\"\">(function(){"
      "new Image().src=\"blah1\";"
      "new Image().src=\"blah2\";"
      "new Image().src=\"", escaped_src, "\";})()"
      "</script></body>");

  html_parse()->SetWriter(&write_to_string_);
  html_parse()->StartParse("http://example.com");
  html_parse()->ParseText(
      "<head>"
      "<script></script>"
      "<script src=\"blah1\" random=\"true\">hi1");
  html_parse()->Flush();
  html_parse()->ParseText(StrCat("</script>"
      "<script src=\"blah2\" random=\"false\">hi2</script>"
      "<script src=\"blah3\" pagespeed_no_defer=\"\"></script>"
      "<script src=\"", src_url, "\">hi4</script>"
      "<script src=\"blah5\">Not a prefetch candidate</script>"
      "</head><body>"));
  html_parse()->Flush();
  html_parse()->ParseText("</body>");
  html_parse()->FinishParse();
  EXPECT_STREQ(expected, output_buffer_);
}

TEST_F(JsDisableFilterTest, PrefetchScriptInHeadNotInBody) {
  rewrite_driver()->SetUserAgent(UserAgentMatcherTestBase::kChrome15UserAgent);
  options()->set_max_prefetch_js_elements(3);
  const GoogleString input_html = StrCat(
      "<head>",
      kUnrelatedNoscriptTags,
      "<script></script>"
      "<script src=\"blah1\" random=\"true\">hi1</script>",
      kUnrelatedTags,
      "<script src=\"blah2\" random=\"false\">hi2</script>"
      "</head><body>"
      "<script src=\"blah3\" pagespeed_no_defer=\"\"></script>"
      "<script src=\"blah4\">dont show up in prefetch script</script>"
      "</body>");

  const GoogleString prefetch_script =
      "<script pagespeed_no_defer=\"\">(function(){"
      "new Image().src=\"blah1\";"
      "new Image().src=\"blah2\";})()"
      "</script>";

  const GoogleString expected = StrCat(
      "<head>",
      kUnrelatedNoscriptTags,
      "<script type=\"text/psajs\" orig_index=\"0\"></script>"
      "<script src=\"blah1\" random=\"true\" type=\"text/psajs\""
      " orig_index=\"1\">hi1</script>",
      kUnrelatedTags, StrCat(
      "<script src=\"blah2\" random=\"false\""
      " type=\"text/psajs\" orig_index=\"2\">hi2</script>"
      "</head>"
      "<body>", prefetch_script,
      "<script src=\"blah3\" pagespeed_no_defer=\"\"></script>"
      "<script src=\"blah4\" type=\"text/psajs\""
      " orig_index=\"3\">dont show up in prefetch script</script>"
      "</body>"));

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
      " type=\"text/psajs\" orig_index=\"0\">hi1</script>",
      kUnrelatedTags,
      "<script src=\"y?a=b&c=d\" random=\"false\""
      " type=\"text/psajs\" orig_index=\"1\">hi2</script>");

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
      " orig_index=\"0\">hi1</script>",
      kUnrelatedTags,
      "<script src random=\"false\" type=\"text/psajs\""
      " orig_index=\"1\">hi2</script>");

  ValidateExpected("http://example.com/", input_html, expected);
}

TEST_F(JsDisableFilterTest, DisablesScriptOnlyFromFirstSrc) {
  options()->set_enable_defer_js_experimental(true);
  options_->EnableFilter(RewriteOptions::kDeferJavascript);
  const GoogleString input_html = StrCat(
      "<body>",
      kUnrelatedNoscriptTags,
      "<script random=\"true\">hi1</script>",
      kUnrelatedTags,
      "<script random=\"false\">hi2</script>"
      "<script src=\"1.js?a#12296;=en\"></script></body>");
  const GoogleString expected = StrCat(
      "<body>",
      kUnrelatedNoscriptTags,
      "<script random=\"true\" type=\"text/psajs\" orig_index=\"0\">"
      "hi1</script>",
      kUnrelatedTags,
      "<script random=\"false\" type=\"text/psajs\" orig_index=\"1\">"
      "hi2</script>"
      "<script src=\"1.js?a#12296;=en\" type=\"text/psajs\""
      " orig_index=\"2\"></script>"
      "<script type=\"text/javascript\" pagespeed_no_defer=\"\">",
      JsDisableFilter::kEnableJsExperimental,
      "</script></body>");

  ValidateExpected("http://example.com/", input_html, expected);
}

TEST_F(JsDisableFilterTest, AddsMetaTagForIE) {
  rewrite_driver()->SetUserAgent("Mozilla/5.0 ( MSIE 9.0; Trident/5.0)");
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
      " orig_index=\"0\">hi1</script>",
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
      "<script src=\"x?a=b&amp;c=d\" pagespeed_orig_type='text/javascript' "
      "type=\"text/psajs\" orig_index=\"0\">hi1</script>",
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
      " orig_index=\"0\">hi1</script>",
      kUnrelatedTags, StrCat(
      "<img src=\"abc.jpg\" data-pagespeed-onload=\"foo1();foo2();\" "
      "onload=\"", JsDisableFilter::kElementOnloadCode, "\">"
      "<script src=\"blah2\" random=\"false\" data-pagespeed-prioritize "
      "type=\"text/prioritypsajs\" orig_index=\"1\">"
      "hi2</script>"
      "<script data-pagespeed-prioritize type=\"text/prioritypsajs\" "
      "orig_index=\"2\">hi5</script>"
      "</body>"));
  ValidateExpectedUrl("http://example.com/", input_html, expected);
}

}  // namespace net_instaweb
