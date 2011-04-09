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

// Author: slamm@google.com (Stephen Lamm)
#include "net/instaweb/rewriter/google_analytics_snippet.h"
#include "net/instaweb/rewriter/public/google_analytics_filter.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"

#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

namespace {

// Expected async glue methods as written to the html output.
// The test sets this list in SetUp().
const char kAsyncGlueMethods[] =
    "        '_trackPageview',\n"
    "        '_setCookiePath'\n";

// Typical initialization code for the synchronous snippet.
const char kSyncInit[] =
    "<script type=\"text/javascript\">\n"
    "try {\n"
    "var pageTracker = _gat._getTracker(\"UA-XXXXX-X\");\n"
    "pageTracker._trackPageview();"
    "} catch(err) {}"
    "</script>";

// The initialization code with _getTracker replaced.
const char kAsyncGlueInit[] =
    "<script type=\"text/javascript\">\n"
    "try {\n"
    "var pageTracker = _modpagespeed_getRewriteTracker(\"UA-XXXXX-X\");\n"
    "pageTracker._trackPageview();"
    "} catch(err) {}"
    "</script>";


class GoogleAnalyticsFilterTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    // Setup the statistics.
    ResourceManagerTestBase::SetUp();

    Statistics* statistics = resource_manager_->statistics();

    GoogleAnalyticsFilter::Initialize(statistics);
    GoogleAnalyticsFilter::MethodVector* glue_methods =
        new GoogleAnalyticsFilter::MethodVector;
    GoogleAnalyticsFilter::MethodVector* unhandled_methods =
        new GoogleAnalyticsFilter::MethodVector;
    glue_methods->push_back("_trackPageview");
    glue_methods->push_back("_setCookiePath");
    unhandled_methods->push_back("_get");
    unhandled_methods->push_back("_getLinkerUrl");

    rewrite_driver_.AddOwnedFilter(new GoogleAnalyticsFilter(
        &rewrite_driver_, statistics, glue_methods, unhandled_methods));
  }

  // Create the expected html.
  GoogleString GetAsyncLoadAndInit(
      const StringPiece& load_prefix, const StringPiece& load_suffix) const {
    return StrCat(
        load_prefix,
        kGaSnippetPrefix,  // from google_analytics_snippet.h
        kAsyncGlueMethods,
        kGaSnippetSuffix,  // from google_analytics_snippet.h
        load_suffix,
        kAsyncGlueInit);
  }

  GoogleAnalyticsFilter* google_analytics_filter_;
};

TEST_F(GoogleAnalyticsFilterTest, SyncScriptSrcMadeAsync) {
  GoogleString html_input = StrCat(
      "<script src=\"http://www.google-analytics.com/ga.js\""
          " type=\"text/javascript\">\n"
      "</script>\n",
      kSyncInit);
  GoogleString expected_html = GetAsyncLoadAndInit(
      "<script type=\"text/javascript\">",
      "\n</script>\n");  // starting newline leftover from empty script
  ValidateExpected("sync_script_src_made_async",
                   html_input, expected_html);
}

// Test the boundary case of load script without a characters node
TEST_F(GoogleAnalyticsFilterTest, SyncScriptSrcNoCharactersNodeMadeAsync) {
  GoogleString html_input = StrCat(
      "<script src=\"http://www.google-analytics.com/ga.js\""
          " type=\"text/javascript\"></script>\n",
      kSyncInit);
  GoogleString expected_html = GetAsyncLoadAndInit(
      "<script type=\"text/javascript\">",
      "</script>\n");  // no newline between script tags
  ValidateExpected("sync_script_src_no_characters_node_made_async",
                   html_input, expected_html);
}

TEST_F(GoogleAnalyticsFilterTest, SyncDocumentWriteMadeAsync) {
  GoogleString html_input = StrCat(
      "<script type='text/javascript'>\n"
      "var gaJsHost = ((\"https:\" == document.location.protocol) ? "
          "\"https://ssl.\" : \"http://www.\");\n"
      "document.write(unescape(\"%3Cscript src=\" + gaJsHost + \"google-"
      "analytics.com/ga.js' type='text/javascript'%3E%3C/script%3E\"));\n"
      "</script>\n",
      kSyncInit);
  GoogleString expected_html = GetAsyncLoadAndInit(
      "<script type='text/javascript'>\n"
      "var gaJsHost = ((\"https:\" == document.location.protocol) ? "
          "\"https://ssl.\" : \"http://www.\");\n",
      "\n</script>\n");
  ValidateExpected("sync_document_write_made_async",
                   html_input, expected_html);
}

