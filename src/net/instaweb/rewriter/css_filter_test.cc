/**
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

// Unit-test the html rewriter
#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/css_filter.h"
#include "net/instaweb/rewriter/public/css_move_to_head_filter.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/img_tag_scanner.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/simple_stats.h"
#include <string>

namespace net_instaweb {

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

  // Check that inline CSS get's rewritten correctly.
  void ValidateRewriteInlineCss(const StringPiece& id,
                                const StringPiece& css_input,
                                const StringPiece& expected_css_output,
                                bool expect_change,
                                bool expect_failure) {
    static const char prefix[] =
        "<head>\n"
        "  <title>Example style outline</title>\n"
        "  <!-- Style starts here -->\n"
        "  <style type='text/css'>";
    static const char suffix[] = "</style>\n"
        "  <!-- Style ends here -->\n"
        "</head>";

    std::string html_input  = StrCat(prefix, css_input, suffix);
    std::string html_output = StrCat(prefix, expected_css_output, suffix);

    // Reset stats
    num_files_minified_->Set(0);
    minified_bytes_saved_->Set(0);
    num_parse_failures_->Set(0);

    // Rewrite
    ValidateExpected(id, html_input, html_output);

    // Check stats
    if (expect_change) {
      EXPECT_EQ(1, num_files_minified_->Get());
      EXPECT_EQ(css_input.size() - expected_css_output.size(),
                minified_bytes_saved_->Get());
      EXPECT_EQ(0, num_parse_failures_->Get());
    } else {
      EXPECT_EQ(0, num_files_minified_->Get());
      EXPECT_EQ(0, minified_bytes_saved_->Get());
      EXPECT_EQ((expect_failure ? 1 : 0), num_parse_failures_->Get()) << id;
    }
  }

  // Check that external CSS gets rewritten correctly.
  void ValidateRewriteExternalCss(const StringPiece& id,
                                  const std::string& css_input,
                                  const std::string& expected_css_output,
                                  bool expect_change,
                                  bool expect_failure) {
    // TODO(sligocki): Allow arbitrary URLs.
    std::string css_url = StrCat("http://test.com/", id, ".css");

    Hasher* hasher = &mock_hasher_;
    // TODO(sligocki): Allow testing with other hashers.
    //resource_manager_->SetHasher(hasher);

    // Set input file.
    mock_url_fetcher_.Clear();
    InitMetaData(StrCat(id, ".css"), kContentTypeCss, css_input, 300);

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
    namer.set_id("cf");
    namer.set_hash(hasher->Hash(expected_css_output));
    namer.set_ext("css");
    // TODO(sligocki): Derive these from css_url the "right" way.
    std::string url_prefix = "http://test.com/";
    namer.set_name(StrCat(id, ".css"));

    std::string expected_new_url = StrCat(url_prefix, namer.Encode());

    if (expect_change) {
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

    // Check stats
    if (expect_change) {
      EXPECT_EQ(1, num_files_minified_->Get());
      EXPECT_EQ(css_input.size() - expected_css_output.size(),
                minified_bytes_saved_->Get());
      EXPECT_EQ(0, num_parse_failures_->Get());
    } else {
      EXPECT_EQ(0, num_files_minified_->Get());
      EXPECT_EQ(0, minified_bytes_saved_->Get());
      EXPECT_EQ((expect_failure ? 1 : 0), num_parse_failures_->Get()) << id;
    }

    // Check CSS output.
    if (expect_change) {
      std::string actual_output;
      // TODO(sligocki): This will only work with mock_hasher.
      EXPECT_TRUE(ServeResource(url_prefix,
                                namer.id(), namer.name(), namer.ext(),
                                &actual_output));
      EXPECT_EQ(expected_css_output, actual_output);

      // Serve from new context.
      ServeResourceFromManyContexts(expected_new_url,
                                    RewriteOptions::kRewriteCss,
                                    hasher, expected_css_output);
    }
  }

  void ValidateRewrite(const StringPiece& id,
                       const std::string& css_input,
                       const std::string& gold_output) {
    ValidateRewriteInlineCss(StrCat(id, "-inline"),
                             css_input, gold_output, true, false);
    ValidateRewriteExternalCss(StrCat(id, "-external"),
                               css_input, gold_output, true, false);
  }

  void ValidateNoChange(const StringPiece& id, const std::string& css_input) {
    ValidateRewriteInlineCss(StrCat(id, "-inline"),
                             css_input, css_input, false, false);
    ValidateRewriteExternalCss(StrCat(id, "-external"),
                               css_input, "", false, false);
  }

  void ValidateFailParse(const StringPiece& id, const std::string& css_input) {
    ValidateRewriteInlineCss(StrCat(id, "-inline"),
                             css_input, css_input, false, true);
    ValidateRewriteExternalCss(StrCat(id, "-external"),
                               css_input, "", false, true);
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

// Make sure we don't change CSS with errors. Note: We can move these tests
// to expected rewrites if we find safe ways to edit them.
TEST_F(CssFilterTest, NoRewriteParseError) {
  ValidateFailParse("non_unicode_charset",
                    "a { font-family: \"\xCB\xCE\xCC\xE5\"; }");
  // From http://www.baidu.com/
  ValidateFailParse("non_unicode_baidu",
                    "#lk span {font:14px \"\xCB\xCE\xCC\xE5\"}");
  // From http://www.yahoo.com/
  const char confusing_value[] =
      "a { background-image:-webkit-gradient(linear, 50% 0%, 50% 100%,"
      " from(rgb(232, 237, 240)), to(rgb(252, 252, 253)));}";
  ValidateFailParse("non_standard_value", confusing_value);

  ValidateFailParse("bad_char_in_selector", ".bold: { font-weight: bold }");
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
    "a{background:none}",  // CSS Parser used to expand this.
    // http://code.google.com/p/modpagespeed/issues/detail?id=5
    "a{font-family:trebuchet ms}",  // Keep space between trebuchet and ms.
    // http://code.google.com/p/modpagespeed/issues/detail?id=121
    "a{color:inherit}",
    };

  for (int i = 0; i < arraysize(good_examples); ++i) {
    std::string id = StringPrintf("distilled_css_good%d", i);
    ValidateNoChange(id, good_examples[i]);
  }

  const char* fail_examples[] = {
    // http://code.google.com/p/modpagespeed/issues/detail?id=50
    "@media screen and (max-width:290px){a{color:red}}",  // CSS3 "and (...)"
    // http://code.google.com/p/modpagespeed/issues/detail?id=51
    "a{box-shadow:-1px -2px 2px rgba(0, 0, 0, .15)}",  // CSS3 rgba
    // http://code.google.com/p/modpagespeed/issues/detail?id=66
    "a{-moz-transform:rotate(7deg)}",
    };

  for (int i = 0; i < arraysize(fail_examples); ++i) {
    std::string id = StringPrintf("distilled_css_fail%d", i);
    ValidateFailParse(id, fail_examples[i]);
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

    // http://code.google.com/p/modpagespeed/issues/detail?id=121
    { "body { font: 2em sans-serif; }", "body{font:2em sans-serif}" },
    { "body { font: 0.75em sans-serif; }", "body{font:0.75em sans-serif}" },
  };

  for (int i = 0; i < arraysize(examples); ++i) {
    std::string id = StringPrintf("complex_css%d", i);
    ValidateRewrite(id, examples[i][0], examples[i][1]);
  }

  const char* parse_fail_examples[] = {
    ".ui-datepicker-cover {\n"
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

    // Right now we bail on parsing the above. Could probably be minified to:
    //".ui-datepicker-cover{display:none;display/**/:block;position:absolute;"
    //"z-index:-1;filter:mask();top:-4px;left:-4px;width:200px;height:200px}"
    // TODO(sligocki): When this is parsed correctly, move it up to examples[][]

    ".shift {\n"
    "  -moz-transform: rotate(7deg);\n"
    "  -webkit-transform: rotate(7deg);\n"
    "  -moz-transform: skew(-25deg);\n"
    "  -webkit-transform: skew(-25deg);\n"
    "  -moz-transform: scale(0.5);\n"
    "  -webkit-transform: scale(0.5);\n"
    "  -moz-transform: translate(3em, 0);\n"
    "  -webkit-transform: translate(3em, 0);\n"
    "}\n",

    // Right now we bail on parsing the above. Could probably be minified to:
    //".shift{-moz-transform:rotate(7deg);-webkit-transform:rotate(7deg);"
    //"-moz-transform:skew(-25deg);-webkit-transform:skew(-25deg);"
    //"-moz-transform:scale(0.5);-webkit-transform:scale(0.5);"
    //"-moz-transform:translate(3em,0);-webkit-transform:translate(3em,0);}"
    // TODO(sligocki): When this is parsed correctly, move it up to examples[][]
  };

  for (int i = 0; i < arraysize(parse_fail_examples); ++i) {
    std::string id = StringPrintf("complex_css_parse_fail%d", i);
    ValidateFailParse(id, parse_fail_examples[i]);
  }

}

}  // namespace net_instaweb
