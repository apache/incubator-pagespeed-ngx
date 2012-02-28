/*
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
// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/rewriter/public/javascript_code_block.h"

#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

// This sample code comes from Douglas Crockford's jsmin example.
// The same code is used to test jsminify in pagespeed.
const GoogleString kBeforeCompilation =
    "// is.js\n"
    "\n"
    "// (c) 2001 Douglas Crockford\n"
    "// 2001 June 3\n"
    "\n"
    "\n"
    "// is\n"
    "\n"
    "// The -is- object is used to identify the browser.  "
    "Every browser edition\n"
    "// identifies itself, but there is no standard way of doing it, "
    "and some of\n"
    "// the identification is deceptive. This is because the authors of web\n"
    "// browsers are liars. For example, Microsoft's IE browsers claim to be\n"
    "// Mozilla 4. Netscape 6 claims to be version 5.\n"
    "\n"
    "var is = {\n"
    "    ie:      navigator.appName == 'Microsoft Internet Explorer',\n"
    "    java:    navigator.javaEnabled(),\n"
    "    ns:      navigator.appName == 'Netscape',\n"
    "    ua:      navigator.userAgent.toLowerCase(),\n"
    "    version: parseFloat(navigator.appVersion.substr(21)) ||\n"
    "             parseFloat(navigator.appVersion),\n"
    "    win:     navigator.platform == 'Win32'\n"
    "}\n"
    "is.mac = is.ua.indexOf('mac') >= 0;\n"
    "if (is.ua.indexOf('opera') >= 0) {\n"
    "    is.ie = is.ns = false;\n"
    "    is.opera = true;\n"
    "}\n"
    "if (is.ua.indexOf('gecko') >= 0) {\n"
    "    is.ie = is.ns = false;\n"
    "    is.gecko = true;\n"
    "}\n";

const GoogleString kTruncatedComment =
    "// is.js\n"
    "\n"
    "// (c) 2001 Douglas Crockford\n"
    "// 2001 June 3\n"
    "\n"
    "\n"
    "// is\n"
    "\n"
    "/* The -is- object is used to identify the browser.  "
    "Every browser edition\n"
    "   identifies itself, but there is no standard way of doing it, "
    "and some of\n";

const GoogleString kTruncatedRewritten =
    "// is.js\n"
    "\n"
    "// (c) 2001 Douglas Crockford\n"
    "// 2001 June 3\n"
    "\n"
    "\n"
    "// is\n"
    "\n"
    "/* The -is- object is used to identify the browser.  "
    "Every browser edition\n"
    "   identifies itself, but there is no standard way of doing it, "
    "and some of";

const GoogleString kTruncatedString =
    "var is = {\n"
    "    ie:      navigator.appName == 'Microsoft Internet Explo";

const GoogleString kAfterCompilation =
    "var is={ie:navigator.appName=='Microsoft Internet Explorer',"
    "java:navigator.javaEnabled(),ns:navigator.appName=='Netscape',"
    "ua:navigator.userAgent.toLowerCase(),version:parseFloat("
    "navigator.appVersion.substr(21))||parseFloat(navigator.appVersion)"
    ",win:navigator.platform=='Win32'}\n"
    "is.mac=is.ua.indexOf('mac')>=0;if(is.ua.indexOf('opera')>=0){"
    "is.ie=is.ns=false;is.opera=true;}\n"
    "if(is.ua.indexOf('gecko')>=0){is.ie=is.ns=false;is.gecko=true;}";

void ExpectStats(JavascriptRewriteConfig* config,
                 int blocks_minified, int minification_failures,
                 int total_bytes_saved, int total_original_bytes) {
  EXPECT_EQ(blocks_minified, config->blocks_minified()->Get());
  EXPECT_EQ(minification_failures, config->minification_failures()->Get());
  EXPECT_EQ(total_bytes_saved, config->total_bytes_saved()->Get());
  EXPECT_EQ(total_original_bytes, config->total_original_bytes()->Get());
  // Note: We cannot compare num_uses() because we only use it in
  // javascript_filter.cc, not javascript_code_block.cc.
}

TEST(JsCodeBlockTest, Config) {
  SimpleStats stats;
  JavascriptRewriteConfig::Initialize(&stats);
  JavascriptRewriteConfig config(&stats);
  EXPECT_TRUE(config.minify());
  config.set_minify(false);
  EXPECT_FALSE(config.minify());
  config.set_minify(true);
  EXPECT_TRUE(config.minify());
  ExpectStats(&config, 0, 0, 0, 0);
}

TEST(JsCodeBlockTest, Rewrite) {
  SimpleStats stats;
  JavascriptRewriteConfig::Initialize(&stats);
  JavascriptRewriteConfig config(&stats);
  GoogleMessageHandler handler;
  JavascriptCodeBlock block(kBeforeCompilation, &config, "Test", &handler);
  EXPECT_TRUE(block.ProfitableToRewrite());
  EXPECT_EQ(kAfterCompilation, block.Rewritten());
  ExpectStats(&config, 1, 0,
              kBeforeCompilation.size() - kAfterCompilation.size(),
              kBeforeCompilation.size());
}

TEST(JsCodeBlockTest, NoRewrite) {
  SimpleStats stats;
  JavascriptRewriteConfig::Initialize(&stats);
  JavascriptRewriteConfig config(&stats);
  GoogleMessageHandler handler;
  JavascriptCodeBlock block(kAfterCompilation, &config, "Test", &handler);
  EXPECT_FALSE(block.ProfitableToRewrite());
  EXPECT_EQ(kAfterCompilation, block.Rewritten());
  // Note: We do record this as a successful minification.
  // Just with 0 bytes saved.
  ExpectStats(&config, 1, 0, 0, kAfterCompilation.size());
}

TEST(JsCodeBlockTest, TruncatedComment) {
  SimpleStats stats;
  JavascriptRewriteConfig::Initialize(&stats);
  JavascriptRewriteConfig config(&stats);
  GoogleMessageHandler handler;
  JavascriptCodeBlock block(kTruncatedComment, &config, "Test", &handler);
  EXPECT_TRUE(block.ProfitableToRewrite());
  EXPECT_EQ(kTruncatedRewritten, block.Rewritten());
  // Note: We do actually strip off a few bytes, but only using TrimWhitespace
  // so we don't count it towards our minification bytes saved.
  ExpectStats(&config, 0, 1, 0, 0);
}

TEST(JsCodeBlockTest, TruncatedString) {
  SimpleStats stats;
  JavascriptRewriteConfig::Initialize(&stats);
  JavascriptRewriteConfig config(&stats);
  GoogleMessageHandler handler;
  JavascriptCodeBlock block(kTruncatedString, &config, "Test", &handler);
  EXPECT_FALSE(block.ProfitableToRewrite());
  EXPECT_EQ(kTruncatedString, block.Rewritten());
  ExpectStats(&config, 0, 1, 0, 0);
}

TEST(JsCodeBlockTest, NoMinification) {
  SimpleStats stats;
  JavascriptRewriteConfig::Initialize(&stats);
  JavascriptRewriteConfig config(&stats);
  config.set_minify(false);
  GoogleMessageHandler handler;
  JavascriptCodeBlock block(kBeforeCompilation, &config, "Test", &handler);
  EXPECT_FALSE(block.ProfitableToRewrite());
  EXPECT_EQ(kBeforeCompilation, block.Rewritten());
  ExpectStats(&config, 0, 0, 0, 0);
}

TEST(JsCodeBlockTest, DealWithSgmlComment) {
  SimpleStats stats;
  JavascriptRewriteConfig::Initialize(&stats);
  JavascriptRewriteConfig config(&stats);
  GoogleMessageHandler handler;
  const GoogleString original = "  <!--  \nvar x = 1;\n  //-->  ";
  const GoogleString expected = "var x=1;";
  JavascriptCodeBlock block(original, &config, "Test", &handler);
  EXPECT_TRUE(block.ProfitableToRewrite());
  EXPECT_EQ(expected, block.Rewritten());
  ExpectStats(&config, 1, 0,
              original.size() - expected.size(), original.size());
}

}  // namespace

}  // namespace net_instaweb
