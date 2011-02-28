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

// Author: jmarantz@google.com (Joshua Marantz)
//     and sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/css_filter.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/css_move_to_head_filter.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/img_tag_scanner.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/simple_stats.h"
#include <string>

namespace net_instaweb {

namespace {

class CssFilterTest : public ResourceManagerTestBase {
 protected:
  CssFilterTest() {
    RewriteDriver::Initialize(&statistics_);

    num_files_minified_ = statistics_.GetVariable(CssFilter::kFilesMinified);
    minified_bytes_saved_ =
        statistics_.GetVariable(CssFilter::kMinifiedBytesSaved);
    num_parse_failures_ = statistics_.GetVariable(CssFilter::kParseFailures);
  }

  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    resource_manager_->set_statistics(&statistics_);
    AddFilter(RewriteOptions::kRewriteCss);
  }

  enum ValidationFlags {
    kExpectNoChange = 1,
    kExpectChange = 2,
    kExpectFailure = 4,
    kExpectSuccess = 8,
    kNoStatCheck = 16,
    kNoClearFetcher = 32,
    kNoOtherContexts = 64
  };

  static bool ExactlyOneTrue(bool a, bool b) {
    return a ^ b;
  }

  bool FlagSet(int flags, ValidationFlags f) {
    return (flags & f) != 0;
  }

  // Sanity check on flags passed in -- should specify exactly
  // one of kExpectChange/kExpectNoChange and kExpectFailure/kExpectSuccess
  void CheckFlags(int flags) {
    CHECK(ExactlyOneTrue(FlagSet(flags, kExpectChange),
                         FlagSet(flags, kExpectNoChange)));
    CHECK(ExactlyOneTrue(FlagSet(flags, kExpectFailure),
                         FlagSet(flags, kExpectSuccess)));
  }

  // Check that inline CSS gets rewritten correctly.
  void ValidateRewriteInlineCss(const StringPiece& id,
                                const StringPiece& css_input,
                                const StringPiece& expected_css_output,
                                int flags) {
    static const char prefix[] =
        "<head>\n"
        "  <title>Example style outline</title>\n"
        "  <!-- Style starts here -->\n"
        "  <style type='text/css'>";
    static const char suffix[] = "</style>\n"
        "  <!-- Style ends here -->\n"
        "</head>";

    CheckFlags(flags);
    std::string html_input  = StrCat(prefix, css_input, suffix);
    std::string html_output = StrCat(prefix, expected_css_output, suffix);

    // Reset stats
    num_files_minified_->Set(0);
    minified_bytes_saved_->Set(0);
    num_parse_failures_->Set(0);

    // Rewrite
    ValidateExpected(id, html_input, html_output);

    // Check stats
    if (!(flags & kNoStatCheck)) {
      if (flags & kExpectChange) {
        EXPECT_EQ(1, num_files_minified_->Get());
        EXPECT_EQ(css_input.size() - expected_css_output.size(),
                  minified_bytes_saved_->Get());
        EXPECT_EQ(0, num_parse_failures_->Get());
      } else {
        EXPECT_EQ(0, num_files_minified_->Get());
        EXPECT_EQ(0, minified_bytes_saved_->Get());
        if (flags & kExpectFailure) {
          EXPECT_EQ(1, num_parse_failures_->Get()) << id;
        } else {
          EXPECT_EQ(0, num_parse_failures_->Get()) << id;
        }
      }
    }
  }

  void GetNamerForCss(const StringPiece& id,
                      const std::string& expected_css_output,
                      ResourceNamer* namer) {
    namer->set_id(RewriteDriver::kCssFilterId);
    namer->set_hash(mock_hasher_.Hash(expected_css_output));
    namer->set_ext("css");
    // TODO(sligocki): Derive these from css_url the "right" way.
    std::string url_prefix = kTestDomain;
    namer->set_name(StrCat(id, ".css"));
  }

  std::string ExpectedUrlForNamer(const ResourceNamer& namer) {
    return StrCat(kTestDomain, namer.Encode());
  }

