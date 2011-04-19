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
#include "net/instaweb/rewriter/public/css_rewrite_test_base.h"

namespace net_instaweb {

namespace {

class CssFilterTest : public CssRewriteTestBase {
};

TEST_F(CssFilterTest, SimpleRewriteCssTest) {
  GoogleString input_style =
      ".background_blue { background-color: #f00; }\n"
      ".foreground_yellow { color: yellow; }\n";
  GoogleString output_style =
      ".background_blue{background-color:red}"
      ".foreground_yellow{color:#ff0}";

  ValidateRewrite("rewrite_css", input_style, output_style);
}

TEST_F(CssFilterTest, UrlTooLong) {
  // Make the filename maximum size, so we cannot rewrite it.
  // -4 because .css will be appended.
  GoogleString filename(options_.max_url_segment_size() - 4, 'z');
  // If filename wasn't too long, this would be rewritten (like in
  // SimpleRewriteCssTest).
  GoogleString input_style =
      ".background_blue { background-color: #f00; }\n"
      ".foreground_yellow { color: yellow; }\n";
  ValidateRewriteExternalCss(filename, input_style, input_style,
                             kExpectNoChange | kExpectSuccess);
}

// Make sure we can deal with 0 character nodes between open and close of style.
TEST_F(CssFilterTest, RewriteEmptyCssTest) {
  ValidateRewriteInlineCss("rewrite_empty_css-inline", "", "",
                           kExpectChange | kExpectSuccess | kNoStatCheck);
  // Note: We must check stats ourselves because, for technical reasons,
  // empty inline styles are not treated as being rewritten at all.
  //EXPECT_EQ(0, num_files_minified_->Get());
  EXPECT_EQ(0, minified_bytes_saved_->Get());
  EXPECT_EQ(0, num_parse_failures_->Get());

  ValidateRewriteExternalCss("rewrite_empty_css-external", "", "",
                             kExpectChange | kExpectSuccess | kNoStatCheck);
  EXPECT_EQ(0, minified_bytes_saved_->Get());
  EXPECT_EQ(0, num_parse_failures_->Get());
}

// Make sure we do not recompute external CSS when re-processing an already
// handled page.
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
// a parse error.
TEST_F(CssFilterTest, RewriteRepeatedParseError) {
  const char kInvalidCss[] = "@media }}";
  // Note: It is important that these both have the same id so that the
  // generated CSS file names are identical.
  // TODO(sligocki): This is sort of annoying for error reporting which
  // is suposed to use id to uniquely distinguish which test was running.
  ValidateRewriteExternalCss("rep_fail", kInvalidCss, "",
                             kExpectNoChange | kExpectFailure);
  // First time, we fail to parse.
  EXPECT_EQ(1, num_parse_failures_->Get());
  ValidateRewriteExternalCss("rep_fail", kInvalidCss, "",
                             kExpectNoChange | kExpectFailure | kNoStatCheck);
  // Second time, we remember failure and so don't try to reparse.
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
  // TODO(sligocki): Get these tests to pass with setlocale.
  //EXPECT_TRUE(setlocale(LC_ALL, "tr_TR.utf8"));
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
    // TODO(sligocki): Get rid of the " ;"?
    "@import url(http://www.example.com) ;",
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
    GoogleString id = StringPrintf("distilled_css_good%d", i);
    ValidateRewrite(id, good_examples[i], good_examples[i]);
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

    // IE8 Hack \0/
    // See http://dimox.net/personal-css-hacks-for-ie6-ie7-ie8/
    "a { color: red\\0/; }",

    // Parameterized pseudo-selector.
    "div:nth-child(1n) { color: red; }",

    // Should fail (bad syntax):
    "a { font:bold verdana 10px; }",
    };

  for (int i = 0; i < arraysize(fail_examples); ++i) {
    GoogleString id = StringPrintf("distilled_css_fail%d", i);
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
    GoogleString id = StringPrintf("to_optimize_%d", i);
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

    // TODO(sligocki): This should raise an error and fail to rewrite.
    { "}}", "" },

    // Don't drop precision on large integers (this is 2^31 + 1 which is
    // just larger than larges z-index accepted by chrome, 2^31 - 1).
    { "#foo { z-index: 2147483649; }",
      // Not "#foo{z-index:2.14748e+09}"
      "#foo{z-index:2147483649}" },

    { "#foo { z-index: 123456789012345678901234567890; }",
      // TODO(sligocki): "#foo{z-index:12345678901234567890}" },
      "#foo{z-index:1.234567890123457e+29}" },
  };

  for (int i = 0; i < arraysize(examples); ++i) {
    GoogleString id = StringPrintf("complex_css%d", i);
    ValidateRewrite(id, examples[i][0], examples[i][1]);
  }

  const char* parse_fail_examples[] = {
    // http://code.google.com/p/modpagespeed/issues/detail?id=220
    ".mui-navbar-wrap, .mui-navbar-clone {"
    "opacity:1;-webkit-transform:translateX(0);"
    "-webkit-transition-property:opacity,-webkit-transform;"
    "-webkit-transition-duration:400ms;}",

    // IE 8 hack \0/.
    ".gbxms{background-color:#ccc;display:block;position:absolute;"
    "z-index:1;top:-1px;left:-2px;right:-2px;bottom:-2px;opacity:.4;"
    "-moz-border-radius:3px;"
    "filter:progid:DXImageTransform.Microsoft.Blur(pixelradius=5);"
    "*opacity:1;*top:-2px;*left:-5px;*right:5px;*bottom:4px;"
    "-ms-filter:\"progid:DXImageTransform.Microsoft.Blur(pixelradius=5)\";"
    "opacity:1\\0/;top:-4px\\0/;left:-6px\\0/;right:5px\\0/;bottom:4px\\0/}",
    };

  for (int i = 0; i < arraysize(parse_fail_examples); ++i) {
    GoogleString id = StringPrintf("complex_css_parse_fail%d", i);
    ValidateFailParse(id, parse_fail_examples[i]);
  }
}

// Most tests are run with set_always_rewrite_css(true),
// but all production use has set_always_rewrite_css(false).
// This test makes sure that setting to false still does what we intend.
TEST_F(CssFilterTest, NoAlwaysRewriteCss) {
  // When we force always_rewrite_css, we can expand some statements.
  // Note: when this example is fixed in the minifier, this test will break :/
  options_.set_always_rewrite_css(true);
  ValidateRewrite("expanding_example",
                  "@import url(http://www.example.com)",
                  "@import url(http://www.example.com) ;");
  // With it set false, we do not expand CSS (as long as we didn't do anything
  // else, like rewrite sub-resources.
  options_.set_always_rewrite_css(false);
  ValidateRewrite("non_expanding_example",
                  "@import url(http://www.example.com)",
                  "@import url(http://www.example.com)",
                  kExpectNoChange | kExpectSuccess);
  // Here: kExpectSuccess means there was no error. (Minification that
  // actually expands the statement is not considered an error.)

  // When we force always_rewrite_css, we allow rewriting something to nothing.
  // Note: when this example is fixed in the parser, this test will break :/
  options_.set_always_rewrite_css(true);
  ValidateRewrite("contracting_example",     "}}", "");
  // With it set false, we do not allow something to be minified to nothing.
  options_.set_always_rewrite_css(false);
  ValidateRewrite("non_contracting_example", "}}", "}}",
                  kExpectNoChange | kExpectFailure);
}

}  // namespace

}  // namespace net_instaweb
