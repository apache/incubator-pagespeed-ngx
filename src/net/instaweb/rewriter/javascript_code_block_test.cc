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

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/javascript_library_identification.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

// This sample code comes from Douglas Crockford's jsmin example.
// The same code is used to test jsminify in pagespeed.
// We've added some leading and trailing whitespace here just to
// test our treatment of those cases (we used to erase this stuff
// even if the file wasn't minifiable).
const char kBeforeCompilation[] =
    "     \n"
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
    "}\n"
    "     \n";

const char kLibraryUrl[] = "//example.com/test_library.js";

const char kTruncatedComment[] =
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

// Again we add some leading whitespace here to check for handling of this issue
// in otherwise non-minifiable code.  We've elected not to strip the whitespace.
const char kTruncatedString[] =
    "     \n"
    "var is = {\n"
    "    ie:      navigator.appName == 'Microsoft Internet Explo";

const char kAfterCompilation[] =
    "var is={ie:navigator.appName=='Microsoft Internet Explorer',"
    "java:navigator.javaEnabled(),ns:navigator.appName=='Netscape',"
    "ua:navigator.userAgent.toLowerCase(),version:parseFloat("
    "navigator.appVersion.substr(21))||parseFloat(navigator.appVersion)"
    ",win:navigator.platform=='Win32'}\n"
    "is.mac=is.ua.indexOf('mac')>=0;if(is.ua.indexOf('opera')>=0){"
    "is.ie=is.ns=false;is.opera=true;}\n"
    "if(is.ua.indexOf('gecko')>=0){is.ie=is.ns=false;is.gecko=true;}";

const char kJsWithGetElementsByTagNameScript[] =
    "// this shouldn't be altered"
    "  var scripts = document.getElementsByTagName('script'),"
    "      script = scripts[scripts.length - 1];"
    "  var some_url = document.createElement(\"a\");";

const char kJsWithJQueryScriptElementSelection[] =
    "// this shouldn't be altered either"
    "  var scripts = $(\"script\"),"
    "      script = scripts[scripts.length - 1];"
    "  var some_url = document.createElement(\"a\");";

const char kBogusLibraryMD5[] = "ltVVzzYxo0";

const char kBogusLibraryUrl[] =
    "//www.example.com/js/bogus_library.js";

class JsCodeBlockTest : public testing::Test {
 protected:
  JsCodeBlockTest() {
    JavascriptRewriteConfig::Initialize(&stats_);
    config_.reset(new JavascriptRewriteConfig(&stats_, true, &libraries_));
    // Register a bogus library with a made-up md5 and plausible canonical url
    // that doesn't occur in our tests, but has the same size as our canonical
    // test case.
    EXPECT_TRUE(libraries_.RegisterLibrary(STATIC_STRLEN(kAfterCompilation),
                                           kBogusLibraryMD5, kBogusLibraryUrl));
  }

  void ExpectStats(int blocks_minified, int minification_failures,
                   int total_bytes_saved, int total_original_bytes) {
    EXPECT_EQ(blocks_minified, config_->blocks_minified()->Get());
    EXPECT_EQ(minification_failures, config_->minification_failures()->Get());
    EXPECT_EQ(total_bytes_saved, config_->total_bytes_saved()->Get());
    EXPECT_EQ(total_original_bytes, config_->total_original_bytes()->Get());
    // Note: We cannot compare num_uses() because we only use it in
    // javascript_filter.cc, not javascript_code_block.cc.
  }

  void DisableMinification() {
    config_.reset(new JavascriptRewriteConfig(&stats_, false, &libraries_));
  }

  // Must be called after DisableMinification if we call both.
  void DisableLibraryIdentification() {
    config_.reset(
        new JavascriptRewriteConfig(&stats_, config_->minify(), NULL));
  }

  void RegisterLibrariesIn(JavascriptLibraryIdentification* libs) {
    MD5Hasher md5;
    GoogleString after_md5 = md5.Hash(kAfterCompilation);
    EXPECT_TRUE(libs->RegisterLibrary(STATIC_STRLEN(kAfterCompilation),
                                      after_md5, kLibraryUrl));
  }

  void RegisterLibraries() {
    RegisterLibrariesIn(&libraries_);
  }

  JavascriptCodeBlock* TestBlock(StringPiece code) {
    return new JavascriptCodeBlock(code, config_.get(), "Test", &handler_);
  }

  void SimpleRewriteTest() {
    scoped_ptr<JavascriptCodeBlock> block(TestBlock(kBeforeCompilation));
    EXPECT_TRUE(block->ProfitableToRewrite());
    EXPECT_EQ(kAfterCompilation, block->Rewritten());
    ExpectStats(1, 0,
                STATIC_STRLEN(kBeforeCompilation) -
                STATIC_STRLEN(kAfterCompilation),
                STATIC_STRLEN(kBeforeCompilation));
  }

  GoogleMessageHandler handler_;
  SimpleStats stats_;
  JavascriptLibraryIdentification libraries_;
  scoped_ptr<JavascriptRewriteConfig> config_;

 private:
  DISALLOW_COPY_AND_ASSIGN(JsCodeBlockTest);
};

TEST_F(JsCodeBlockTest, Config) {
  EXPECT_TRUE(config_->minify());
  ExpectStats(0, 0, 0, 0);
}

TEST_F(JsCodeBlockTest, Rewrite) {
  SimpleRewriteTest();
}

TEST_F(JsCodeBlockTest, RewriteNoIdentification) {
  // Make sure library identification setting doesn't change minification.
  DisableLibraryIdentification();
  SimpleRewriteTest();
}