  // Check that external CSS gets rewritten correctly.
  void ValidateRewriteExternalCss(const StringPiece& id,
                                  const std::string& css_input,
                                  const std::string& expected_css_output,
                                  int flags) {
    CheckFlags(flags);

    // TODO(sligocki): Allow arbitrary URLs.
    std::string css_url = StrCat(kTestDomain, id, ".css");

    // Set input file.
    if ((flags & kNoClearFetcher) == 0) {
      mock_url_fetcher_.Clear();
    }
    InitResponseHeaders(StrCat(id, ".css"), kContentTypeCss, css_input, 300);

    static const char html_template[] =
        "<head>\n"
        "  <title>Example style outline</title>\n"
        "  <!-- Style starts here -->\n"
        "  <link rel='stylesheet' type='text/css' href='%s'>\n"
        "  <!-- Style ends here -->\n"
        "</head>";

    std::string html_input  = StringPrintf(html_template, css_url.c_str());

    std::string html_output;

    ResourceNamer namer;
    GetNamerForCss(id, expected_css_output, &namer);
    std::string expected_new_url = ExpectedUrlForNamer(namer);

    if (flags & kExpectChange) {
      html_output = StringPrintf(html_template, expected_new_url.c_str());
    } else {
      html_output = html_input;
    }

    // Reset stats
    num_files_minified_->Set(0);
    minified_bytes_saved_->Set(0);
    num_parse_failures_->Set(0);

    // Rewrite
    ValidateExpected(id, html_input, html_output);

    // Check stats, if requested
    if (!(flags & kNoStatCheck)) {
      if (flags & kExpectChange) {
        EXPECT_EQ(1, num_files_minified_->Get());
        EXPECT_EQ(css_input.size() - expected_css_output.size(),
                  minified_bytes_saved_->Get());
        EXPECT_EQ(0, num_parse_failures_->Get());
      } else {
        EXPECT_EQ(0, num_files_minified_->Get());
        EXPECT_EQ(0, minified_bytes_saved_->Get());
        if (flags & kExpectFailure) {
          EXPECT_EQ(1, num_parse_failures_->Get()) << id;
        } else {
          EXPECT_EQ(0, num_parse_failures_->Get()) << id;
        }
      }
    }

    // If we produced a new output resource, check it.
    if (flags & kExpectChange) {
      std::string actual_output;
      // TODO(sligocki): This will only work with mock_hasher.
      EXPECT_TRUE(ServeResource(kTestDomain,
                                namer.id(), namer.name(), namer.ext(),
                                &actual_output));
      EXPECT_EQ(expected_css_output, actual_output);

      // Serve from new context.
      if ((flags & kNoOtherContexts) == 0) {
        ServeResourceFromManyContexts(expected_new_url,
                                      RewriteOptions::kRewriteCss,
                                      &mock_hasher_, expected_css_output);
      }
    }
  }

  void ValidateRewrite(const StringPiece& id,
                       const std::string& css_input,
                       const std::string& gold_output) {
    ValidateRewriteInlineCss(StrCat(id, "-inline"),
                             css_input, gold_output,
                             kExpectChange | kExpectSuccess);
    ValidateRewriteExternalCss(StrCat(id, "-external"),
                               css_input, gold_output,
                               kExpectChange | kExpectSuccess);
  }

  void ValidateNoChange(const StringPiece& id, const std::string& css_input) {
    ValidateRewriteInlineCss(StrCat(id, "-inline"),
                             css_input, css_input,
                             kExpectNoChange | kExpectSuccess);
    ValidateRewriteExternalCss(StrCat(id, "-external"),
                               css_input, "",
                               kExpectNoChange | kExpectSuccess);
  }

  void ValidateFailParse(const StringPiece& id, const std::string& css_input) {
    ValidateRewriteInlineCss(StrCat(id, "-inline"),
                             css_input, css_input,
                             kExpectNoChange | kExpectFailure);
    ValidateRewriteExternalCss(StrCat(id, "-external"),
                               css_input, "",
                               kExpectNoChange | kExpectFailure);
  }

