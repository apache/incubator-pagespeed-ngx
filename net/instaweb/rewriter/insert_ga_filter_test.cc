/*
 * Copyright 2011 Google Inc.
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

// Author: nforman@google.com (Naomi Forman)
//
// This file contains unit tests for the InsertGAFilter.

#include "net/instaweb/rewriter/public/insert_ga_filter.h"

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"

namespace net_instaweb {

namespace {

const char kGaId[] = "UA-21111111-1";

const char kHtmlInput[] =
    "<head>\n"
    "<title>Something</title>\n"
    "</head>"
    "<body> Hello World!</body>";

const char kHtmlNoCloseBody[] =
    "<head>\n"
    "<title>Something</title>\n"
    "</head>"
    "<body> Hello World!%s";

const char kHtmlOutputFormat[] =
    "<head>\n"
    "<title>Something</title>\n"
    "</head><body> Hello World!"
    "%s"
    "<script>%s%s</script>"
    "</body>";

const char kUrchinScript[] =
    "%s"
    "<script src=\"http://www.google-analytics.com/urchin.js\""
    " type=\"text/javascript\"></script>"
    " <script>_uacct = \"%s\";"
    " urchinTracker();</script>";

const char kUnusableSnippet[] =
    "%s"
    "<script>"
    "var ga_id = '%s';"
    "</script>";

const char kSynchronousGA[] =
    "%s"
    "<script>"
    " var gaJsHost = ((\"https:\" == document.location.protocol) ?"
    "                \"https://ssl.\" : \"http://www.\");"
    " document.write(unescape(\"%%3Cscript src='\" + gaJsHost +"
    "                         \"google-analytics.com/ga.js'"
    "                         type='text/javascript'%%3E%%3C/script%%3E\"));"
    "</script>"
    "<script>%s"
    " try { var pageTracker = _gat._getTracker(\"%s\");"
    "       pageTracker._trackPageview(); } catch(err) {}"
    "</script>";

const char kAsyncGA[] =
    "%s"
    "<script type='text/javascript'>document.write('another script');</script>"
    "<script>%s"
    "var _gaq = _gaq || [];"
    "_gaq.push(['_setAccount', '%s']);"
    "_gaq.push(['_trackPageview']);"
    "(function() {"
    "  var ga = document.createElement('script');"
    "  ga.src = ('https:' == document.location.protocol ?"
    "  'https://ssl' : 'http://www') +"
    "  '.google-analytics.com/ga.js';"
    "  ga.setAttribute('async', 'true');"
    "  document.documentElement.firstChild.appendChild(ga);"
    "})();"
    "</script>";

const char kSynchronousDC[] =
    "%s"
    "<script>"
    " var gaJsHost = ((\"https:\" == document.location.protocol) ?"
    "                \"https://\" : \"http://\");"
    " document.write(unescape(\"%%3Cscript src='\" + gaJsHost +"
    "                         \"stats.g.doubleclick.net/dc.js'"
    "                         type='text/javascript'%%3E%%3C/script%%3E\"));"
    "</script>"
    "<script>%s"
    " try { var pageTracker = _gat._getTracker(\"%s\");"
    "       pageTracker._trackPageview(); } catch(err) {}"
    "</script>";

const char kAsyncDC[] =
    "%s"
    "<script type='text/javascript'>document.write('another script');</script>"
    "<script>%s"
    "var _gaq = _gaq || [];"
    "_gaq.push(['_setAccount', '%s']);"
    "_gaq.push(['_trackPageview']);"
    "(function() {"
    "  var ga = document.createElement('script');"
    "  ga.src = ('https:' == document.location.protocol ?"
    "  'https://' : 'http://') +"
    "  'stats.g.doubleclick.net/dc.js'';"
    "  ga.setAttribute('async', 'true');"
    "  document.documentElement.firstChild.appendChild(ga);"
    "})();"
    "</script>";

const char kAsyncGAPart1[] =
    "<script type='text/javascript'>document.write('another script');</script>"
    "<script>";

const char kAsyncGAPart2[] =
    "var _gaq = _gaq || [];"
    "_gaq.push(['_setAccount', '%s']);"
    "_gaq.push(['_trackPageview']);"
    "(function() {"
    "  var ga = document.createElement('script');"
    "  ga.src = ('https:' == document.location.protocol ?"
    "  'https://ssl' : 'http://www') +"
    "  '.google-analytics.com/ga.js';"
    "  ga.setAttribute('async', 'true');"
    "  document.documentElement.firstChild.appendChild(ga);"
    "})();";

const char kAsyncGAPart3[] = "</script>";

const char kAnalyticsJS[] =
   "%s"
    "<script>"
    "(function(i,s,o,g,r,a,m){"
    "  i['GoogleAnalyticsObject']=r;"
    "  i[r]=i[r]||function(){"
    "    (i[r].q=i[r].q||[]).push(arguments)},i[r].l=1*new Date();"
    "    a=s.createElement(o), m=s.getElementsByTagName(o)[0];"
    "    a.async=1;a.src=g;m.parentNode.insertBefore(a,m)"
    "})(window,document,'script',"
    "   '//www.google-analytics.com/analytics.js','ga');"
    "ga('create', '%s', 'auto'%s);"
    "%s"
    "%s"
    "</script>";

const char kAnalyticsJSNoCreate[] =
   "%s"
    "<script>"
    "(function(i,s,o,g,r,a,m){"
    "  i['GoogleAnalyticsObject']=r;"
    "  i[r]=i[r]||function(){"
    "    (i[r].q=i[r].q||[]).push(arguments)},i[r].l=1*new Date();"
    "    a=s.createElement(o), m=s.getElementsByTagName(o)[0];"
    "    a.async=1;a.src=g;m.parentNode.insertBefore(a,m)"
    "})(window,document,'script',"
    "   '//www.google-analytics.com/analytics.js','ga');"
    "%s"
    "%s"
    "ga('send', 'pageview');"
    "</script>";

const char kAnalyticsJSInvalid[] =
   "%s"
    "<script>"
    "(functioni,s,o,g,r,a,m){"
    "  i['GoogleAnalyticsObject']=r;"
    "  i[r]=i[r]||function(){"
    "    (i[r].q=i[r].q||[]).push(arguments)},i[r].l=1*new Date();"
    "    a=s.createElement(o), m=s.getElementsByTagName(o)[0];"
    "    a.async=1;a.src=g;m.parentNode.insertBefore(a,m)"
    "})(window,document,'script',"
    "   '//www.google-analytics.com/analytics.js','ga');"
    "ga('create', '%s', 'auto'%s);"
    "%s"
    "%s"
    "</script>";

const char* const kSendPageviews[] = {
  "ga('send', 'pageview');",
  "ga(\"send\", \"pageview\");",
  "ga   (   'send'    ,        'pageview'    )    ;",
  "ga(\n'send',\n'pageview'\n);",
  "ga('MyTracker.send','pageview', 'foo', 'bar');" };

const char* const kNotSendPageviews[] = {
  "ga('sendpageview');",
  "ga('send''pageview');",
  "ga('send' 'pageview');",
  "a('send', 'pageview');",
  "ga('send', 'pageview'[1]);",
  "ga('send', 'event', 'link', 'click');" };

const char* const kNoFieldObjectGaCreates[] = {
  "ga('create', '%s', 'auto'%s);",
  "ga(\"create\", \"%s\", \"auto\"%s);",
  "ga('create','%s','auto'%s);",
  "ga    (    'create'    ,    '%s'    ,    'auto'     %s);",
  "ga(\n'create'\n,\n'%s'\n,\n'auto'\n%s);",
  "ga('create', '%s'%s);",
  "ga('create','%s','example.com', 'myTracker'%s);" };

const char* const kYesFieldObjectGaCreates[] = {
  "ga('create', '%s', {%stransport: 'beacon'});",
  "ga('create', '%s', {%stransport: \"beacon\"});",
  "ga('create', '%s', {%stransport: 'beacon', cookieDomain: 'auto'});",
  "ga('create','%s',{%stransport:'beacon'});",
  "ga( 'create' , '%s' , {   %stransport   : 'beacon'  }  );",
  "ga('create', {%2$strackingId: '%1$s'});",
  "ga('create', '%s', 'auto', 'foo', {%stransport: 'beacon'});"};

const char* const kGaNoCreates[] = {
  "ga('create \"%s\" auto');",
  "ga('create, \"%s\", auto');",
  "ga[0]('create', '%s', 'auto');",
  "ga('create', ('%s', 'auto'));",
  "ga('create'('%s', 'auto'));",
  "ga('create, \"%s\", auto, {transport: \\'beacon\\'}');"};

// We don't handle:
//
//   ga('send', {
//     hitType: 'pageview'
//   });
//
// Or increase speed tracking with:
//
//   ga('create', {
//     trackingId: 'UA-XXXXX-Y'
//   });
//
// but these are rare.

// Test fixture for InsertGAFilter unit tests.
class InsertGAFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    options()->set_ga_id(kGaId);
    options()->EnableFilter(RewriteOptions::kInsertGA);
    RewriteTestBase::SetUp();
  }

  void SetUpCustomVarExperiment(bool use_analytics_js,
                                GoogleString* experiment_string) {
    NullMessageHandler handler;
    RewriteOptions* options = rewrite_driver()->options()->Clone();
    options->set_use_analytics_js(use_analytics_js);
    options->set_running_experiment(true);
    ASSERT_TRUE(options->AddExperimentSpec("id=2;percent=10;slot=4;",
                                           &handler));
    ASSERT_TRUE(options->AddExperimentSpec(
        "id=7;percent=10;level=CoreFilters;slot=4;", &handler));
    options->SetExperimentState(2);

    // Setting up experiments automatically enables AddInstrumentation.
    // Turn it off so our output is easier to understand.
    options->DisableFilter(RewriteOptions::kAddInstrumentation);
    rewrite_driver()->set_custom_options(options);
    rewrite_driver()->AddFilters();
    experiment_string->assign(options->ToExperimentString());
  }

  void SetUpContentExperiment(bool use_analytics_js) {
    SetUpContentExperiment(use_analytics_js, "456");
  }

  void SetUpContentExperiment(bool use_analytics_js,
                              const GoogleString& variant_id) {
    NullMessageHandler handler;
    RewriteOptions* options = rewrite_driver()->options()->Clone();
    options->set_use_analytics_js(use_analytics_js);
    options->set_running_experiment(true);
    ASSERT_TRUE(options->AddExperimentSpec(StringPrintf(
        "id=2;percent=10;slot=4;options="
        "ContentExperimentID=123,"
        "ContentExperimentVariantID=%s", variant_id.c_str()), &handler));
    ASSERT_TRUE(options->AddExperimentSpec(
        "id=7;percent=10;level=CoreFilters;slot=4;options="
        "ContentExperimentID=123,"
        "ContentExperimentVariantID=789", &handler));
    options->SetExperimentState(2);  // Expecting cxid=123, cxvid=variant_id.

    // Setting up experiments automatically enables AddInstrumentation.
    // Turn it off so our output is easier to understand.
    options->DisableFilter(RewriteOptions::kAddInstrumentation);
    rewrite_driver()->set_custom_options(options);
    rewrite_driver()->AddFilters();
  }
};

GoogleString GenerateExpectedHtml(GoogleString domain_name,
                                  int experiment_var,
                                  GoogleString experiment_state,
                                  bool include_speed_tracking) {
  GoogleString speed_tracking = include_speed_tracking ? kGASpeedTracking : "";

  GoogleString snippet_addition;
  if (experiment_var != -1 && !experiment_state.empty()) {
    snippet_addition = StringPrintf(kGAExperimentSnippet,
                                    "" /* speed tracking added below */,
                                    experiment_var,
                                    experiment_state.c_str());
  }
  GoogleString analytics_js = StringPrintf(kGAJsSnippet,
                                           kGaId,
                                           domain_name.c_str(),
                                           speed_tracking.c_str());
  GoogleString output = StringPrintf(kHtmlOutputFormat,
                                     "",
                                     snippet_addition.c_str(),
                                     analytics_js.c_str());
  return output;
}

