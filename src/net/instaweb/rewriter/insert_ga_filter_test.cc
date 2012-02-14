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
  GoogleString format_output = StringPrintf(kHtmlOutputFormat, kGASnippet);
  GoogleString output = StringPrintf(format_output.c_str(), kGaId,
                                     kGASpeedTracking);
  ValidateExpected("simple_addition", kHtmlInput, output);
  ValidateNoChanges("already_there", output);
}

const char kHtmlOutsideHead[] =
    "<head>\n<title>Something</title>\n"
    "</head>\n"
    "<script type=\"text/javascript\">%s</script>"
    "<body> Hello World!</body>";

TEST_F(InsertGAFilterTest, no_double) {
  rewrite_driver()->AddFilters();
  GoogleString format_html = StringPrintf(kHtmlOutsideHead, kGASnippet);
  ValidateNoChanges("outside_head", StringPrintf(format_html.c_str(), kGaId,
                                                 kGASpeedTracking));
}

TEST_F(InsertGAFilterTest, no_increased_speed) {
  options()->set_increase_speed_tracking(false);
  rewrite_driver()->AddFilters();

  GoogleString format_output = StringPrintf(kHtmlOutputFormat, kGASnippet);
  GoogleString output = StringPrintf(format_output.c_str(), kGaId, "");

  ValidateExpected("simple_addition", kHtmlInput, output);
  ValidateNoChanges("already_there", output);
}

}  // namespace

}  // namespace net_instaweb