// Test the boundary case of "document.write" at the first position
// of the characters node.
TEST_F(GoogleAnalyticsFilterTest, SyncDocumentWriteAtPositionZero) {
  GoogleString html_input = StrCat(
      "<script type='text/javascript'>document.write("
          "unescape(\"%3Cscript src=\"http://www.google-analytics.com/ga.js' "
          "type='text/javascript'%3E%3C/script%3E\"));\n"
      "</script>\n",
      kSyncInit);
  GoogleString expected_html = GetAsyncLoadAndInit(
      "<script type='text/javascript'>",
      "\n</script>\n");
  ValidateExpected("sync_document_write_made_async",
                   html_input, expected_html);
}

TEST_F(GoogleAnalyticsFilterTest, MultipleLoadReducedToSingle) {
  GoogleString html_input = StrCat(
      "<script type='text/javascript'>document.write("
          "unescape(\"%3Cscript src=\"http://www.google-analytics.com/ga.js' "
          "type='text/javascript'%3E%3C/script%3E\"));\n"
      "</script>\n",
      kSyncInit,
      "<script type='text/javascript'>\n"
      "var gaJsHost = ((\"https:\" == document.location.protocol) ? "
          "\"https://ssl.\" : \"http://www.\");\n"
      "document.write(unescape(\"%3Cscript src=\" + gaJsHost + \"google-"
      "analytics.com/ga.js' type='text/javascript'%3E%3C/script%3E\"));\n"
      "</script>\n",
      kSyncInit,
      "<script src=\"http://www.google-analytics.com/ga.js\""
          " type=\"text/javascript\"></script>",
      kSyncInit);
  GoogleString expected_html = StrCat(
      GetAsyncLoadAndInit(
          "<script type='text/javascript'>",
          "\n</script>\n"),
      "<script type='text/javascript'>\n"
      "var gaJsHost = ((\"https:\" == document.location.protocol) ?"
          " \"https://ssl.\" : \"http://www.\");\n"
      // The second load (document.write) is removed completely.
      "\n"
      "</script>\n",
      kAsyncGlueInit,
      // The third load (script src) is removed completely.
      kAsyncGlueInit);
  ValidateExpected("multiple_load_reduced to_single",
                   html_input, expected_html);
}

TEST_F(GoogleAnalyticsFilterTest, AsyncGiveNoChanges) {
  GoogleString html_input =
      "<script type=\"text/javascript\">\n"
      "var _gaq = _gaq || [];\n"
      "_gaq.push(['_setAccount', 'UA-XXXXX-X']);\n"
      "_gaq.push(['_trackPageview']);\n"
      "(function() {\n"
      "  var ga = document.createElement('script');\n"
      "  ga.type = 'text/javascript'; ga.async = true;\n"
      "  ga.src = ('https:' == document.location.protocol ? 'https://ssl' : "
          "'http://www') +\n"
      "    '.google-analytics.com/ga.js';\n"
      "  var s = document.getElementsByTagName('script')[0]\n"
      "  s.parentNode.insertBefore(ga, s);\n"
      "})();\n"
      "</script>\n";
  ValidateNoChanges("async_unchanged", html_input);
}

TEST_F(GoogleAnalyticsFilterTest, NonstandardInitGiveNoChanges) {
  GoogleString html_input =
      "<script type=\"text/javascript\">\n"
      "googleAnalytics = new TFHtmlUtilsGoogleAnalytics({\n"
      "  'trackingcode': \"UA-XXXXX-X\"\n"
      "});\n"
      "googleAnalytics.trackPageview();\n"
      "} catch(err) {}\n"
      "</script>\n";
  ValidateNoChanges("nonstandard_init_unchanged", html_input);
}

TEST_F(GoogleAnalyticsFilterTest, UnhandledCallCausesSkippedRewrite) {
  GoogleString html_input = StrCat(
      "<script src=\"http://www.google-analytics.com/ga.js\""
          " type=\"text/javascript\"></script>\n",
      kSyncInit,
      // _getLinkerUrl is listed as an unhandled call.
      "<script type=\"text/javascript\">pageTracker._getLinkerUrl ();\n"
      "</script>\n");
  ValidateNoChanges("unhandled_call_causes_skipped_rewrite", html_input);
}

TEST_F(GoogleAnalyticsFilterTest, UnknownCallStillAllowsRewrite) {
  GoogleString html_input = StrCat(
      "<script src=\"http://www.google-analytics.com/ga.js\""
          " type=\"text/javascript\"></script>\n",
      kSyncInit,
      // _getFoo is not explicitly listed as an unhandled call.
      "<script type=\"text/javascript\">pageTracker._getFoo();</script>\n");
  GoogleString expected_html = StrCat(
      GetAsyncLoadAndInit(
          "<script type=\"text/javascript\">",
          "</script>\n"),
      "<script type=\"text/javascript\">pageTracker._getFoo();</script>\n");
  ValidateExpected("unknown_call_still_allows_rewrite",
                   html_input, expected_html);
}

}  // namespace

}  // namespace net_instaweb
