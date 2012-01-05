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

static const char kGaId[] = "UA-21111111-1";

// Test fixture for InsertGAFilter unit tests.
class InsertGAFilterTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    options()->set_ga_id(kGaId);
    options()->EnableFilter(RewriteOptions::kInsertGA);
    ResourceManagerTestBase::SetUp();
    rewrite_driver()->AddFilters();
  }
};

static const char html_input[] =
    "<head>\n"
    "<title>Something</title>\n"
    "</head>"
    "<body> Hello World!</body>";

static const char html_output_format[] =
    "<head>\n<title>Something</title>\n"
    "<script type=\"text/javascript\">var _gaq = _gaq || [];\n"
    "_gaq.push(['_setAccount', '%s']);\n"
    "_gaq.push(['_trackPageview']);\n"
    "(function() {\n"
    "var ga = document.createElement('script'); ga.type = 'text/javascript'; "
    "ga.async = true;\n"
    "ga.src = ('https:' == document.location.protocol ? "
    "'https://ssl' : 'http://www') + '.google-analytics.com/ga.js';\n"
    "var s = document.getElementsByTagName('script')[0]; "
    "s.parentNode.insertBefore(ga, s);\n"
    "})();</script></head><body> Hello World!</body>";

TEST_F(InsertGAFilterTest, simple_insert) {
  GoogleString output = StringPrintf(html_output_format, kGaId);
  ValidateExpected("simple_addition", html_input, output);
  ValidateNoChanges("already_there", output);
}

static const char html_outside_head[] =
    "<head>\n<title>Something</title>\n"
    "</head>\n"
    "<script type=\"text/javascript\">var _gaq = _gaq || [];\n"
    "_gaq.push(['_setAccount', '%s']);\n"
    "_gaq.push(['_trackPageview']);\n"
    "(function() {\n"
    "var ga = document.createElement('script'); ga.type = 'text/javascript'; "
    "ga.async = true;\n"
    "ga.src = ('https:' == document.location.protocol ? "
    "'https://ssl' : 'http://www') + '.google-analytics.com/ga.js';\n"
    "var s = document.getElementsByTagName('script')[0]; "
    "s.parentNode.insertBefore(ga, s);\n"
    "})();</script><body> Hello World!</body>";

TEST_F(InsertGAFilterTest, no_double) {
  ValidateNoChanges("outside_head", StringPrintf(html_outside_head, kGaId));
}

}  // namespace

}  // namespace net_instaweb
