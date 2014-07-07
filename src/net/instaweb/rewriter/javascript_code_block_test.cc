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

#include "net/instaweb/rewriter/public/javascript_library_identification.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/js/js_tokenizer.h"
#include "pagespeed/kernel/util/platform.h"

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

const char kAfterCompilationOld[] =
    "var is={ie:navigator.appName=='Microsoft Internet Explorer',"
    "java:navigator.javaEnabled(),ns:navigator.appName=='Netscape',"
    "ua:navigator.userAgent.toLowerCase(),version:parseFloat("
    "navigator.appVersion.substr(21))||parseFloat(navigator.appVersion)"
    ",win:navigator.platform=='Win32'}\n"
    "is.mac=is.ua.indexOf('mac')>=0;if(is.ua.indexOf('opera')>=0){"
    "is.ie=is.ns=false;is.opera=true;}\n"  // Note trailing \n
    "if(is.ua.indexOf('gecko')>=0){is.ie=is.ns=false;is.gecko=true;}";

const char kAfterCompilationNew[] =
    "var is={ie:navigator.appName=='Microsoft Internet Explorer',"
    "java:navigator.javaEnabled(),ns:navigator.appName=='Netscape',"
    "ua:navigator.userAgent.toLowerCase(),version:parseFloat("
    "navigator.appVersion.substr(21))||parseFloat(navigator.appVersion)"
    ",win:navigator.platform=='Win32'}\n"
    "is.mac=is.ua.indexOf('mac')>=0;if(is.ua.indexOf('opera')>=0){"
    "is.ie=is.ns=false;is.opera=true;}"  // Note lack of trailing \n
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

// Sample JSON code from http://json.org/example with tons of whitespace.
// Modified to include even more whitespace between special characters and
// in string values/keys.
const char kJsonBeforeCompilation[] =
    "\n\n{\n"
    "    \"glossary    \": {\n"
    "        \"title\": 'example glossary',\n"
    "\t\t \"GlossDiv\": {\n"
    "            \"title\": \"S\",\n"
    "\t\t\t\"GlossList\"  : {\n"
    "                \"GlossEntry\": {\n"
    "                    \"ID\": \"SGML\"   ,\t\n"
    "\t\t\t\t\t\t\"SortAs\": \"SGML\",\n"
    "\t\t\t\t\t\t\t\t\"GlossTerm\": \"Standard Generalized Markup Language\",\n"
    "\t\t\t\t\t\t\t\t\t\t\t     \t       \t\t   \t  \"Acronym\": \"SGML\",\n"
    "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t  \t        \"Abbrev\": \"ISO 8879:1986\",\n"
    "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t         \"GlossDef\": {\n"
    "                        \"para\": \"A meta-markup language, used to create"
    " markup languages such as DocBook.\",\n"
    "\t\t\t\t   \t       \t\t      \"GlossSeeAlso\": [\"GML\", \"XML\"]\n"
    "                    },\n"
    "\t\t\t\t\t\t\"GlossSee\": \"markup\"\n"
    "                }\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "}\n\n\n";

const char kJsonAfterCompilation[] =
    "{\"glossary    \":{\"title\":'example glossary',\"GlossDiv\":{\"title\":"
    "\"S\",\"GlossList\":{\"GlossEntry\":{\"ID\":\"SGML\",\"SortAs\":\"SGML\","
    "\"GlossTerm\":\"Standard Generalized Markup Language\",\"Acronym\":"
    "\"SGML\",\"Abbrev\":\"ISO 8879:1986\",\"GlossDef\":{\"para\":\"A "
    "meta-markup language, used to create markup languages such as DocBook.\","
    "\"GlossSeeAlso\":[\"GML\",\"XML\"]},\"GlossSee\":\"markup\"}}}}}";

