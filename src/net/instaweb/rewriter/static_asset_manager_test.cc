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

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/empty_html_filter.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/http/content_type.h"

namespace net_instaweb {

namespace {

const char kHtml[] = "<body><br></body>";
const char kScript[] = "alert('foo');";

class StaticAssetManagerTest : public RewriteTestBase {
 protected:
  StaticAssetManagerTest() {
    manager_.reset(new StaticAssetManager("http://proxy-domain",
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
};

TEST_F(StaticAssetManagerTest, TestBlinkHandler) {
  const char blink_url[] = "http://proxy-domain/psajs/blink.0.js";
  EXPECT_STREQ(blink_url, manager_->GetAssetUrl(StaticAssetEnum::BLINK_JS,
                                                options_));
}

TEST_F(StaticAssetManagerTest, TestBlinkGstatic) {
  manager_->set_static_asset_base("http://proxy-domain");
  manager_->set_serve_asset_from_gstatic(true);
  manager_->set_gstatic_hash(
      StaticAssetEnum::BLINK_JS, StaticAssetManager::kGStaticBase, "1");
  const char blink_url[] =
      "//www.gstatic.com/psa/static/1-blink.js";
  EXPECT_STREQ(blink_url, manager_->GetAssetUrl(StaticAssetEnum::BLINK_JS,
                                                options_));
}

TEST_F(StaticAssetManagerTest, TestBlinkDebug) {
  manager_->set_serve_asset_from_gstatic(true);
  manager_->set_gstatic_hash(
      StaticAssetEnum::BLINK_JS, StaticAssetManager::kGStaticBase, "1");
  options_->EnableFilter(RewriteOptions::kDebug);
  const char blink_url[] = "http://proxy-domain/psajs/blink_debug.0.js";
  EXPECT_STREQ(blink_url, manager_->GetAssetUrl(StaticAssetEnum::BLINK_JS,
                                                options_));
}

TEST_F(StaticAssetManagerTest, TestDeferJsGstatic) {
  manager_->set_serve_asset_from_gstatic(true);
  manager_->set_gstatic_hash(
      StaticAssetEnum::DEFER_JS, StaticAssetManager::kGStaticBase, "1");
  const char defer_js_url[] =
      "//www.gstatic.com/psa/static/1-js_defer.js";
  EXPECT_STREQ(defer_js_url, manager_->GetAssetUrl(
      StaticAssetEnum::DEFER_JS, options_));
}

TEST_F(StaticAssetManagerTest, TestDeferJsDebug) {
  manager_->set_serve_asset_from_gstatic(true);
  manager_->set_gstatic_hash(
      StaticAssetEnum::DEFER_JS, StaticAssetManager::kGStaticBase, "1");
  options_->EnableFilter(RewriteOptions::kDebug);
  const char defer_js_debug_url[] =
      "http://proxy-domain/psajs/js_defer_debug.0.js";
  EXPECT_STREQ(defer_js_debug_url, manager_->GetAssetUrl(
      StaticAssetEnum::DEFER_JS, options_));
}

TEST_F(StaticAssetManagerTest, TestDeferJsNonGStatic) {
  const char defer_js_url[] =
      "http://proxy-domain/psajs/js_defer.0.js";
  EXPECT_STREQ(defer_js_url, manager_->GetAssetUrl(
      StaticAssetEnum::DEFER_JS, options_));
}

TEST_F(StaticAssetManagerTest, TestJsDebug) {
  options_->EnableFilter(RewriteOptions::kDebug);
  for (int i = 0; i < StaticAssetEnum::StaticAsset_ARRAYSIZE; ++i) {
    StaticAssetEnum::StaticAsset module =
        static_cast<StaticAssetEnum::StaticAsset>(i);
    // TODO(sligocki): This should generalize to all resources which don't have
    // kContentTypeJs. But no interface provides content types currently :/
    if (module != StaticAssetEnum::BLANK_GIF) {
      GoogleString script(manager_->GetAsset(module, options_));
      // Debug code is also put through the closure compiler to resolve any uses
      // of goog.require. As part of this, comments also get stripped out.
      EXPECT_EQ(GoogleString::npos, script.find("/*"))
          << "Comment found in debug version of asset " << module;
    }
  }
}

TEST_F(StaticAssetManagerTest, TestJsOpt) {
  for (int i = 0; i < StaticAssetEnum::StaticAsset_ARRAYSIZE; ++i) {
    StaticAssetEnum::StaticAsset module =
        static_cast<StaticAssetEnum::StaticAsset>(i);
    // TODO(sligocki): This should generalize to all resources which don't have
    // kContentTypeJs. But no interface provides content types currently :/
    if (module != StaticAssetEnum::BLANK_GIF) {
      GoogleString script(manager_->GetAsset(module, options_));
      EXPECT_EQ(GoogleString::npos, script.find("/*"))
          << "Comment found in opt version of asset " << module;
    }
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

TEST_F(StaticAssetManagerTest, TestEncodedUrls) {
  for (int i = 0; i < StaticAssetEnum::StaticAsset_ARRAYSIZE; ++i) {
    StaticAssetEnum::StaticAsset module =
        static_cast<StaticAssetEnum::StaticAsset>(i);

    GoogleString url = manager_->GetAssetUrl(module, options_);
    StringPiece file_name = url;
    const StringPiece kDomainAndPath = "http://proxy-domain/psajs/";
    ASSERT_TRUE(file_name.starts_with(kDomainAndPath));
    file_name.remove_prefix(kDomainAndPath.size());

    StringPiece content, cache_header;
    ContentType content_type;
    EXPECT_TRUE(manager_->GetAsset(file_name, &content, &content_type,
                                   &cache_header));
    EXPECT_EQ("max-age=31536000", cache_header);
  }
}

}  // namespace

}  // namespace net_instaweb