  // Helper to test for how we handle trailing junk
  void TestCorruptUrl(const char* junk, bool should_fetch_ok) {
    const char kInput[] = " div { } ";
    const char kOutput[] = "div{}";
    // Compute normal version
    ValidateRewriteExternalCss("rep", kInput, kOutput,
                              kExpectChange | kExpectSuccess);

    // Fetch with messed up extension
    ResourceNamer namer;
    GetNamerForCss("rep", kOutput, &namer);
    std::string css_url = ExpectedUrlForNamer(namer);
    std::string output;
    EXPECT_EQ(should_fetch_ok,
              ServeResourceUrl(StrCat(css_url, junk), &output));

    // Now see that output is correct
    ValidateRewriteExternalCss(
        "rep", kInput, kOutput,
        kExpectChange | kExpectSuccess | kNoClearFetcher | kNoStatCheck);
  }

  SimpleStats statistics_;
  Variable* num_files_minified_;
  Variable* minified_bytes_saved_;
  Variable* num_parse_failures_;
};

TEST_F(CssFilterTest, SimpleRewriteCssTest) {
  std::string input_style =
      ".background_blue { background-color: #f00; }\n"
      ".foreground_yellow { color: yellow; }\n";
  std::string output_style =
      ".background_blue{background-color:red}"
      ".foreground_yellow{color:#ff0}";

  ValidateRewrite("rewrite_css", input_style, output_style);
}

// Make sure we can deal with 0 character nodes between open and close of style.
TEST_F(CssFilterTest, RewriteEmptyCssTest) {
  ValidateNoChange("rewrite_empty_css", "");
}

// Make sure we do not recompute external CSS when re-processing an already
// handled page
TEST_F(CssFilterTest, RewriteRepeated) {
  ValidateRewriteExternalCss("rep", " div { } ", "div{}",
                             kExpectChange | kExpectSuccess);
  int inserts_before = lru_cache_->num_inserts();
  ValidateRewriteExternalCss("rep", " div { } ", "div{}",
                             kExpectChange | kExpectSuccess | kNoStatCheck);
  int inserts_after = lru_cache_->num_inserts();
  EXPECT_EQ(0, lru_cache_->num_identical_reinserts());
  EXPECT_EQ(inserts_before, inserts_after);
  // We expect num_files_minified_ to be reset to 0 by
  // ValidateRewriteExternalCss and left there since we should not re-minimize.
  EXPECT_EQ(0, num_files_minified_->Get());
}

// Make sure we do not reparse external CSS when we know it already has
// a parse error
TEST_F(CssFilterTest, RewriteRepeatedParseError) {
  const char kInvalidCss[] = "}}";
  ValidateRewriteExternalCss("rep_fail", kInvalidCss, "",
                             kExpectNoChange | kExpectFailure);
  ValidateRewriteExternalCss("rep_fail", kInvalidCss, "",
                             kExpectNoChange | kExpectFailure | kNoStatCheck);
  // We expect num_parse_failures_ to be reset to 0 at the beginning of the
  // test, and to remain at it since we should remember the failure
  EXPECT_EQ(0, num_parse_failures_->Get());
}

// Make sure we don't change CSS with errors. Note: We can move these tests
// to expected rewrites if we find safe ways to edit them.
TEST_F(CssFilterTest, NoRewriteParseError) {
  ValidateFailParse("non_unicode_charset",
                    "a { font-family: \"\xCB\xCE\xCC\xE5\"; }");
  // From http://www.baidu.com/
  ValidateFailParse("non_unicode_baidu",
                    "#lk span {font:14px \"\xCB\xCE\xCC\xE5\"}");

  ValidateFailParse("bad_char_in_selector", ".bold: { font-weight: bold }");
}

// Make sure bad requests do not corrupt our extension.
TEST_F(CssFilterTest, NoExtensionCorruption) {
  TestCorruptUrl("%22", false);
}

TEST_F(CssFilterTest, NoQueryCorruption) {
  TestCorruptUrl("?query", true);
}

TEST_F(CssFilterTest, RewriteVariousCss) {
  // Distilled examples.
  const char* good_examples[] = {
    "a.b #c.d e#d,f:g>h+i>j{color:red}",  // .#,>+: in selectors
    "a{border:solid 1px #ccc}",  // Multiple values declaration
    "a{border:none!important}",  // !important
    "a{background-image:url(foo.png)}",  // url
    "a{background-position:-19px 60%}",  // negative position
    "a{margin:0}",  // 0 w/ no units
    "a{padding:0.01em 0.25em}",  // fractions and em
    "a{-moz-border-radius-topleft:0}",  // Browser-specific (-moz)
    ".ds{display:-moz-inline-box}",
    "a{background:none}",  // CSS Parser used to expand this.
    // http://code.google.com/p/modpagespeed/issues/detail?id=5
    "a{font-family:trebuchet ms}",  // Keep space between trebuchet and ms.
    // http://code.google.com/p/modpagespeed/issues/detail?id=121
    "a{color:inherit}",
    // Added for code coverage.
    "@import url(http://www.example.com)",
    "@media a,b{a{color:red}}",
    "a{content:\"Odd chars: \\(\\)\\,\\\"\\\'\"}",
    "img{clip:rect(0px,60px,200px,0px)}",
    // CSS3-style pseudo-elements.
    "p.normal::selection{background:#c00;color:#fff}",
    "::-moz-focus-inner{border:0}",
    "input::-webkit-input-placeholder{color:#ababab}"
    // http://code.google.com/p/modpagespeed/issues/detail?id=51
    "a{box-shadow:-1px -2px 2px rgba(0,0,0,0.15)}",  // CSS3 rgba
    // http://code.google.com/p/modpagespeed/issues/detail?id=66
    "a{-moz-transform:rotate(7deg)}",
    // Microsoft syntax values.
    "a{filter:progid:DXImageTransform.Microsoft.Alpha(Opacity=80)}",
    // Found in the wild:
    "a{width:overflow:hidden}",
    };

  for (int i = 0; i < arraysize(good_examples); ++i) {
    std::string id = StringPrintf("distilled_css_good%d", i);
    ValidateNoChange(id, good_examples[i]);
  }

  const char* fail_examples[] = {
    // CSS3 media "and (max-width: 290px).
    // http://code.google.com/p/modpagespeed/issues/detail?id=50
    "@media screen and (max-width: 290px) { a { color:red } }",

    // Slashes in value list.
    ".border8 { border-radius: 36px / 12px; }"

    // http://code.google.com/p/modpagespeed/issues/detail?id=220
    // See https://developer.mozilla.org/en/CSS/-moz-transition-property
    // and http://www.webkit.org/blog/138/css-animation/
    "a { -webkit-transition-property:opacity,-webkit-transform; }",

    // Should fail (bad syntax):
    "a { font:bold verdana 10px; }",
    };

  for (int i = 0; i < arraysize(fail_examples); ++i) {
    std::string id = StringPrintf("distilled_css_fail%d", i);
    ValidateFailParse(id, fail_examples[i]);
  }
}

// Things we could be optimizing.
// This test will fail when we start optimizing these thing.
TEST_F(CssFilterTest, ToOptimize) {
  const char* examples[][2] = {
    // Noticed from YUI minification.
    { "td { line-height: 0.8em; }",
      // Could be: "td{line-height:.8em}"
      "td{line-height:0.8em}", },
    { ".gb1, .gb3 {}",
      // Could be: ""
      ".gb1,.gb3{}", },
    { ".lst:focus { outline:none; }",
      // Could be: ".lst:focus{outline:0}"
      ".lst:focus{outline:none}", },
  };

  for (int i = 0; i < arraysize(examples); ++i) {
    std::string id = StringPrintf("to_optimize_%d", i);
    ValidateRewrite(id, examples[i][0], examples[i][1]);
  }
}

// Test more complicated CSS.
TEST_F(CssFilterTest, ComplexCssTest) {
  // Real-world examples. Picked out of Wikipedia's CSS.
  const char* examples[][2] = {
    { "#userlogin, #userloginForm {\n"
      "  border: solid 1px #cccccc;\n"
      "  padding: 1.2em;\n"
      "  float: left;\n"
      "}\n",

      "#userlogin,#userloginForm{border:solid 1px #ccc;padding:1.2em;"
      "float:left}"},

    { "h3 .editsection { font-size: 76%; font-weight: normal; }\n",
      "h3 .editsection{font-size:76%;font-weight:normal}"},

    { "div.magnify a, div.magnify img {\n"
      "  display: block;\n"
      "  border: none !important;\n"
      "  background: none !important;\n"
      "}\n",

      "div.magnify a,div.magnify img{display:block;border:none!important;"
      "background:none!important}"},

    { "#ca-watch.icon a:hover {\n"
      "  background-image: url('images/watch-icons.png?1');\n"
      "  background-position: -19px 60%;\n"
      "}\n",

      "#ca-watch.icon a:hover{background-image:url(images/watch-icons.png?1);"
      "background-position:-19px 60%}"},

    { "body {\n"
      "  background: White;\n"
      "  /*font-size: 11pt !important;*/\n"
      "  color: Black;\n"
      "  margin: 0;\n"
      "  padding: 0;\n"
      "}\n",

      "body{background:#fff;color:#000;margin:0;padding:0}"},

    { ".suggestions-result{\n"
      "  color:black;\n"
      "  color:WindowText;\n"
      "  padding:0.01em 0.25em;\n"
      "}\n",

      // TODO(sligocki): Do we care about color:WindowText?
      //".suggestions-result{color:#000;color:WindowText;padding:0.01em 0.25em}"

      ".suggestions-result{color:#000;color:#000;padding:0.01em 0.25em}"},

    { ".ui-corner-tl { -moz-border-radius-topleft: 0; -webkit-border-top-left"
      "-radius: 0; }\n",

      ".ui-corner-tl{-moz-border-radius-topleft:0;-webkit-border-top-left"
      "-radius:0}"},

    { ".ui-tabs .ui-tabs-nav li.ui-tabs-selected a, .ui-tabs .ui-tabs-nav li."
      "ui-state-disabled a, .ui-tabs .ui-tabs-nav li.ui-state-processing a { "
      "cursor: pointer; }\n",

      ".ui-tabs .ui-tabs-nav li.ui-tabs-selected a,.ui-tabs .ui-tabs-nav "
      "li.ui-state-disabled a,.ui-tabs .ui-tabs-nav li.ui-state-processing a{"
      "cursor:pointer}"},

    { ".ui-datepicker-cover {\n"
      "  display: none; /*sorry for IE5*/\n"
      "  display/**/: block; /*sorry for IE5*/\n"
      "  position: absolute; /*must have*/\n"
      "  z-index: -1; /*must have*/\n"
      "  filter: mask(); /*must have*/\n"
      "  top: -4px; /*must have*/\n"
      "  left: -4px; /*must have*/\n"
      "  width: 200px; /*must have*/\n"
      "  height: 200px; /*must have*/\n"
      "}\n",

      // TODO(sligocki): Should we preserve the dispaly/**/:?
      //".ui-datepicker-cover{display:none;display/**/:block;position:absolute;"
      //"z-index:-1;filter:mask();top:-4px;left:-4px;width:200px;height:200px}"

      ".ui-datepicker-cover{display:none;display:block;position:absolute;"
      "z-index:-1;filter:mask();top:-4px;left:-4px;width:200px;height:200px}" },

    { ".shift {\n"
      "  -moz-transform: rotate(7deg);\n"
      "  -webkit-transform: rotate(7deg);\n"
      "  -moz-transform: skew(-25deg);\n"
      "  -webkit-transform: skew(-25deg);\n"
      "  -moz-transform: scale(0.5);\n"
      "  -webkit-transform: scale(0.5);\n"
      "  -moz-transform: translate(3em, 0);\n"
      "  -webkit-transform: translate(3em, 0);\n"
      "}\n",

      ".shift{-moz-transform:rotate(7deg);-webkit-transform:rotate(7deg);"
      "-moz-transform:skew(-25deg);-webkit-transform:skew(-25deg);"
      "-moz-transform:scale(0.5);-webkit-transform:scale(0.5);"
      "-moz-transform:translate(3em,0);-webkit-transform:translate(3em,0)}" },

    // http://code.google.com/p/modpagespeed/issues/detail?id=121
    { "body { font: 2em sans-serif; }", "body{font:2em sans-serif}" },
    { "body { font: 0.75em sans-serif; }", "body{font:0.75em sans-serif}" },

    // http://code.google.com/p/modpagespeed/issues/detail?id=128
    { "#breadcrumbs ul { list-style-type: none; }",
      "#breadcrumbs ul{list-style-type:none}" },

    // http://code.google.com/p/modpagespeed/issues/detail?id=126
    // Extra spaces assure that we actually rewrite the first arg even if
    // font: is expanded by parser.
    { ".menu { font: menu; }               ", ".menu{font:menu}" },

    // http://code.google.com/p/modpagespeed/issues/detail?id=211
    { "#some_id {\n"
      "background: #cccccc url(images/picture.png) 50% 50% repeat-x;\n"
      "}\n",

      "#some_id{background:#ccc url(images/picture.png) 50% 50% repeat-x}" },

    { ".gac_od { border-color: -moz-use-text-color #E7E7E7 #E7E7E7 "
      "-moz-use-text-color; }",

      ".gac_od{border-color:-moz-use-text-color #e7e7e7 #e7e7e7 "
      "-moz-use-text-color}" },

    // Star/Underscore hack
    // See: http://developer.yahoo.com/yui/compressor/css.html
    { "a { *padding-bottom: 0px; }",
      "a{*padding-bottom:0px}" },

    { "#element { width: 1px; _width: 3px; }",
      "#element{width:1px;_width:3px}" },

    // Complex nested functions
    { "body {\n"
      "  background-image:-webkit-gradient(linear, 50% 0%, 50% 100%,"
      " from(rgb(232, 237, 240)), to(rgb(252, 252, 253)));\n"
      "  color: red;\n"
      "}\n"
      ".foo { color: rgba(1, 2, 3, 0.4); }\n",

      "body{background-image:-webkit-gradient(linear,50% 0%,50% 100%,"
      "from(#e8edf0),to(#fcfcfd));color:red}.foo{color:rgba(1,2,3,0.4)}" },

    // Counters
    // http://www.w3schools.com/CSS/tryit.asp?filename=trycss_gen_counter-reset
    { "body {counter-reset:section;}\n"
      "h1 {counter-reset:subsection;}\n"
      "h1:before\n"
      "{\n"
      "counter-increment:section;\n"
      "content:\"Section \" counter(section) \". \";\n"
      "}\n"
      "h2:before \n"
      "{\n"
      "counter-increment:subsection;\n"
      "content:counter(section) \".\" counter(subsection) \" \";\n"
      "}\n",

      "body{counter-reset:section}"
      "h1{counter-reset:subsection}"
      "h1:before{counter-increment:section;"
      "content:\"Section \" counter(section) \". \"}"
      "h2:before{counter-increment:subsection;"
      "content:counter(section) \".\" counter(subsection) \" \"}" },

    // Don't lowercase font names.
    { "a { font-family: Arial; }",
      "a{font-family:Arial}" },
  };

  for (int i = 0; i < arraysize(examples); ++i) {
    std::string id = StringPrintf("complex_css%d", i);
    ValidateRewrite(id, examples[i][0], examples[i][1]);
  }

  const char* parse_fail_examples[] = {
    // http://code.google.com/p/modpagespeed/issues/detail?id=220
    ".mui-navbar-wrap, .mui-navbar-clone {"
    "opacity:1;-webkit-transform:translateX(0);"
    "-webkit-transition-property:opacity,-webkit-transform;"
    "-webkit-transition-duration:400ms;}",
    };

  for (int i = 0; i < arraysize(parse_fail_examples); ++i) {
    std::string id = StringPrintf("complex_css_parse_fail%d", i);
    ValidateFailParse(id, parse_fail_examples[i]);
  }
}

// These tests are to make sure our TTL considers that of subresources.
class CssFilterSubresourceTest : public CssFilterTest {
 public:
  virtual void SetUp() {
    // We setup the options before the upcall so that the
    // CSS filter is created aware of these.
    options_.EnableFilter(RewriteOptions::kExtendCache);
    options_.EnableFilter(RewriteOptions::kRewriteImages);
    CssFilterTest::SetUp();

    // As we use invalid payloads, we expect image rewriting to
    // fail but cache extension to succeed.
    InitResponseHeaders("a.png", kContentTypePng, "notapng", 10);
    InitResponseHeaders("b.png", kContentTypePng, "notbpng", 20);
  }