class JsCodeBlockTest : public ::testing::Test,
                        public ::testing::WithParamInterface<bool> {
 protected:
  JsCodeBlockTest()
      : thread_system_(Platform::CreateThreadSystem()),
        stats_(thread_system_.get()),
        use_experimental_minifier_(GetParam()),
        after_compilation_(use_experimental_minifier_
                           ? kAfterCompilationNew
                           : kAfterCompilationOld) {
    JavascriptRewriteConfig::InitStats(&stats_);
    config_.reset(new JavascriptRewriteConfig(
        &stats_, true, use_experimental_minifier_, &libraries_,
        &js_tokenizer_patterns_));
    // Register a bogus library with a made-up md5 and plausible canonical url
    // that doesn't occur in our tests, but has the same size as our canonical
    // test case.
    EXPECT_TRUE(libraries_.RegisterLibrary(strlen(after_compilation_),
                                           kBogusLibraryMD5, kBogusLibraryUrl));
  }

  void ExpectStats(int blocks_minified, int minification_failures,
                   int total_bytes_saved, int total_original_bytes,
                   int num_reducing_uses) {
    EXPECT_EQ(blocks_minified, config_->blocks_minified()->Get());
    EXPECT_EQ(minification_failures, config_->minification_failures()->Get());
    EXPECT_EQ(total_bytes_saved, config_->total_bytes_saved()->Get());
    EXPECT_EQ(total_original_bytes, config_->total_original_bytes()->Get());
    EXPECT_EQ(num_reducing_uses, config_->num_reducing_uses()->Get());
    // Note: We cannot compare num_uses() because we only use it in
    // javascript_filter.cc, not javascript_code_block.cc.
  }

  void DisableMinification() {
    config_.reset(new JavascriptRewriteConfig(
        &stats_, false, use_experimental_minifier_, &libraries_,
        &js_tokenizer_patterns_));
  }

  // Must be called after DisableMinification if we call both.
  void DisableLibraryIdentification() {
    config_.reset(new JavascriptRewriteConfig(
        &stats_, config_->minify(), use_experimental_minifier_, NULL,
        &js_tokenizer_patterns_));
  }

  void RegisterLibrariesIn(JavascriptLibraryIdentification* libs) {
    MD5Hasher md5(JavascriptLibraryIdentification::kNumHashChars);
    GoogleString after_md5 = md5.Hash(after_compilation_);
    EXPECT_TRUE(libs->RegisterLibrary(strlen(after_compilation_),
                                      after_md5, kLibraryUrl));
    EXPECT_EQ(JavascriptLibraryIdentification::kNumHashChars,
              after_md5.size());
  }

  void RegisterLibraries() {
    RegisterLibrariesIn(&libraries_);
  }

  JavascriptCodeBlock* TestBlock(StringPiece code) {
    return new JavascriptCodeBlock(code, config_.get(), "Test", &handler_);
  }

  void SingleBlockRewriteTest(const char* before_compilation,
                              const char* after_compilation) {
    scoped_ptr<JavascriptCodeBlock> block(TestBlock(before_compilation));
    EXPECT_TRUE(block->Rewrite());
    EXPECT_TRUE(block->successfully_rewritten());
    EXPECT_EQ(after_compilation, block->rewritten_code());
    ExpectStats(1, 0,
                strlen(before_compilation) - strlen(after_compilation),
                strlen(before_compilation), 1);
  }

  GoogleMessageHandler handler_;
  scoped_ptr<ThreadSystem> thread_system_;
  SimpleStats stats_;
  JavascriptLibraryIdentification libraries_;
  const pagespeed::js::JsTokenizerPatterns js_tokenizer_patterns_;
  scoped_ptr<JavascriptRewriteConfig> config_;

  const bool use_experimental_minifier_;
  const char* after_compilation_;

 private:
  DISALLOW_COPY_AND_ASSIGN(JsCodeBlockTest);
};

TEST_P(JsCodeBlockTest, Config) {
  EXPECT_TRUE(config_->minify());
  ExpectStats(0, 0, 0, 0, 0);
}

TEST_P(JsCodeBlockTest, Rewrite) {
  SingleBlockRewriteTest(kBeforeCompilation, after_compilation_);
}

TEST_P(JsCodeBlockTest, RewriteNoIdentification) {
  // Make sure library identification setting doesn't change minification.
  DisableLibraryIdentification();
  SingleBlockRewriteTest(kBeforeCompilation, after_compilation_);
}

TEST_P(JsCodeBlockTest, UnsafeToRename) {
  EXPECT_TRUE(JavascriptCodeBlock::UnsafeToRename(
      kJsWithGetElementsByTagNameScript));
  EXPECT_TRUE(JavascriptCodeBlock::UnsafeToRename(
      kJsWithJQueryScriptElementSelection));
  EXPECT_FALSE(JavascriptCodeBlock::UnsafeToRename(
      kBeforeCompilation));
}

TEST_P(JsCodeBlockTest, NoRewrite) {
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(after_compilation_));
  EXPECT_FALSE(block->Rewrite());
  // Note: Minifier succeeded, but no minification was applied and thus
  // no bytes saved (nor original bytes marked).
  ExpectStats(1, 0, 0, 0, 0);
}

TEST_P(JsCodeBlockTest, TruncatedComment) {
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kTruncatedComment));
  EXPECT_FALSE(block->Rewrite());
  ExpectStats(0, 1, 0, 0, 0);
}

TEST_P(JsCodeBlockTest, TruncatedString) {
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kTruncatedString));
  EXPECT_FALSE(block->Rewrite());
  ExpectStats(0, 1, 0, 0, 0);
}

TEST_P(JsCodeBlockTest, NoMinification) {
  DisableMinification();
  DisableLibraryIdentification();
  EXPECT_FALSE(config_->minify());
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kBeforeCompilation));
  EXPECT_FALSE(block->Rewrite());
  ExpectStats(0, 0, 0, 0, 0);
}

TEST_P(JsCodeBlockTest, DealWithSgmlComment) {
  // Based on actual code seen in the wild; the surprising part is this works at
  // all (due to xhtml in the source document)!
  static const char kOriginal[] = "  <!--  \nvar x = 1;\n  //-->  ";
  static const char kExpected[] = "var x=1;";
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kOriginal));
  EXPECT_TRUE(block->Rewrite());
  EXPECT_EQ(kExpected, block->rewritten_code());
  ExpectStats(1, 0,
              STATIC_STRLEN(kOriginal) - STATIC_STRLEN(kExpected),
              STATIC_STRLEN(kOriginal), 1);
}