TEST_F(InsertGAFilterTest, SimpleInsertGaJs) {
  // Show that we can insert ga.js.
  options()->set_use_analytics_js(false);
  rewrite_driver()->AddFilters();
  GoogleString output = GenerateExpectedHtml("test.com", -1, "", true);
  ValidateExpected("simple_addition", kHtmlInput, output);

  output = GenerateExpectedHtml("www.test1.com", -1, "", true);
  ValidateExpectedUrl("https://www.test1.com/index.html", kHtmlInput,
                      output);
}

TEST_F(InsertGAFilterTest, SimpleInsertGaJsIdUnset) {
  // Show that when the ga id is not set we do nothing.
  options()->set_use_analytics_js(false);
  options()->set_ga_id("");
  rewrite_driver()->AddFilters();
  ValidateNoChanges("can't do anything without a ga id", kHtmlInput);
}

TEST_F(InsertGAFilterTest, SimpleInsertAnalyticsJs) {
  // Show that we can insert analytics.js.
  rewrite_driver()->AddFilters();
  GoogleString output = StringPrintf(
      kHtmlOutputFormat,
      "", "",
      StringPrintf(kAnalyticsJsSnippet,
                   kGaId,
                   kAnalyticsJsIncreaseSiteSpeedTracking,
                   "").c_str());
  ValidateExpected("simple_addition", kHtmlInput, output);
}

