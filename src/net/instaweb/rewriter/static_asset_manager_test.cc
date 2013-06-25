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

#include "net/instaweb/rewriter/public/static_asset_manager.h"

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
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const char kHtml[] = "<body><br></body>";
const char kScript[] = "alert('foo');";

class StaticAssetManagerTest : public RewriteTestBase {
 protected:
  StaticAssetManagerTest() {
    url_namer_.set_proxy_domain("http://proxy-domain");
    manager_.reset(new StaticAssetManager(&url_namer_,
                                          server_context()->hasher(),
                                          server_context()->message_handler()));
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
        driver_->InsertNodeBeforeNode(element, script);
        driver_->server_context()->static_asset_manager()->
            AddJsToElement(kScript, script, driver_);
      }
    }
    virtual const char* Name() const { return "AddStaticJsBeforeBr"; }
   private:
    RewriteDriver* driver_;
    DISALLOW_COPY_AND_ASSIGN(AddStaticJsBeforeBr);
  };

  scoped_ptr<StaticAssetManager> manager_;
  UrlNamer url_namer_;
};

TEST_F(StaticAssetManagerTest, TestBlinkHandler) {
  const char blink_url[] = "http://proxy-domain/psajs/blink.0.js";
  EXPECT_STREQ(blink_url, manager_->GetAssetUrl(StaticAssetManager::kBlinkJs,
                                                options_));
}

TEST_F(StaticAssetManagerTest, TestBlinkGstatic) {
  manager_->set_serve_asset_from_gstatic(true);
  manager_->set_gstatic_hash(
      StaticAssetManager::kBlinkJs, StaticAssetManager::kGStaticBase, "1");
  const char blink_url[] =
      "//www.gstatic.com/psa/static/1-blink.js";
  EXPECT_STREQ(blink_url, manager_->GetAssetUrl(StaticAssetManager::kBlinkJs,
                                                options_));
}

TEST_F(StaticAssetManagerTest, TestBlinkDebug) {
  manager_->set_serve_asset_from_gstatic(true);
  manager_->set_gstatic_hash(
      StaticAssetManager::kBlinkJs, StaticAssetManager::kGStaticBase, "1");
  options_->EnableFilter(RewriteOptions::kDebug);
  const char blink_url[] = "http://proxy-domain/psajs/blink_debug.0.js";
  EXPECT_STREQ(blink_url, manager_->GetAssetUrl(StaticAssetManager::kBlinkJs,
                                                options_));
}

TEST_F(StaticAssetManagerTest, TestDeferJsGstatic) {
  manager_->set_serve_asset_from_gstatic(true);
  manager_->set_gstatic_hash(
      StaticAssetManager::kDeferJs, StaticAssetManager::kGStaticBase, "1");
  const char defer_js_url[] =
      "//www.gstatic.com/psa/static/1-js_defer.js";
  EXPECT_STREQ(defer_js_url, manager_->GetAssetUrl(
      StaticAssetManager::kDeferJs, options_));
}

TEST_F(StaticAssetManagerTest, TestDeferJsDebug) {
  manager_->set_serve_asset_from_gstatic(true);
  manager_->set_gstatic_hash(
      StaticAssetManager::kDeferJs, StaticAssetManager::kGStaticBase, "1");
  options_->EnableFilter(RewriteOptions::kDebug);
  const char defer_js_debug_url[] =
      "http://proxy-domain/psajs/js_defer_debug.0.js";
  EXPECT_STREQ(defer_js_debug_url, manager_->GetAssetUrl(
      StaticAssetManager::kDeferJs, options_));
}

TEST_F(StaticAssetManagerTest, TestDeferJsNonGStatic) {
  const char defer_js_url[] =
      "http://proxy-domain/psajs/js_defer.0.js";
  EXPECT_STREQ(defer_js_url, manager_->GetAssetUrl(
      StaticAssetManager::kDeferJs, options_));
}

TEST_F(StaticAssetManagerTest, TestJsDebug) {
  options_->EnableFilter(RewriteOptions::kDebug);
  for (int i = 0;
       i < static_cast<int>(StaticAssetManager::kEndOfModules);
       ++i) {
    StaticAssetManager::StaticAsset module =
        static_cast<StaticAssetManager::StaticAsset>(i);
    GoogleString script(manager_->GetAsset(module, options_));
    if (module != StaticAssetManager::kBlankGif &&
        module != StaticAssetManager::kBlinkJs &&
        module != StaticAssetManager::kGhostClickBusterJs) {
      EXPECT_NE(GoogleString::npos, script.find("/*"))
          << "There should be some comments in the debug code";
    }
  }
}

TEST_F(StaticAssetManagerTest, TestJsOpt) {
  for (int i = 0;
       i < static_cast<int>(StaticAssetManager::kEndOfModules);
       ++i) {
    StaticAssetManager::StaticAsset module =
        static_cast<StaticAssetManager::StaticAsset>(i);
    GoogleString script(manager_->GetAsset(module, options_));
    EXPECT_EQ(GoogleString::npos, script.find("/*"))
        << "There should be no comments in the compiled code";
  }
}

TEST_F(StaticAssetManagerTest, TestHtmlInsertInlineJs) {
  SetHtmlMimetype();
  AddStaticJsBeforeBr filter(rewrite_driver());
  rewrite_driver()->AddFilter(&filter);
  ParseUrl(kTestDomain, kHtml);
  EXPECT_EQ("<html>\n<body><script type=\"text/javascript\">alert('foo');"
            "</script><br></body>\n</html>", output_buffer_);
}

TEST_F(StaticAssetManagerTest, TestXhtmlInsertInlineJs) {
  SetXhtmlMimetype();
  AddStaticJsBeforeBr filter(rewrite_driver());
  rewrite_driver()->AddFilter(&filter);
  ParseUrl(kTestDomain, kHtml);
  EXPECT_EQ("<html>\n<body><script type=\"text/javascript\">//<![CDATA[\n"
            "alert('foo');\n//]]></script><br></body>\n</html>",
            output_buffer_);
}

TEST_F(StaticAssetManagerTest, TestHtml5InsertInlineJs) {
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