  void ValidateExpirationTime(const char* id, const char* output,
                              int64 expected_expire_ms) {
    ResourceNamer namer;
    GetNamerForCss(id, output, &namer);
    std::string css_url = ExpectedUrlForNamer(namer);

    // See what cache information we have
    scoped_ptr<OutputResource> output_resource(
        rewrite_driver_.CreateOutputResourceWithPath(
            kTestDomain, RewriteDriver::kCssFilterId, StrCat(id, ".css"),
            &kContentTypeCss, RewriteDriver::kRewrittenResource));
    ASSERT_TRUE(output_resource.get() != NULL);
    EXPECT_EQ(css_url, output_resource->url());
    ASSERT_TRUE(output_resource->cached_result() != NULL);

    EXPECT_EQ(expected_expire_ms,
              output_resource->cached_result()->origin_expiration_time_ms());
  }
};

// Test to make sure expiration time for cached result is the
// smallest of subresource and CSS times, not just CSS time.
TEST_F(CssFilterSubresourceTest, SubResourceDepends) {
  const char kInput[] = "div { background-image: url(a.png); }"
                        "span { background-image: url(b.png); }";
  const char kOutput[] =
      "div{background-image:url(http://test.com/a.png.pagespeed.ce.0.png)}"
      "span{background-image:url(http://test.com/b.png.pagespeed.ce.0.png)}";

  // Here we don't use the other contexts since it has different
  // synchronicity, and we presently do best-effort for loaded subresources
  // even in Fetch.
  ValidateRewriteExternalCss(
      "ext", kInput, kOutput, kNoOtherContexts | kNoClearFetcher |
                              kExpectChange | kExpectSuccess);

  // 10 is the smaller of expiration times of a.png, b.png and ext.css
  ValidateExpirationTime("ext", kOutput, 10 * Timer::kSecondMs);
}

// Test to make sure we don't cache for long if the rewrite was based
// on not-yet-loaded resources.
TEST_F(CssFilterSubresourceTest, SubResourceDependsNotYetLoaded) {
  scoped_ptr<WaitUrlAsyncFetcher> wait_fetcher(SetupWaitFetcher());

  // Disable atime simulation so that the clock doesn't move on us.
  file_system_.set_atime_enabled(false);

  const char kInput[] = "div { background-image: url(a.png); }"
                        "span { background-image: url(b.png); }";
  const char kOutput[] = "div{background-image:url(a.png)}"
                        "span{background-image:url(b.png)}";

  // At first try, not even the CSS gets loaded, so nothing gets
  // changed at all.
  ValidateRewriteExternalCss(
      "wip", kInput, kInput, kNoOtherContexts | kNoClearFetcher |
                             kExpectNoChange | kExpectSuccess);

  // Get the CSS to load (resources are still unavailable).
  wait_fetcher->CallCallbacks();
  ValidateRewriteExternalCss(
      "wip", kInput, kOutput, kNoOtherContexts | kNoClearFetcher |
                              kExpectChange | kExpectSuccess);

  // Since resources haven't loaded, the output cache should have a very small
  // expiration time.
  ValidateExpirationTime("wip", kOutput, Timer::kSecondMs);

  // Make sure the subresource callbacks fire for leak cleanliness
  wait_fetcher->CallCallbacks();
}

}  // namespace

}  // namespace net_instaweb