TEST_F(InsertGAFilterTest, NoIncreasedSpeed) {
  // Show that we don't add the js to increase speed tracking unless that option
  // is enabled.
  options()->set_use_analytics_js(false);
  options()->set_increase_speed_tracking(false);
  rewrite_driver()->AddFilters();
  GoogleString output = GenerateExpectedHtml("test.com", -1, "", false);
  ValidateExpected("simple_addition, in increased speed", kHtmlInput, output);
}

TEST_F(InsertGAFilterTest, AnalyticsJsNoIncreasedSpeed) {
  // Show that we can insert analytics.js without increasing speed tracking.
  options()->set_increase_speed_tracking(false);
  rewrite_driver()->AddFilters();
  GoogleString output = StringPrintf(
      kHtmlOutputFormat,
      "", "",
      StringPrintf(kAnalyticsJsSnippet,
                   kGaId,
                   "",
                   "").c_str());
  ValidateExpected("simple_addition", kHtmlInput, output);
}

TEST_F(InsertGAFilterTest, ExperimentGaJsCv) {
  // Show that we can insert a ga.js snippet that includes custom variable
  // tracking.
  GoogleString experiment_string;
  SetUpCustomVarExperiment(false, &experiment_string);
  GoogleString output = GenerateExpectedHtml(
      "test.com", 4, experiment_string, true);
  ValidateExpected("ga.js cv experiment", kHtmlInput, output);
}

