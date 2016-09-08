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

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_filter.h"
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
                                          server_context()->thread_system(),
                                          server_context()->hasher(),
                                          server_context()->message_handler()));
  }

  virtual void SetUp() {
    RewriteTestBase::SetUp();
  }

  // Helper filters to help test inserting of static JS.
  class AddStaticJsBeforeBr: public CommonFilter {
   public:
    explicit AddStaticJsBeforeBr(RewriteDriver* driver)
        : CommonFilter(driver) { }
    virtual void StartDocumentImpl() { }
    virtual void StartElementImpl(HtmlElement* element) { }
    virtual void EndElementImpl(HtmlElement* element) {
      if (element->keyword() == HtmlName::kBr) {
        HtmlElement* script = driver()->NewElement(element->parent(),
                                                  HtmlName::kScript);
        driver()->InsertNodeBeforeNode(element, script);
        AddJsToElement(kScript, script);
      }
    }
    virtual const char* Name() const { return "AddStaticJsBeforeBr"; }
    ScriptUsage GetScriptUsage() const override { return kWillInjectScripts; }

   private:
    DISALLOW_COPY_AND_ASSIGN(AddStaticJsBeforeBr);
  };

  // Extracts the first comment sequences in a script that doesn't
  // contain any whitelisted substrings.
  //
  // TODO(jmarantz): Note that this is not a proper lexer and will be
  // fooled by comment sequences in strings, thus this might require
  // refactoring to use our js tokenizer in the future, should this
  // yield false positives.
  StringPiece ExtractCommentSkippingWhitelist(StringPiece script) {
    StringPieceVector v;
    stringpiece_ssize_type pos = 0;
    while (true) {
      pos = script.find("/*", pos);
      if (pos == StringPiece::npos) {
        return "";
      }
      stringpiece_ssize_type endpos = 0;
      endpos = script.find("*/", pos + 2);
      if (endpos == StringPiece::npos) {
        return "";
      }
      endpos += 2;
      StringPiece comment = script.substr(pos, endpos - pos);
      if (comment.find("MochiKit") == StringPiece::npos) {
        return comment;
      }
      pos = endpos;
    }
  }

  scoped_ptr<StaticAssetManager> manager_;
};

TEST_F(StaticAssetManagerTest, TestDeferJsGstatic) {
  manager_->ServeAssetsFromGStatic(StaticAssetManager::kGStaticBase);
  manager_->SetGStaticHashForTest(StaticAssetEnum::DEFER_JS, "1");
  const char defer_js_url[] =
      "//www.gstatic.com/psa/static/1-js_defer.js";
  EXPECT_STREQ(defer_js_url, manager_->GetAssetUrl(
      StaticAssetEnum::DEFER_JS, options_));
}

TEST_F(StaticAssetManagerTest, TestDeferJsDebug) {
  manager_->ServeAssetsFromGStatic(StaticAssetManager::kGStaticBase);
  manager_->SetGStaticHashForTest(StaticAssetEnum::DEFER_JS, "1");
  options_->EnableFilter(RewriteOptions::kDebug);
  const char defer_js_debug_url[] =
      "//www.gstatic.com/psa/static/1-js_defer.js";
  EXPECT_STREQ(defer_js_debug_url, manager_->GetAssetUrl(
      StaticAssetEnum::DEFER_JS, options_));
}

TEST_F(StaticAssetManagerTest, TestDeferJsNonGStatic) {
  const char defer_js_url[] =
      "http://proxy-domain/psajs/js_defer.0.js";
  EXPECT_STREQ(defer_js_url, manager_->GetAssetUrl(
      StaticAssetEnum::DEFER_JS, options_));
}

TEST_F(StaticAssetManagerTest, TestDeferJsNonGStaticDebug) {
  const char defer_js_url[] =
      "http://proxy-domain/psajs/js_defer_debug.0.js";
  options_->EnableFilter(RewriteOptions::kDebug);
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
    if (module != StaticAssetEnum::BLANK_GIF &&
        module != StaticAssetEnum::MOBILIZE_CSS) {
      GoogleString script(manager_->GetAsset(module, options_));
      // Debug code is also put through the closure compiler to resolve any uses
      // of goog.require. As part of this, comments also get stripped out.
      EXPECT_STREQ("", ExtractCommentSkippingWhitelist(script))
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
    if (module != StaticAssetEnum::BLANK_GIF &&
        module != StaticAssetEnum::MOBILIZE_CSS) {
      GoogleString script(manager_->GetAsset(module, options_));
      EXPECT_STREQ("", ExtractCommentSkippingWhitelist(script))
          << "Comment found in debug version of asset " << module;
    }
  }
}

TEST_F(StaticAssetManagerTest, TestHtmlInsertInlineJs) {
  SetHtmlMimetype();
  AddStaticJsBeforeBr filter(rewrite_driver());
  rewrite_driver()->AddFilter(&filter);
  ParseUrl(kTestDomain, kHtml);
  EXPECT_EQ("<html>\n<body><script type=\"text/javascript\">alert('foo');"
            "</script><br></body></html>", output_buffer_);
}