TEST_P(JsCodeBlockTest, IdentifyUnminified) {
  RegisterLibraries();
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kBeforeCompilation));
  block->Rewrite();
  EXPECT_EQ(kLibraryUrl, block->ComputeJavascriptLibrary());
}

TEST_P(JsCodeBlockTest, IdentifyMerged) {
  JavascriptLibraryIdentification other_libraries;
  RegisterLibrariesIn(&other_libraries);
  libraries_.Merge(other_libraries);
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kBeforeCompilation));
  block->Rewrite();
  EXPECT_EQ(kLibraryUrl, block->ComputeJavascriptLibrary());
}

TEST_P(JsCodeBlockTest, IdentifyMergedDuplicate) {
  RegisterLibraries();
  JavascriptLibraryIdentification other_libraries;
  RegisterLibrariesIn(&other_libraries);
  libraries_.Merge(other_libraries);
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kBeforeCompilation));
  block->Rewrite();
  EXPECT_EQ(kLibraryUrl, block->ComputeJavascriptLibrary());
}

TEST_P(JsCodeBlockTest, IdentifyMinified) {
  RegisterLibraries();
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(after_compilation_));
  block->Rewrite();
  EXPECT_EQ(kLibraryUrl, block->ComputeJavascriptLibrary());
}

TEST_P(JsCodeBlockTest, IdentifyNoMinification) {
  DisableMinification();
  RegisterLibraries();
  scoped_ptr<JavascriptCodeBlock> block(TestBlock(kBeforeCompilation));
  block->Rewrite();
  EXPECT_EQ(kLibraryUrl, block->ComputeJavascriptLibrary());
  EXPECT_FALSE(block->successfully_rewritten());
  ExpectStats(1, 0, 0, 0, 0);
}

TEST_P(JsCodeBlockTest, IdentifyNoMatch) {
  RegisterLibraries();
  scoped_ptr<JavascriptCodeBlock> block(
      TestBlock(kJsWithGetElementsByTagNameScript));
  block->Rewrite();
  EXPECT_EQ("", block->ComputeJavascriptLibrary());
}

TEST_P(JsCodeBlockTest, LibrarySignature) {
  RegisterLibraries();
  GoogleString signature;
  libraries_.AppendSignature(&signature);
  MD5Hasher md5(JavascriptLibraryIdentification::kNumHashChars);
  GoogleString after_md5 = md5.Hash(after_compilation_);
  GoogleString expected_signature =
      StrCat("S:", Integer64ToString(strlen(after_compilation_)),
             "_H:", after_md5, "_J:", kLibraryUrl,
             StrCat("_H:", kBogusLibraryMD5, "_J:", kBogusLibraryUrl));
  EXPECT_EQ(expected_signature, signature);
}

TEST_P(JsCodeBlockTest, RewriteJson) {
  SingleBlockRewriteTest(kJsonBeforeCompilation, kJsonAfterCompilation);
}

TEST_P(JsCodeBlockTest, InvalidJsonValidJs) {
  // The JS minifier cannot detect invalid JSON which is also valid JS, so we
  // expect this to work.
  SingleBlockRewriteTest(
      "{'foo': bar, baz :}",
      "{'foo':bar,baz:}");
}

TEST_P(JsCodeBlockTest, BogusLibraryRegistration) {
  RegisterLibraries();
  // Try to register a library with a bad md5 string.
  EXPECT_FALSE(libraries_.RegisterLibrary(73, "@$%@^#&#$^!%@#$",
                                          "//www.example.com/test.js"));
  // Try to register a library with a bad url.
  EXPECT_FALSE(libraries_.RegisterLibrary(47, kBogusLibraryMD5,
                                          "totally://bogus.protocol/"));
  EXPECT_FALSE(libraries_.RegisterLibrary(74, kBogusLibraryMD5,
                                          "totally:bogus.protocol"));

  // Don't allow non-standard protocols either.
  EXPECT_FALSE(libraries_.RegisterLibrary(138, kBogusLibraryMD5,
                                          "mailto:johndoe@example.com"));
  EXPECT_FALSE(libraries_.RegisterLibrary(150, kBogusLibraryMD5,
                                          "ftp://www.example.com/test.js"));
  EXPECT_FALSE(libraries_.RegisterLibrary(222, kBogusLibraryMD5,
                                          "file:///etc/passwd"));
  EXPECT_FALSE(libraries_.RegisterLibrary(234, kBogusLibraryMD5,
                                          "data:text/plain,Hello-world"));
}

// We test with use_experimental_minifier == GetParam() as both true and false.
INSTANTIATE_TEST_CASE_P(JsCodeBlockTestInstance, JsCodeBlockTest,
                        ::testing::Bool());

}  // namespace

}  // namespace net_instaweb