TEST_F(JsCodeBlockTest, UnsafeToRename) {
  EXPECT_TRUE(JavascriptCodeBlock::UnsafeToRename(
      kJsWithGetElementsByTagNameScript));
  EXPECT_TRUE(JavascriptCodeBlock::UnsafeToRename(
      kJsWithJQueryScriptElementSelection));
  EXPECT_FALSE(JavascriptCodeBlock::UnsafeToRename(
      kBeforeCompilation));
}

TEST_F(JsCodeBlockTest, NoRewrite) {
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kAfterCompilation));
  EXPECT_FALSE(block->ProfitableToRewrite());
  EXPECT_EQ(kAfterCompilation, block->Rewritten());
  // Note: We do record this as a successful minification.
  // Just with 0 bytes saved.
  ExpectStats(1, 0, 0, STATIC_STRLEN(kAfterCompilation));
}

TEST_F(JsCodeBlockTest, TruncatedComment) {
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kTruncatedComment));
  EXPECT_FALSE(block->ProfitableToRewrite());
  EXPECT_EQ(kTruncatedComment, block->Rewritten());
  ExpectStats(0, 1, 0, 0);
}

TEST_F(JsCodeBlockTest, TruncatedString) {
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kTruncatedString));
  EXPECT_FALSE(block->ProfitableToRewrite());
  EXPECT_EQ(kTruncatedString, block->Rewritten());
  ExpectStats(0, 1, 0, 0);
}

TEST_F(JsCodeBlockTest, NoMinification) {
  DisableMinification();
  DisableLibraryIdentification();
  EXPECT_FALSE(config_->minify());
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kBeforeCompilation));
  EXPECT_FALSE(block->ProfitableToRewrite());
  EXPECT_EQ(kBeforeCompilation, block->Rewritten());
  ExpectStats(0, 0, 0, 0);
}

TEST_F(JsCodeBlockTest, DealWithSgmlComment) {
  // Based on actual code seen in the wild; the surprising part is this works at
  // all (due to xhtml in the source document)!
  static const char kOriginal[] = "  <!--  \nvar x = 1;\n  //-->  ";
  static const char kExpected[] = "var x=1;";
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kOriginal));
  EXPECT_TRUE(block->ProfitableToRewrite());
  EXPECT_EQ(kExpected, block->Rewritten());
  ExpectStats(1, 0,
              STATIC_STRLEN(kOriginal) - STATIC_STRLEN(kExpected),
              STATIC_STRLEN(kOriginal));
}

TEST_F(JsCodeBlockTest, IdentifyUnminified) {
  RegisterLibraries();
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kBeforeCompilation));
  EXPECT_EQ(kLibraryUrl, block->ComputeJavascriptLibrary());
}

TEST_F(JsCodeBlockTest, IdentifyMerged) {
  JavascriptLibraryIdentification other_libraries;
  RegisterLibrariesIn(&other_libraries);
  libraries_.Merge(other_libraries);
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kBeforeCompilation));
  EXPECT_EQ(kLibraryUrl, block->ComputeJavascriptLibrary());
}

TEST_F(JsCodeBlockTest, IdentifyMergedDuplicate) {
  RegisterLibraries();
  JavascriptLibraryIdentification other_libraries;
  RegisterLibrariesIn(&other_libraries);
  libraries_.Merge(other_libraries);
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kBeforeCompilation));
  EXPECT_EQ(kLibraryUrl, block->ComputeJavascriptLibrary());
}

TEST_F(JsCodeBlockTest, IdentifyMinified) {
  RegisterLibraries();
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kAfterCompilation));
  EXPECT_EQ(kLibraryUrl, block->ComputeJavascriptLibrary());
}

TEST_F(JsCodeBlockTest, IdentifyNoMinification) {
  DisableMinification();
  RegisterLibraries();
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kBeforeCompilation));
  EXPECT_EQ(kLibraryUrl, block->ComputeJavascriptLibrary());
  EXPECT_FALSE(block->ProfitableToRewrite());
  EXPECT_EQ(kBeforeCompilation, block->Rewritten());
  ExpectStats(1, 0, 0, 0);
}

TEST_F(JsCodeBlockTest, IdentifyNoMatch) {
  RegisterLibraries();
  scoped_ptr<JavascriptCodeBlock> block(
      TestBlock(kJsWithGetElementsByTagNameScript));
  EXPECT_EQ("", block->ComputeJavascriptLibrary());
}

TEST_F(JsCodeBlockTest, LibrarySignature) {
  RegisterLibraries();
  GoogleString signature;
  libraries_.AppendSignature(&signature);
  MD5Hasher md5;
  GoogleString after_md5 = md5.Hash(kAfterCompilation);
  GoogleString expected_signature =
      StrCat("S:", Integer64ToString(STATIC_STRLEN(kAfterCompilation)),
             "_H:", after_md5, "_J:", kLibraryUrl,
             StrCat("_H:", kBogusLibraryMD5, "_J:", kBogusLibraryUrl));
  EXPECT_EQ(expected_signature, signature);
}

TEST_F(JsCodeBlockTest, BogusLibraryRegistration) {
  RegisterLibraries();
  // Try to register a library with a bad md5 string
  EXPECT_FALSE(libraries_.RegisterLibrary(73, "@$%@^#&#$^!%@#$",
                                          "//www.example.com/test.js"));
  // Try to register a library with a bad url
  EXPECT_FALSE(libraries_.RegisterLibrary(47, kBogusLibraryMD5,
                                          "totally://bogus.protocol/"));
}

}  // namespace

}  // namespace net_instaweb