TEST_F(StaticAssetManagerTest, TestXhtmlInsertInlineJs) {
  SetXhtmlMimetype();
  AddStaticJsBeforeBr filter(rewrite_driver());
  rewrite_driver()->AddFilter(&filter);
  ParseUrl(kTestDomain, kHtml);
  EXPECT_EQ("<html>\n<body><script type=\"text/javascript\">//<![CDATA[\n"
            "alert('foo');\n//]]></script><br></body></html>",
            output_buffer_);
}

TEST_F(StaticAssetManagerTest, TestHtml5InsertInlineJs) {
  SetHtmlMimetype();
  AddStaticJsBeforeBr filter(rewrite_driver());
  rewrite_driver()->AddFilter(&filter);
  GoogleString html = StrCat("<!DOCTYPE html>", kHtml);
  ParseUrl(kTestDomain, html);
  EXPECT_EQ("<html>\n<!DOCTYPE html><body><script>alert('foo');"
            "</script><br></body></html>", output_buffer_);
}

TEST_F(StaticAssetManagerTest, TestEncodedUrls) {
  for (int i = 0; i < StaticAssetEnum::StaticAsset_ARRAYSIZE; ++i) {
    if (!manager_->IsValidIndex(i)) {
      continue;
    }
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

TEST_F(StaticAssetManagerTest, FullGStaticConf) {
  scoped_ptr<RewriteOptions> debug_options(options_->Clone());
  debug_options->EnableFilter(RewriteOptions::kDebug);

  manager_->ServeAssetsFromGStatic("http://actually_any_cdn.com/");

  // Setup initial batch configuration.
  StaticAssetConfig config;
  config.set_release_label("100");
  StaticAssetConfig::Asset* asset_conf = config.add_asset();
  asset_conf->set_role(StaticAssetEnum::ADD_INSTRUMENTATION_JS);
  asset_conf->set_name("add_instr.js");
  asset_conf->set_debug_hash("dbg1");
  asset_conf->set_opt_hash("opt1");

  asset_conf = config.add_asset();
  asset_conf->set_role(StaticAssetEnum::LAZYLOAD_IMAGES_JS);
  asset_conf->set_name("lazy.js");
  asset_conf->set_debug_hash("dbg2");
  asset_conf->set_opt_hash("opt2");

  manager_->ApplyGStaticConfiguration(
      config, StaticAssetManager::kInitialConfiguration);

  // The configuration is sparse, so things still retain defaults.
  EXPECT_EQ("http://proxy-domain/psajs/js_defer.0.js",
            manager_->GetAssetUrl(StaticAssetEnum::DEFER_JS, options_));
  EXPECT_EQ("http://proxy-domain/psajs/js_defer_debug.0.js",
            manager_->GetAssetUrl(StaticAssetEnum::DEFER_JS,
                                  debug_options.get()));

  // The configured things do work, however.
  EXPECT_EQ("http://actually_any_cdn.com/opt1-add_instr.js",
            manager_->GetAssetUrl(StaticAssetEnum::ADD_INSTRUMENTATION_JS,
                                  options_));
  EXPECT_EQ("http://actually_any_cdn.com/dbg1-add_instr.js",
            manager_->GetAssetUrl(StaticAssetEnum::ADD_INSTRUMENTATION_JS,
                                  debug_options.get()));

  EXPECT_EQ("http://actually_any_cdn.com/opt2-lazy.js",
            manager_->GetAssetUrl(StaticAssetEnum::LAZYLOAD_IMAGES_JS,
                                  options_));
  EXPECT_EQ("http://actually_any_cdn.com/dbg2-lazy.js",
            manager_->GetAssetUrl(StaticAssetEnum::LAZYLOAD_IMAGES_JS,
                                  debug_options.get()));

  // Now try updating with a config with different version.
  StaticAssetConfig config2;
  config2.set_release_label("99");
  asset_conf = config2.add_asset();
  asset_conf->set_role(StaticAssetEnum::ADD_INSTRUMENTATION_JS);
  asset_conf->set_name("add_instr.js");
  asset_conf->set_debug_hash("dbg0");
  asset_conf->set_opt_hash("opt0");

  asset_conf = config2.add_asset();
  asset_conf->set_role(StaticAssetEnum::LAZYLOAD_IMAGES_JS);
  asset_conf->set_name("lazy.js");
  asset_conf->set_debug_hash("dbg0");
  asset_conf->set_opt_hash("opt0");

  asset_conf = config2.add_asset();
  asset_conf->set_role(StaticAssetEnum::DEFER_JS);
  asset_conf->set_name("defer.js");
  asset_conf->set_debug_hash("dbg0");
  asset_conf->set_opt_hash("opt0");

  manager_->ApplyGStaticConfiguration(
      config2,
      StaticAssetManager::kUpdateConfiguration);

  // Nothing is actually changed.
  EXPECT_EQ("http://proxy-domain/psajs/js_defer.0.js",
            manager_->GetAssetUrl(StaticAssetEnum::DEFER_JS, options_));
  EXPECT_EQ("http://proxy-domain/psajs/js_defer_debug.0.js",
            manager_->GetAssetUrl(StaticAssetEnum::DEFER_JS,
                                  debug_options.get()));

  EXPECT_EQ("http://actually_any_cdn.com/opt1-add_instr.js",
            manager_->GetAssetUrl(StaticAssetEnum::ADD_INSTRUMENTATION_JS,
                                  options_));
  EXPECT_EQ("http://actually_any_cdn.com/dbg1-add_instr.js",
            manager_->GetAssetUrl(StaticAssetEnum::ADD_INSTRUMENTATION_JS,
                                  debug_options.get()));

  EXPECT_EQ("http://actually_any_cdn.com/opt2-lazy.js",
            manager_->GetAssetUrl(StaticAssetEnum::LAZYLOAD_IMAGES_JS,
                                  options_));
  EXPECT_EQ("http://actually_any_cdn.com/dbg2-lazy.js",
            manager_->GetAssetUrl(StaticAssetEnum::LAZYLOAD_IMAGES_JS,
                                  debug_options.get()));

  // Now with matching version, things can change. Notice that we still didn't
  // update things that were not specified in initial config. This is a design
  // tradeoff: it makes it harder to hotfix things, but means we won't get stuck
  // with a stale override in CDD.
  StaticAssetConfig config3;
  config3.set_release_label("100");
  asset_conf = config3.add_asset();
  asset_conf->set_role(StaticAssetEnum::ADD_INSTRUMENTATION_JS);
  asset_conf->set_name("add_instr.js");
  asset_conf->set_debug_hash("dbg3");
  asset_conf->set_opt_hash("opt3");

  asset_conf = config3.add_asset();
  asset_conf->set_role(StaticAssetEnum::LAZYLOAD_IMAGES_JS);
  asset_conf->set_name("lazy.js");
  asset_conf->set_debug_hash("dbg4");
  asset_conf->set_opt_hash("opt4");

  asset_conf = config3.add_asset();
  asset_conf->set_role(StaticAssetEnum::DEFER_JS);
  asset_conf->set_name("defer.js");
  asset_conf->set_debug_hash("dbg5");
  asset_conf->set_opt_hash("opt5");

  manager_->ApplyGStaticConfiguration(
      config3,
      StaticAssetManager::kUpdateConfiguration);

  // Everything that was initially configured via this is changed.
  EXPECT_EQ("http://proxy-domain/psajs/js_defer.0.js",
            manager_->GetAssetUrl(StaticAssetEnum::DEFER_JS, options_));
  EXPECT_EQ("http://proxy-domain/psajs/js_defer_debug.0.js",
            manager_->GetAssetUrl(StaticAssetEnum::DEFER_JS,
                                  debug_options.get()));

  EXPECT_EQ("http://actually_any_cdn.com/opt3-add_instr.js",
            manager_->GetAssetUrl(StaticAssetEnum::ADD_INSTRUMENTATION_JS,
                                  options_));
  EXPECT_EQ("http://actually_any_cdn.com/dbg3-add_instr.js",
            manager_->GetAssetUrl(StaticAssetEnum::ADD_INSTRUMENTATION_JS,
                                  debug_options.get()));

  EXPECT_EQ("http://actually_any_cdn.com/opt4-lazy.js",
            manager_->GetAssetUrl(StaticAssetEnum::LAZYLOAD_IMAGES_JS,
                                  options_));
  EXPECT_EQ("http://actually_any_cdn.com/dbg4-lazy.js",
            manager_->GetAssetUrl(StaticAssetEnum::LAZYLOAD_IMAGES_JS,
                                  debug_options.get()));

  // Now test that resetting config works.
  manager_->ResetGStaticConfiguration();
  EXPECT_EQ("http://proxy-domain/psajs/js_defer.0.js",
            manager_->GetAssetUrl(StaticAssetEnum::DEFER_JS, options_));
  EXPECT_EQ("http://proxy-domain/psajs/js_defer_debug.0.js",
            manager_->GetAssetUrl(StaticAssetEnum::DEFER_JS,
                                  debug_options.get()));

  // The configured things do work, however.
  EXPECT_EQ("http://actually_any_cdn.com/opt1-add_instr.js",
            manager_->GetAssetUrl(StaticAssetEnum::ADD_INSTRUMENTATION_JS,
                                  options_));
  EXPECT_EQ("http://actually_any_cdn.com/dbg1-add_instr.js",
            manager_->GetAssetUrl(StaticAssetEnum::ADD_INSTRUMENTATION_JS,
                                  debug_options.get()));

  EXPECT_EQ("http://actually_any_cdn.com/opt2-lazy.js",
            manager_->GetAssetUrl(StaticAssetEnum::LAZYLOAD_IMAGES_JS,
                                  options_));
  EXPECT_EQ("http://actually_any_cdn.com/dbg2-lazy.js",
            manager_->GetAssetUrl(StaticAssetEnum::LAZYLOAD_IMAGES_JS,
                                  debug_options.get()));
}

}  // namespace

}  // namespace net_instaweb