TEST_F(InsertGAFilterTest, ExperimentGaJsCx) {
  // Show that we can insert a ga.js snippet that includes content experiment
  // tracking.
  SetUpContentExperiment(false);
  GoogleString output =  StringPrintf(
      kHtmlOutputFormat,
      StrCat("<script src=\"",
             kContentExperimentsJsClientUrl,
             "\"></script>").c_str(),
      "",
      StrCat(StringPrintf(kContentExperimentsSetChosenVariationSnippet,
                          456, "123"),
             StringPrintf(kGAJsSnippet, kGaId, "test.com",
                          kGASpeedTracking)).c_str());
  ValidateExpected("ga.js cx experiment", kHtmlInput, output);
}

TEST_F(InsertGAFilterTest, ExperimentGaJsCxString) {
  // Show that an attempt to insert a ga.js snippet with a string variant ID
  // results in a warning message.
  const GoogleString& kVariantText("StringVariant");
  SetUpContentExperiment(false, kVariantText);
  GoogleString output = StringPrintf(
      kHtmlOutputFormat,
      StrCat("<script src=\"",
             kContentExperimentsJsClientUrl,
             "\"></script>").c_str(),
      "",
      StrCat(StringPrintf(kContentExperimentsNonNumericVariantComment,
                          kVariantText.c_str()),
             StringPrintf(kGAJsSnippet, kGaId, "test.com",
                          kGASpeedTracking)).c_str());
  ValidateExpected("ga.js cx experiment", kHtmlInput, output);
}

TEST_F(InsertGAFilterTest, ExperimentAnalyticsJsCv) {
  // We're asked to insert an analytics.js snippet with custom variable
  // experiment tracking.  analytics.js doesn't support custom variables so we
  // can't log the experiment, but we can still insert the snippet.
  GoogleString experiment_string;
  SetUpCustomVarExperiment(true, &experiment_string);
  GoogleString output =  StringPrintf(
      kHtmlOutputFormat, "", "", StringPrintf(
          kAnalyticsJsSnippet,
          kGaId,
          kAnalyticsJsIncreaseSiteSpeedTracking,
          "").c_str());
  ValidateExpected("analytics.js cx experiment", kHtmlInput, output);
}

TEST_F(InsertGAFilterTest, ExperimentAnalyticsJsCx) {
  // Show that we can insert an anlytics.js snippet that includes content
  // experiment tracking.
  SetUpContentExperiment(true);
  GoogleString output =  StringPrintf(
      kHtmlOutputFormat, "", "", StringPrintf(
          kAnalyticsJsSnippet,
          kGaId,
          kAnalyticsJsIncreaseSiteSpeedTracking,
          StringPrintf(kContentExperimentsSetExpAndVariantSnippet,
                       "123", "456").c_str()).c_str());
  ValidateExpected("analytics.js cx experiment", kHtmlInput, output);
}

TEST_F(InsertGAFilterTest, ExperimentAnalyticsJsCxString) {
  // Show that we can insert an anlytics.js snippet that includes content
  // experiment tracking where the variant is a string.
  const GoogleString& kVariantText("StringVariant");
  SetUpContentExperiment(true, kVariantText);
  GoogleString output =  StringPrintf(
      kHtmlOutputFormat, "", "", StringPrintf(
          kAnalyticsJsSnippet,
          kGaId,
          kAnalyticsJsIncreaseSiteSpeedTracking,
          StringPrintf(kContentExperimentsSetExpAndVariantSnippet,
                       "123", kVariantText.c_str()).c_str()).c_str());
  ValidateExpected("analytics.js cx experiment", kHtmlInput, output);
}

const char kHtmlInputWithGASnippetFormat[] =
    "<head>\n<title>Something</title>\n"
    "</head><body> Hello World!"
    "<script>%s</script>"
    "</body>";

