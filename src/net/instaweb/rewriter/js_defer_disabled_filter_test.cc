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

// Author: atulvasu@google.com (Atul Vasu)

#include "net/instaweb/rewriter/public/js_defer_disabled_filter.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

const char * kDeferJsCodeNonGStatic = "<script type=\"text/javascript\" "
    "src=\"/psajs/js_defer.0.js\"></script>";

class JsDeferDisabledFilterTest : public RewriteTestBase {
 protected:
  // TODO(matterbury): Delete this method as it should be redundant.
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    SetHtmlMimetype();  // Prevent insertion of CDATA tags to static JS.
  }

  virtual void InitJsDeferDisabledFilter(bool debug) {
    if (debug) {
      options()->EnableFilter(RewriteOptions::kDebug);
    }
    js_defer_disabled_filter_.reset(
        new JsDeferDisabledFilter(rewrite_driver()));
    rewrite_driver()->AddFilter(js_defer_disabled_filter_.get());
  }

  virtual bool AddBody() const { return false; }

  scoped_ptr<JsDeferDisabledFilter> js_defer_disabled_filter_;
};

TEST_F(JsDeferDisabledFilterTest, DeferScript) {
  InitJsDeferDisabledFilter(false);
  ValidateExpected("defer_script",
      "<head>"
      "<script type='text/psajs' "
      "src='http://www.google.com/javascript/ajax_apis.js'></script>"
      "<script type='text/psajs'"
      "> func();</script>"
      "</head><body>Hello, world!</body>",
      StrCat("<head>"
             "<script type='text/psajs' "
             "src='http://www.google.com/javascript/ajax_apis.js'></script>"
             "<script type='text/psajs'"
             "> func();</script>",
             kDeferJsCodeNonGStatic,
             "<script type=\"text/javascript\">",
             JsDeferDisabledFilter::kSuffix,
             "</script></head><body>Hello, world!"
             "</body>"));
}

TEST_F(JsDeferDisabledFilterTest, DeferScriptMultiBody) {
  InitJsDeferDisabledFilter(false);
  ValidateExpected("defer_script_multi_body",
      "<head>"
      "<script type='text/psajs' "
      "src='http://www.google.com/javascript/ajax_apis.js'></script>"
      "<script type='text/psajs'> func(); </script>"
      "</head><body>Hello, world!</body><body>"
      "<script type='text/psajs'> func2(); </script></body>",
      StrCat("<head>"
             "<script type='text/psajs' "
             "src='http://www.google.com/javascript/ajax_apis.js'></script>"
             "<script type='text/psajs'> func(); </script>",
             kDeferJsCodeNonGStatic,
             "<script type=\"text/javascript\">",
             JsDeferDisabledFilter::kSuffix,
             "</script></head><body>Hello, world!"
             "</body><body><script type='text/psajs'> func2(); "
             "</script></body>"));
}

TEST_F(JsDeferDisabledFilterTest, DeferScriptNoHead) {
  InitJsDeferDisabledFilter(false);
  ValidateExpected("defer_script_no_head",
      "<body>Hello, world!</body><body>"
      "<script type='text/psajs'> func2(); </script></body>",
      StrCat("<head>",
             kDeferJsCodeNonGStatic,
             "<script type=\"text/javascript\">",
             JsDeferDisabledFilter::kSuffix,
             "</script></head><body>Hello, world!"
             "</body><body><script type='text/psajs'> func2(); "
             "</script></body>"));
}

TEST_F(JsDeferDisabledFilterTest, DeferScriptOptimized) {
  InitJsDeferDisabledFilter(false);
  Parse("optimized",
        "<body><script type='text/psajs' src='foo.js'></script></body>");
  EXPECT_NE(GoogleString::npos, output_buffer_.find("js_defer.0.js"))
      << "js_defer.js should have been included";
}

TEST_F(JsDeferDisabledFilterTest, DeferScriptDebug) {
  InitJsDeferDisabledFilter(true);
  Parse("optimized",
        "<head></head><body><script type='text/psajs' src='foo.js'>"
        "</script></body>");
  EXPECT_NE(GoogleString::npos, output_buffer_.find("js_defer_debug.0.js"))
      << "js_defer_debug.js should have been included";
}

TEST_F(JsDeferDisabledFilterTest, InvalidUserAgent) {
  InitJsDeferDisabledFilter(false);
  rewrite_driver()->set_user_agent("BlackListUserAgent");
  const char script[] = "<head>"
      "<script type='text/psajs' "
      "src='http://www.google.com/javascript/ajax_apis.js'></script>"
      "<script type='text/psajs'"
      "> func();</script>"
      "</head><body>Hello, world!</body>";

  ValidateNoChanges("defer_script", script);
}

TEST_F(JsDeferDisabledFilterTest, TestDeferJsUrlFromGStatic) {
  UrlNamer url_namer;
  StaticJavascriptManager js_manager(&url_namer, server_context()->hasher(),
                                     server_context()->message_handler());
  js_manager.set_serve_js_from_gstatic(true);
  js_manager.set_gstatic_defer_js_hash("1");

  server_context()->set_static_javascript_manager(&js_manager);

  InitJsDeferDisabledFilter(false);
  ValidateExpected(
      "defer_script_url",
      "<body>Hello, world!</body><body>"
      "<script type='text/psajs'> func2(); </script></body>",
      StrCat("<head>"
             "<script type=\"text/javascript\" "
             "src=\"http://www.gstatic.com/psa/static/1-js_defer.js\"></script>"
             "<script type=\"text/javascript\">",
             JsDeferDisabledFilter::kSuffix,
             "</script></head><body>Hello, world!"
             "</body><body><script type='text/psajs'> func2(); "
             "</script></body>"));
}

TEST_F(JsDeferDisabledFilterTest, TestDeferJsUrlFromNonGStatic) {
  InitJsDeferDisabledFilter(false);

  ValidateExpected(
      "defer_script_url",
      "<body>Hello, world!</body><body>"
      "<script type='text/psajs'> func2(); </script></body>",
      StrCat("<head>",
             kDeferJsCodeNonGStatic,
             "<script type=\"text/javascript\">",
             JsDeferDisabledFilter::kSuffix,
             "</script></head><body>Hello, world!"
             "</body><body><script type='text/psajs'> func2(); "
             "</script></body>"));
}

}  // namespace net_instaweb
