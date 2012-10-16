/*
 * Copyright 2012 Google Inc.
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

// Author: guptaa@google.com (Ashish Gupta)

#include "net/instaweb/rewriter/public/static_javascript_manager.h"

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const char kHtml[] = "<body><br></body>";
const char kScript[] = "alert('foo');";

class StaticJavascriptManagerTest : public RewriteTestBase {
 protected:
  StaticJavascriptManagerTest() {
    url_namer_.set_proxy_domain("http://proxy-domain");
  }

  virtual void SetUp() {
    RewriteTestBase::SetUp();
  }

  // Helper filters to help test inserting of static JS.
  class AddStaticJsBeforeBr: public EmptyHtmlFilter {
   public:
    explicit AddStaticJsBeforeBr(RewriteDriver* driver) : driver_(driver) {
    }

    virtual void EndElement(HtmlElement* element) {
      if (element->keyword() == HtmlName::kBr) {
        HtmlElement* script = driver_->NewElement(element->parent(),
                                                  HtmlName::kScript);
        driver_->InsertElementBeforeElement(element, script);
        driver_->server_context()->static_javascript_manager()->
            AddJsToElement(kScript, script, driver_);
      }
    }
    virtual const char* Name() const { return "AddStaticJsBeforeBr"; }
   private:
    RewriteDriver* driver_;
    DISALLOW_COPY_AND_ASSIGN(AddStaticJsBeforeBr);
  };

  UrlNamer url_namer_;
};

TEST_F(StaticJavascriptManagerTest, TestBlinkHandler) {
  StaticJavascriptManager manager(&url_namer_, false, "");
  const char blink_url[] = "http://proxy-domain/psajs/blink.js";
  EXPECT_STREQ(blink_url, manager.GetBlinkJsUrl(options_));
}

TEST_F(StaticJavascriptManagerTest, TestBlinkGstatic) {
  StaticJavascriptManager manager(&url_namer_, true, "1");
  const char blink_url[] = "http://www.gstatic.com/psa/static/1-blink.js";
  EXPECT_STREQ(blink_url, manager.GetBlinkJsUrl(options_));
}

TEST_F(StaticJavascriptManagerTest, TestBlinkDebug) {
  StaticJavascriptManager manager(&url_namer_, true, "1");
  options_->EnableFilter(RewriteOptions::kDebug);
  const char blink_url[] = "http://proxy-domain/psajs/blink.js";
  EXPECT_STREQ(blink_url, manager.GetBlinkJsUrl(options_));
}

TEST_F(StaticJavascriptManagerTest, TestJsDebug) {
  StaticJavascriptManager manager(&url_namer_, true, "1");
  options_->EnableFilter(RewriteOptions::kDebug);
  for (int i = 0;
       i < static_cast<int>(StaticJavascriptManager::kEndOfModules);
       ++i) {
    StaticJavascriptManager::JsModule module =
        static_cast<StaticJavascriptManager::JsModule>(i);
    GoogleString script(manager.GetJsSnippet(module, options_));
    EXPECT_NE(GoogleString::npos, script.find("/*"))
        << "There should be some comments in the debug code";
  }
}

TEST_F(StaticJavascriptManagerTest, TestJsOpt) {
  StaticJavascriptManager manager(&url_namer_, true, "1");
  for (int i = 0;
       i < static_cast<int>(StaticJavascriptManager::kEndOfModules);
       ++i) {
    StaticJavascriptManager::JsModule module =
        static_cast<StaticJavascriptManager::JsModule>(i);
    GoogleString script(manager.GetJsSnippet(module, options_));
    EXPECT_EQ(GoogleString::npos, script.find("/*"))
        << "There should be no comments in the compiled code";
  }
}

TEST_F(StaticJavascriptManagerTest, TestHtmlInsertInlineJs) {
  SetHtmlMimetype();
  AddStaticJsBeforeBr filter(rewrite_driver());
  rewrite_driver()->AddFilter(&filter);
  ParseUrl(kTestDomain, kHtml);
  EXPECT_EQ("<html>\n<body><script type=\"text/javascript\">alert('foo');"
            "</script><br></body>\n</html>", output_buffer_);
}

TEST_F(StaticJavascriptManagerTest, TestXhtmlInsertInlineJs) {
  SetXhtmlMimetype();
  AddStaticJsBeforeBr filter(rewrite_driver());
  rewrite_driver()->AddFilter(&filter);
  ParseUrl(kTestDomain, kHtml);
  EXPECT_EQ("<html>\n<body><script type=\"text/javascript\">//<![CDATA[\n"
            "alert('foo');\n//]]></script><br></body>\n</html>",
            output_buffer_);
}

TEST_F(StaticJavascriptManagerTest, TestHtml5InsertInlineJs) {
  SetHtmlMimetype();
  AddStaticJsBeforeBr filter(rewrite_driver());
  rewrite_driver()->AddFilter(&filter);
  GoogleString html = StrCat("<!DOCTYPE html>", kHtml);
  ParseUrl(kTestDomain, html);
  EXPECT_EQ("<html>\n<!DOCTYPE html><body><script>alert('foo');"
            "</script><br></body>\n</html>", output_buffer_);
}

}  // namespace

}  // namespace net_instaweb