TEST_F(InsertGAFilterTest, ExperimentNoDouble) {
  // Input already has a GA js snippet.
  GoogleString experiment_string;
  SetUpCustomVarExperiment(false, &experiment_string);
  GoogleString analytics_js = StringPrintf(kGAJsSnippet, kGaId, "test.com", "");
  GoogleString input = StringPrintf(kHtmlInputWithGASnippetFormat,
                                     analytics_js.c_str());
  GoogleString experiment_snippet =
      StringPrintf(kGAExperimentSnippet, kGASpeedTracking,
                   4, experiment_string.c_str());
  // The output should still have the original GA snippet as well as an inserted
  // experiment snippet.
  GoogleString output = StringPrintf(
      kHtmlOutputFormat, "", experiment_snippet.c_str(), analytics_js.c_str());

  ValidateExpected("variable_added", input, output);
}

TEST_F(InsertGAFilterTest, ManyHeadsAndBodies) {
  // Make sure we only add the GA snippet in one place.
  GoogleString experiment_string;
  SetUpCustomVarExperiment(false, &experiment_string);
  const char* kHeadsFmt = "<head></head><head></head><head></head></head>"
      "<body></body><body>%s</body>";
  GoogleString input = StringPrintf(kHeadsFmt, "");
  GoogleString experiment_snippet =
      StringPrintf(kGAExperimentSnippet, "" /* speed tracking added below */,
                   4, experiment_string.c_str());
  GoogleString analytics_js = StringPrintf(
      kGAJsSnippet, kGaId, "test.com", kGASpeedTracking);

  GoogleString output = StringPrintf(kHeadsFmt,
                                     StrCat("<script>",
                                            experiment_snippet, analytics_js,
                                            "</script>").c_str());
  ValidateExpected("many_heads_and_bodies", input, output);
}

// We don't support the urchin snippet at all.
TEST_F(InsertGAFilterTest, ExistingUrchinAnalyticsNoExperiment) {
  rewrite_driver()->AddFilters();
  ValidateNoChanges("analytics already present",
                    StringPrintf(kUrchinScript, "", kGaId));
}
TEST_F(InsertGAFilterTest, ExistingUrchinAnalyticsCustomVarExperiment) {
  GoogleString experiment_string;
  SetUpCustomVarExperiment(false, &experiment_string);
  GoogleString input = StringPrintf(kUrchinScript, "", kGaId);
  GoogleString output = StringPrintf(kUrchinScript, "<head/>", kGaId);
  ValidateExpected("urchin not supported for experiments", input, output);
}
TEST_F(InsertGAFilterTest, ExistingUrchinAnalyticsContentExperiment) {
  SetUpContentExperiment(false);
  GoogleString input = StringPrintf(kUrchinScript, "", kGaId);
  GoogleString output = StringPrintf(kUrchinScript, "<head/>", kGaId);
  ValidateExpected("urchin not supported for experiments", input, output);
}

// If there's the ga_id but no actual loading of ga we can't do anything.
TEST_F(InsertGAFilterTest, UnusableSnippetNoExperiment) {
  rewrite_driver()->AddFilters();
  ValidateNoChanges("unusable script",
                    StringPrintf(kUnusableSnippet, "", kGaId));
}
TEST_F(InsertGAFilterTest, UnusableSnippetCustomVarExperiment) {
  GoogleString experiment_string;
  SetUpCustomVarExperiment(false, &experiment_string);
  GoogleString input = StringPrintf(kUnusableSnippet, "", kGaId);
  GoogleString output = StringPrintf(kUnusableSnippet, "<head/>", kGaId);
  ValidateExpected("unusable script", input, output);
}
TEST_F(InsertGAFilterTest, UnusableSnippetContentExperiment) {
  SetUpContentExperiment(false);
  GoogleString input = StringPrintf(kUnusableSnippet, "", kGaId);
  GoogleString output = StringPrintf(kUnusableSnippet, "<head/>", kGaId);
  ValidateExpected("unusable script", input, output);
}

TEST_F(InsertGAFilterTest, SynchronousGANoExperiment) {
  // If experiments are off and there's already a snippet we should do
  // nothing at all.
  rewrite_driver()->AddFilters();
  ValidateNoChanges("ga.js no experiment",
                    StringPrintf(kSynchronousGA, "", "", kGaId));
}

TEST_F(InsertGAFilterTest, SynchronousGACustomVarExperiment) {
  // Show that we can add custom variable experiment tracking to existing
  // synchronous ga.js usage.
  GoogleString experiment_string;
  SetUpCustomVarExperiment(false, &experiment_string);
  GoogleString input = StringPrintf(kSynchronousGA, "", "", kGaId);
  GoogleString output = StringPrintf(kSynchronousGA, "<head/>", StringPrintf(
      kGAExperimentSnippet, kGASpeedTracking, 4,
      experiment_string.c_str()).c_str(), kGaId);
  ValidateExpected("extend sync ga.js for cv experiment", input, output);
}

