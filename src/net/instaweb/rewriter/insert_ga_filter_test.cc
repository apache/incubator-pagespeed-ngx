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

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const char kGaId[] = "UA-21111111-1";

// Test fixture for InsertGAFilter unit tests.
class InsertGAFilterTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    options()->set_ga_id(kGaId);
    options()->EnableFilter(RewriteOptions::kInsertGA);
    ResourceManagerTestBase::SetUp();
  }
};

const char kHtmlInput[] =
    "<head>\n"
    "<title>Something</title>\n"
    "</head>"
    "<body> Hello World!</body>";

const char kHtmlOutputFormat[] =
    "<head>\n<title>Something</title>\n"
    "<script type=\"text/javascript\">%s</script>"
    "</head><body> Hello World!</body>";

TEST_F(InsertGAFilterTest, simple_insert) {
  rewrite_driver()->AddFilters();
  GoogleString ga_snippet = StringPrintf(kGASnippet, kGaId, "test.com",
                                         kGASpeedTracking, "", "http://www");
  GoogleString output = StringPrintf(kHtmlOutputFormat, ga_snippet.c_str());
  ValidateExpected("simple_addition", kHtmlInput, output);
  ValidateNoChanges("already_there", output);

  ga_snippet = StringPrintf(kGASnippet, kGaId, "www.test1.com",
                            kGASpeedTracking, "", "https://ssl");
  output = StringPrintf(kHtmlOutputFormat, ga_snippet.c_str());
  ValidateExpectedUrl("https://www.test1.com/index.html", kHtmlInput,
                      output);
}

const char kHtmlOutsideHead[] =
    "<head>\n<title>Something</title>\n"
    "</head>\n"
    "<script type=\"text/javascript\">%s</script>"
    "<body> Hello World!</body>";

TEST_F(InsertGAFilterTest, no_double) {
  rewrite_driver()->AddFilters();
  GoogleString ga_snippet = StringPrintf(kGASnippet, kGaId, "test.com",
                                         kGASpeedTracking, "", "http://www");
  ValidateNoChanges("outside_head", StringPrintf(kHtmlOutsideHead,
                                                 ga_snippet.c_str()));
}

TEST_F(InsertGAFilterTest, no_increased_speed) {
  options()->set_increase_speed_tracking(false);
  rewrite_driver()->AddFilters();

  GoogleString ga_snippet = StringPrintf(kGASnippet, kGaId, "test.com",
                                         "", "", "http://www");
  GoogleString output = StringPrintf(kHtmlOutputFormat, ga_snippet.c_str());

  ValidateExpected("simple_addition", kHtmlInput, output);
  ValidateNoChanges("already_there", output);
}

TEST_F(InsertGAFilterTest, furious) {
  NullMessageHandler handler;
  RewriteOptions* options = rewrite_driver()->options()->Clone();
  options->set_running_furious_experiment(true);
  options->AddFuriousSpec("id=2", &handler);
  options->AddFuriousSpec("id=7;level=CoreFilters", &handler);
  options->SetFuriousState(2);

  // Setting up Furious automatically enables AddInstrumentation.
  // Turn it off so our output is easier to understand.
  options->DisableFilter(RewriteOptions::kAddInstrumentation);
  rewrite_driver()->set_custom_options(options);
  rewrite_driver()->AddFilters();

  GoogleString variable_value = StringPrintf(
      "_gaq.push(['_setCustomVar', 1, 'FuriousState', '%s', 2])",
      options->ToExperimentString().c_str());
  GoogleString ga_snippet = StringPrintf(
      kGASnippet, kGaId, "test.com", kGASpeedTracking,
      variable_value.c_str(), "http://www");
  GoogleString output = StringPrintf(kHtmlOutputFormat, ga_snippet.c_str());

  ValidateExpected("simple_addition", kHtmlInput, output);
}

}  // namespace

}  // namespace net_instaweb