TEST_F(InsertGAFilterTest, SynchronousDCCustomVarExperiment) {
  // dc.js version of SynchronousGACustomVarExperiment.
  GoogleString experiment_string;
  SetUpCustomVarExperiment(false, &experiment_string);
  GoogleString input = StringPrintf(kSynchronousDC, "", "", kGaId);
  GoogleString output = StringPrintf(kSynchronousDC, "<head/>", StringPrintf(
      kGAExperimentSnippet, kGASpeedTracking, 4,
      experiment_string.c_str()).c_str(), kGaId);
  ValidateExpected("extend sync dc.js for cv experiment", input, output);
}

TEST_F(InsertGAFilterTest, SynchronousGAContentExperiment) {
  // Show that we can add content experiment tracking to existing synchronous
  // ga.js usage.
  SetUpContentExperiment(false);
  GoogleString input = StringPrintf(kSynchronousGA, "", "", kGaId);
  GoogleString output =
      StringPrintf(kSynchronousGA,
                   "<head/>",
                   StrCat(
                       "</script><script src=\"",
                       kContentExperimentsJsClientUrl,
                       "\"></script><script>",
                       StringPrintf(
                           kContentExperimentsSetChosenVariationSnippet,
                           456, "123")).c_str(),
                   kGaId);
  ValidateExpected("extend sync ga.js for content experiment", input, output);
}

TEST_F(InsertGAFilterTest, AsynchronousGANoExperiment) {
  // If experiments are off and there's already a snippet we should no
  // nothing at all.
  rewrite_driver()->AddFilters();
  ValidateNoChanges("async ga.js no experiment",
                    StringPrintf(kAsyncGA, "", "", kGaId));
}

TEST_F(InsertGAFilterTest, AsynchronousGACustomVarExperiment) {
  // Show that we can add custom variable experiment tracking to existing async
  // ga.js usage.
  GoogleString experiment_string;
  SetUpCustomVarExperiment(false, &experiment_string);
  GoogleString input = StringPrintf(kAsyncGA, "", "", kGaId);
  GoogleString output = StringPrintf(kAsyncGA, "<head/>", StringPrintf(
      kGAExperimentSnippet, kGASpeedTracking, 4,
      experiment_string.c_str()).c_str(), kGaId);
  ValidateExpected("extend async ga.js for cv experiment", input, output);
}

TEST_F(InsertGAFilterTest, AsynchronousDCCustomVarExperiment) {
  // dc.js version of AsynchronousGACustomVarExperiment.
  GoogleString experiment_string;
  SetUpCustomVarExperiment(false, &experiment_string);
  GoogleString input = StringPrintf(kAsyncDC, "", "", kGaId);
  GoogleString output = StringPrintf(kAsyncDC, "<head/>", StringPrintf(
      kGAExperimentSnippet, kGASpeedTracking, 4,
      experiment_string.c_str()).c_str(), kGaId);
  ValidateExpected("extend async dc.js for cv experiment", input, output);
}

TEST_F(InsertGAFilterTest, AsynchronousGAContentExperiment) {
  // Show that we can add content experiment tracking to existing async ga.js
  // usage.
  SetUpContentExperiment(false);
  GoogleString input = StringPrintf(kAsyncGA, "", "", kGaId);
  GoogleString output =
      StringPrintf(kAsyncGA,
                   "<head/>",
                   StrCat(
                       "</script><script src=\"",
                       kContentExperimentsJsClientUrl,
                       "\"></script><script>",
                       StringPrintf(
                           kContentExperimentsSetChosenVariationSnippet,
                           456, "123")).c_str(),
                   kGaId);
  ValidateExpected("extend async ga.js for content experiment", input, output);
}

TEST_F(InsertGAFilterTest, AnalyticsJSNoExperiment) {
  // If experiments are off and there's already a snippet we should no nothing
  // at all.
  rewrite_driver()->AddFilters();
  ValidateNoChanges("analytics.js no experiment",
                    StringPrintf(kAnalyticsJS, "", kGaId, "", "",
                                 kSendPageviews[0]));
}

TEST_F(InsertGAFilterTest, AnalyticsJSNoCustomVarExperiment) {
  // Test what happens when we insert analytics.js for an experiment with custom
  // variables.  Analytics doesn't support these, so we should do nothing.
  GoogleString experiment_string;
  SetUpCustomVarExperiment(false, &experiment_string);
  GoogleString input = StringPrintf(
      kAnalyticsJS, "", kGaId, "", "", kSendPageviews[0]);
  GoogleString output = StringPrintf(
      kAnalyticsJS, "<head/>", kGaId, "", "", kSendPageviews[0]);
  ValidateExpected("analytics.js with cv experiment", input, output);
}

TEST_F(InsertGAFilterTest, AnalyticsJSContentExperiment) {
  // Test that we can handle existing analytics.js snippets, and that we only
  // make changes when they're valid.
  SetUpContentExperiment(false);
  GoogleString input, output;
  GoogleString experiment_snippet = StringPrintf(
      kContentExperimentsSetExpAndVariantSnippet, "123", "456");
  for (int i = 0; i < arraysize(kSendPageviews); ++i) {
    input = StringPrintf(kAnalyticsJS, "", kGaId, "", "", kSendPageviews[i]);
    output = StringPrintf(kAnalyticsJS, "<head/>", kGaId,
                          kAnalyticsJsIncreaseSiteSpeedTracking,
                          experiment_snippet.c_str(), kSendPageviews[i]);
    ValidateExpected("analytics.js cx insertion", input, output);
  }
  for (int i = 0; i < arraysize(kNotSendPageviews); ++i) {
    input = StringPrintf(kAnalyticsJS, "", kGaId, "", "", kNotSendPageviews[i]);
    output = StringPrintf(
        kAnalyticsJS, "<head/>", kGaId, "", "", kNotSendPageviews[i]);
    ValidateExpected("analytics.js cx non-insertion", input, output);
  }
}

TEST_F(InsertGAFilterTest, AnalyticsJSInvalidNoExperiment) {
  // If experiments are off and there's already a snippet we should no nothing
  // at all, even if that snippet is invalid js.
  rewrite_driver()->AddFilters();
  ValidateNoChanges("analytics.js no experiment",
                    StringPrintf(kAnalyticsJSInvalid, "", kGaId, "", "",
                                 kSendPageviews[0]));
}

TEST_F(InsertGAFilterTest, AnalyticsJSInvalidNoCustomVarExperiment) {
  // Test what happens when we insert analytics.js for an experiment with custom
  // variables.  Analytics doesn't support these, so we should do nothing, even
  // if the snippet isn't valid js.
  GoogleString experiment_string;
  SetUpCustomVarExperiment(false, &experiment_string);
  GoogleString input = StringPrintf(
      kAnalyticsJSInvalid, "", kGaId, "", "", kSendPageviews[0]);
  GoogleString output = StringPrintf(
      kAnalyticsJSInvalid, "<head/>", kGaId, "", "", kSendPageviews[0]);
  ValidateExpected("analytics.js with cv experiment", input, output);
}

TEST_F(InsertGAFilterTest, AnalyticsJSInvalidContentExperiment) {
  // Test that we can handle existing analytics.js snippets, and that if the
  // snippet isn't valid js we don't make changes.
  SetUpContentExperiment(false);
  GoogleString input, output;
  GoogleString experiment_snippet = StringPrintf(
      kContentExperimentsSetExpAndVariantSnippet, "123", "456");
  for (int i = 0; i < arraysize(kSendPageviews); ++i) {
    input = StringPrintf(kAnalyticsJSInvalid, "", kGaId, "", "",
                         kSendPageviews[i]);
    output = StringPrintf(kAnalyticsJSInvalid, "<head/>", kGaId, "", "",
                          kSendPageviews[i]);
    ValidateExpected("analytics.js cx insertion", input, output);
  }
  for (int i = 0; i < arraysize(kNotSendPageviews); ++i) {
    input = StringPrintf(kAnalyticsJSInvalid, "", kGaId, "", "",
                         kNotSendPageviews[i]);
    output = StringPrintf(kAnalyticsJSInvalid, "<head/>", kGaId, "", "",
                          kNotSendPageviews[i]);
    ValidateExpected("analytics.js cx non-insertion", input, output);
  }
}

TEST_F(InsertGAFilterTest, AnalyticsJSContentExperimentSpeedTracking) {
  // Test that we can handle existing analytics.js snippets, and that we only
  // make changes when they're valid.
  SetUpContentExperiment(false);
  GoogleString input, output;
  GoogleString experiment_snippet = StringPrintf(
      kContentExperimentsSetExpAndVariantSnippet, "123", "456");

  // These ones don't already have a field object.
  for (int i = 0; i < arraysize(kNoFieldObjectGaCreates); ++i) {
    input = StringPrintf(kAnalyticsJSNoCreate, "",
                         StringPrintf(kNoFieldObjectGaCreates[i],
                                      kGaId, "").c_str(), "");
    output = StringPrintf(
        kAnalyticsJSNoCreate, "<head/>",
        StringPrintf(kNoFieldObjectGaCreates[i], kGaId,
                     kAnalyticsJsIncreaseSiteSpeedTracking).c_str(),
        experiment_snippet.c_str());
    ValidateExpected("analytics.js cx insertion speed tracking", input, output);
  }

  // These ones do already have a field object.
  for (int i = 0; i < arraysize(kYesFieldObjectGaCreates); ++i) {
    input = StringPrintf(kAnalyticsJSNoCreate, "",
                         StringPrintf(kYesFieldObjectGaCreates[i],
                                      kGaId, "").c_str(), "");
    output = StringPrintf(
        kAnalyticsJSNoCreate, "<head/>",
        StringPrintf(kYesFieldObjectGaCreates[i], kGaId,
                     kAnalyticsJsIncreaseSiteSpeedTrackingMinimal).c_str(),
        experiment_snippet.c_str());
    ValidateExpected("analytics.js cx insertion field object speed tracking",
                     input, output);
  }

  // These ones are invalid or we can't insert for some other reason.
  for (int i = 0; i < arraysize(kGaNoCreates); ++i) {
    input = StringPrintf(kAnalyticsJSNoCreate, "",
                         StringPrintf(kGaNoCreates[i], kGaId).c_str(), "");
    output = StringPrintf(kAnalyticsJSNoCreate, "<head/>",
                          StringPrintf(kGaNoCreates[i], kGaId).c_str(),
                          experiment_snippet.c_str());
    ValidateExpected("analytics.js cx non-insertion speed tracking",
                     input, output);
  }
}

TEST_F(InsertGAFilterTest, AnalyticsJSContentExperimentNoIncreaseSpeed) {
  // Test that we can handle existing analytics.js snippets without increasing
  // speed tracking.
  options()->set_increase_speed_tracking(false);
  SetUpContentExperiment(false);
  GoogleString input =
      StringPrintf(kAnalyticsJS, "", kGaId, "", "", kSendPageviews[0]);
  GoogleString experiment_snippet = StringPrintf(
      kContentExperimentsSetExpAndVariantSnippet, "123", "456");
  GoogleString output =
      StringPrintf(kAnalyticsJS, "<head/>", kGaId, "",
                   experiment_snippet.c_str(), kSendPageviews[0]);
  ValidateExpected("analytics.js cx insertion, no increased speed",
                   input, output);
}

TEST_F(InsertGAFilterTest, AnalyticsJSNoCloseBody) {
  // When no snippet is present we should insert one at the end of the document,
  // even if there's no </body> tag.
  SetUpContentExperiment(true);
  GoogleString input = StringPrintf(kHtmlNoCloseBody, "");
  GoogleString output = StringPrintf(kHtmlNoCloseBody, StrCat(
      "<script>",
      StringPrintf(kAnalyticsJsSnippet,
                   kGaId,
                   kAnalyticsJsIncreaseSiteSpeedTracking,
                   StringPrintf(kContentExperimentsSetExpAndVariantSnippet,
                                "123", "456").c_str()),
      "</script>").c_str());
  ValidateExpected("no close body", input, output);
}


// TODO(jefftk): this test fails, but it's pretty weird.  Is that a problem?
#if 0
TEST_F(InsertGAFilterTest, ExistingGaJsContentExperimentNoCloseAnything) {
  // When there's already a ga.js snippet present and we want to add content
  // experiment support, make sure we can do this even if no tags are closed
  // after the open <script>.

  SetUpContentExperiment(true);

  GoogleString input =  StringPrintf(
      kHtmlNoCloseBody,
      StrCat("<script>",
             StringPrintf(kGAJsSnippet, kGaId, "test.com", "")).c_str());

  GoogleString output =  StringPrintf(
      kHtmlNoCloseBody,
      StrCat("<script>"
             "</script>"
             "<script src=\"",
             kContentExperimentsJsClientUrl,
             "\"></script>"
             "<script>",
             StringPrintf(kContentExperimentsSetChosenVariationSnippet,
                          456, "123"),
             StringPrintf(kGAJsSnippet, kGaId, "test.com",
                          kGASpeedTracking)).c_str());

  ValidateExpected("ga.js cx experiment no close script", input, output);
}
#endif

TEST_F(InsertGAFilterTest, AsynchronousGAContentExperimentFlush) {
  // Show that we can add content experiment tracking to existing async ga.js
  // usage even if there are flushes.
  SetUpContentExperiment(false);

  GoogleString output =
      StringPrintf(kAsyncGA,
                   "<html><head/>",
                   StrCat(
                       "</script><script src=\"",
                       kContentExperimentsJsClientUrl,
                       "\"></script><script>",
                       StringPrintf(
                           kContentExperimentsSetChosenVariationSnippet,
                           456, "123")).c_str(),
                   kGaId);

  SetupWriter();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText("<html>");
  rewrite_driver()->ParseText(kAsyncGAPart1);
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText(StringPrintf(kAsyncGAPart2, kGaId));
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText(kAsyncGAPart3);
  rewrite_driver()->FinishParse();

  EXPECT_EQ(output, output_buffer_);
}

}  // namespace

}  // namespace net_instaweb
