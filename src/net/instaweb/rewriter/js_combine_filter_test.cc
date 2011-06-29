/*
 * Copyright 2011 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)
//
// Unit tests for JsCombineFilter.

#include "net/instaweb/rewriter/public/js_combine_filter.h"

#include <vector>

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const char kJsUrl1[] = "a.js";
const char kJsUrl2[] = "b.js";
const char kJsUrl3[] = "c.js";
const char kStrictUrl1[] = "strict1.js";
const char kStrictUrl2[] = "strict2.js";
const char kJsText1[] = "// script1\nvar a=\"hello\\nsecond line\"";
const char kJsText2[] = "// script2\r\nvar b=42;\n";
const char kJsText3[] = "var x = 42;\nvar y = 31459;\n";
const char kStrictText1[] = "'use strict'; var x = 32;";
const char kStrictText2[] = "\"use strict\"; var x = 42;";
const char kEscapedJs1[] =
    "\"// script1\\nvar a=\\\"hello\\\\nsecond line\\\"\"";
const char kEscapedJs2[] = "\"// script2\\r\\nvar b=42;\\n\"";

}  // namespace

// Test fixture for JsCombineFilter unit tests.
class JsCombineFilterTest : public ResourceManagerTestBase,
                            public ::testing::WithParamInterface<bool> {
 public:
  struct ScriptInfo {
    HtmlElement* element;
    GoogleString url;  // if empty, the <script> didn't have a src
    GoogleString text_content;
  };

  typedef std::vector<ScriptInfo> ScriptInfoVector;

  // Helper class that collects all the script elements in html and
  // their sources and bodies.
  //
  // It also verifies that is will be no nesting of things inside scripts.
  class ScriptCollector : public EmptyHtmlFilter {
   public:
    explicit ScriptCollector(ScriptInfoVector* output)
        : output_(output), active_script_(NULL) {
    }

    virtual void StartElement(HtmlElement* element) {
      EXPECT_EQ(NULL, active_script_);
      if (element->keyword() == HtmlName::kScript) {
        active_script_ = element;
        script_content_.clear();
      }
    }

    virtual void Characters(HtmlCharactersNode* characters) {
      script_content_ += characters->contents();
    }

    virtual void EndElement(HtmlElement* element) {
      if (element->keyword() == HtmlName::kScript) {
        ScriptInfo info;
        info.element = element;
        const char* url_cstr = element->AttributeValue(HtmlName::kSrc);
        if (url_cstr != NULL) {
          info.url = GoogleString(url_cstr);
        }
        info.text_content = script_content_;
        output_->push_back(info);
        active_script_ = NULL;
      }
    }

    virtual const char* Name() const { return "ScriptCollector"; }

   private:
    ScriptInfoVector* output_;
    GoogleString script_content_;  // contents of any script tag, if any.
    HtmlElement* active_script_;  // any script we're in.

    DISALLOW_COPY_AND_ASSIGN(ScriptCollector);
  };

  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    bool async_rewrites = GetParam();
    UseMd5Hasher();
    SetDefaultLongCacheHeaders(&kContentTypeJavascript, &default_js_header_);
    SimulateJsResource(kJsUrl1, kJsText1);
    SimulateJsResource(kJsUrl2, kJsText2);
    SimulateJsResource(kJsUrl3, kJsText3);
    SimulateJsResource(kStrictUrl1, kStrictText1);
    SimulateJsResource(kStrictUrl2, kStrictText2);

    rewrite_driver()->SetAsynchronousRewrites(async_rewrites);
    filter_ = new JsCombineFilter(rewrite_driver(),
                                  RewriteDriver::kJavascriptCombinerId);
    AddRewriteFilter(filter_);
    rewrite_driver()->AddFilters();
    // Some tests need an another domain, with (different)source files on it as
    // well.
    GoogleString test_domain(kTestDomain);
    if (EndsInSlash(test_domain)) {
      test_domain.resize(test_domain.length() - 1);
    }
    other_domain_ = StrCat(test_domain, ".us/");
    SimulateJsResourceOnDomain(other_domain_, kJsUrl1, "othera");
    SimulateJsResourceOnDomain(other_domain_, kJsUrl2, "otherb");
  }

  void SimulateJsResource(const StringPiece& url, const StringPiece& text) {
    SimulateJsResourceOnDomain(kTestDomain, url, text);
  }

  void SimulateJsResourceOnDomain(const StringPiece& domain,
                                  const StringPiece& url,
                                  const StringPiece& text) {
    SetFetchResponse(StrCat(domain, url), default_js_header_, text);
  }

  void PrepareToCollectScriptsInto(ScriptInfoVector* output) {
    rewrite_driver()->AddOwnedFilter(new ScriptCollector(output));
  }

  // Makes sure that the script looks like a combination.
  void VerifyCombinedOnDomain(const StringPiece& domain, const ScriptInfo& info,
                              const StringPiece& name) {
    EXPECT_FALSE(info.url.empty());
    // The combination url should incorporate both names...
    GoogleUrl combination_url(info.url);
    EXPECT_EQ(domain, combination_url.AllExceptLeaf());
    ResourceNamer namer;
    EXPECT_TRUE(namer.Decode(combination_url.LeafWithQuery()));
    EXPECT_EQ(RewriteDriver::kJavascriptCombinerId, namer.id());
    EXPECT_EQ(name, namer.name());
    EXPECT_EQ("js", namer.ext());
  }

  void VerifyCombined(const ScriptInfo& info, const StringPiece& name) {
    VerifyCombinedOnDomain(kTestDomain, info, name);
  }

  // Make sure the script looks like it was rewritten for a use of given URL
  void VerifyUseOnDomain(const StringPiece& domain, const ScriptInfo& info,
                         const StringPiece& rel_url) {
    GoogleString abs_url = StrCat(domain, rel_url);
    EXPECT_TRUE(info.url.empty());
    EXPECT_EQ(StrCat("eval(", filter_->VarName(abs_url), ");"),
              info.text_content);
  }

  void VerifyUse(const ScriptInfo& info, const StringPiece& rel_url) {
    VerifyUseOnDomain(kTestDomain, info, rel_url);
  }

 protected:
  ResponseHeaders default_js_header_;
  GoogleString other_domain_;
  JsCombineFilter* filter_;  // Owned by rewrite_driver_
};

// Test for basic operation, including escaping and fetch reconstruction.
TEST_P(JsCombineFilterTest, CombineJs) {
  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);
  ParseUrl(kTestDomain, StrCat("<script src=", kJsUrl1, "></script>",
                               "<script src=", kJsUrl2, "></script>"));

  // This should produce 3 script elements, with the first one referring to
  // the combination, and the second and third using eval.
  ASSERT_EQ(3, scripts.size());
  VerifyCombined(scripts[0], StrCat(kJsUrl1, "+", kJsUrl2));
  VerifyUse(scripts[1], kJsUrl1);
  VerifyUse(scripts[2], kJsUrl2);

  // Now check the actual contents. These might change slightly
  // during implementation changes, requiring update of the test;
  // but this is also not dependent on VarName working right.
  EXPECT_EQ(
      AddHtmlBody(StrCat("<script src=\"", kTestDomain,
                         "a.js+b.js.pagespeed.jc.g2Xe9o4bQ2.js\"></script>",
                         "<script>eval(mod_pagespeed_KecOGCIjKt);</script>",
                         "<script>eval(mod_pagespeed_dzsx6RqvJJ);</script>")),
      output_buffer_);

  // Now fetch the combined URL.
  GoogleString combination_src;
  ASSERT_TRUE(ServeResourceUrl(scripts[0].url, &combination_src));
  EXPECT_EQ(StrCat("var mod_pagespeed_KecOGCIjKt = ", kEscapedJs1, ";\n",
                   "var mod_pagespeed_dzsx6RqvJJ = ", kEscapedJs2, ";\n"),
            combination_src);
  ServeResourceFromManyContexts(scripts[0].url, combination_src);
}

// Various things that prevent combining
TEST_P(JsCombineFilterTest, TestBarriers) {
  ValidateNoChanges("noscript",
                    StrCat("<noscript><script src=", kJsUrl1, "></script>",
                           "</noscript><script src=", kJsUrl2, "></script>"));

  // inline scripts or scripts with random stuff inside
  ValidateNoChanges("non-inline",
                    StrCat("<script src=", kJsUrl1, "></script>",
                           "<script>code</script>"));

  ValidateNoChanges("content",
                    StrCat("<script src=", kJsUrl1, "></script>",
                           "<script src=", kJsUrl2, ">code</script>"));

  // Languages
  ValidateNoChanges("tcl",
                    StrCat("<script language=tcl src=", kJsUrl1, "></script>"
                           "<script src=", kJsUrl2, "></script>"));

  ValidateNoChanges("tcl2",
                    StrCat("<script language=tcl src=", kJsUrl1, "></script>"
                           "<script language=tcl src=", kJsUrl2, "></script>"));

  ValidateNoChanges("tcl3",
                    StrCat("<script src=", kJsUrl1, "></script>"
                           "<script language=tcl src=", kJsUrl2, "></script>"));

  // Execution model
  ValidateNoChanges("exec",
                    StrCat("<script src=", kJsUrl1, "></script>"
                           "<script defer src=", kJsUrl2, "></script>"));

  // IE conditional comments
  ValidateNoChanges("iec",
                    StrCat("<script src=", kJsUrl1, "></script>",
                           "<!--[if IE]><![endif]-->",
                           "<script src=", kJsUrl2, "></script>"));

  // Strict mode, with 2 different quote styles
  ValidateNoChanges("strict1",
                    StrCat("<script src=", kJsUrl1, "></script>",
                           "<script src=", kStrictUrl1, "></script>"));

  ValidateNoChanges("strict2",
                    StrCat("<script src=", kJsUrl1, "></script>",
                           "<script src=", kStrictUrl2, "></script>"));

  ValidateNoChanges("strict3",
                    StrCat("<script src=", kStrictUrl1, "></script>",
                           "<script src=", kJsUrl1, "></script>"));

  ValidateNoChanges("strict4",
                    StrCat("<script src=", kStrictUrl2, "></script>",
                           "<script src=", kJsUrl1, "></script>"));
}

// Things between scripts that should not prevent combination
TEST_P(JsCombineFilterTest, TestNonBarriers) {
  GoogleString combined_url = StrCat(kJsUrl1, "+", kJsUrl2);

  // Intervening text
  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);
  ParseUrl(kTestDomain, StrCat("<script src=", kJsUrl1, "></script>",
                               "some text",
                               "<script src=", kJsUrl2, "></script>"));

  // This should produce 3 script elements, with the first one referring to
  // the combination, and the second and third using eval.
  ASSERT_EQ(3, scripts.size());
  VerifyCombined(scripts[0], combined_url);
  VerifyUse(scripts[1], kJsUrl1);
  VerifyUse(scripts[2], kJsUrl2);

  // Same thing with other tags, even nested.
  scripts.clear();
  ParseUrl(kTestDomain, StrCat("<s><script src=", kJsUrl1, "></script></s>",
                               "<div>block</div><!-- comment -->",
                               "<b><script src=", kJsUrl2, "></script></b>"));

  ASSERT_EQ(3, scripts.size());
  VerifyCombined(scripts[0], combined_url);
  VerifyUse(scripts[1], kJsUrl1);
  VerifyUse(scripts[2], kJsUrl2);

  // Whitespace inside scripts is OK.
  scripts.clear();
  ParseUrl(kTestDomain, StrCat("<script src=", kJsUrl1, ">       </script>",
                               "<div>block</div>",
                               "<b><script src=", kJsUrl2, ">\t</script></b>"));

  ASSERT_EQ(3, scripts.size());
  VerifyCombined(scripts[0], combined_url);
  VerifyUse(scripts[1], kJsUrl1);
  VerifyUse(scripts[2], kJsUrl2);
}

// Flush in the middle of first one --- should not change first one;
// should combine the next two
TEST_P(JsCombineFilterTest, TestFlushMiddle1) {
  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);
  SetupWriter();
  html_parse()->StartParse(kTestDomain);
  html_parse()->ParseText(StrCat("<script src=", kJsUrl1, ">"));
  html_parse()->Flush();
  html_parse()->ParseText("</script>");
  html_parse()->ParseText(StrCat("<script src=", kJsUrl2, "></script>"));
  html_parse()->ParseText(StrCat("<script src=", kJsUrl3, "></script>"));
  html_parse()->FinishParse();

  ASSERT_EQ(4, scripts.size());
  EXPECT_EQ(kJsUrl1, scripts[0].url);
  VerifyCombined(scripts[1], StrCat(kJsUrl2, "+", kJsUrl3));
  VerifyUse(scripts[2], kJsUrl2);
  VerifyUse(scripts[3], kJsUrl3);
}

// Flush in the middle of a second tag - should back it out and realize
// that the single entry left should not be touched
TEST_P(JsCombineFilterTest, TestFlushMiddle2) {
  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);
  SetupWriter();
  html_parse()->StartParse(kTestDomain);
  html_parse()->ParseText(StrCat("<script src=", kJsUrl1, "></script>"));
  html_parse()->ParseText(StrCat("<script src=", kJsUrl2, ">"));
  html_parse()->Flush();
  html_parse()->ParseText("</script>");
  html_parse()->ParseText(StrCat("<script src=", kJsUrl3, "></script>"));
  html_parse()->FinishParse();

  // Nothing should have gotten combined
  EXPECT_EQ(StrCat(StrCat("<script src=", kJsUrl1, "></script>"),
                   StrCat("<script src=", kJsUrl2, "></script>"),
                   StrCat("<script src=", kJsUrl3, "></script>")),
            output_buffer_);
}

// Flush in the middle of a third tag -- first two should be combined.
TEST_P(JsCombineFilterTest, TestFlushMiddle3) {
  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);
  SetupWriter();
  html_parse()->StartParse(kTestDomain);
  html_parse()->ParseText(StrCat("<script src=", kJsUrl1, "></script>"));
  html_parse()->ParseText(StrCat("<script src=", kJsUrl2, "></script>"));
  html_parse()->Flush();
  html_parse()->ParseText(StrCat("<script src=", kJsUrl3, "></script>"));
  html_parse()->FinishParse();

  ASSERT_EQ(4, scripts.size());
  VerifyCombined(scripts[0], StrCat(kJsUrl1, "+", kJsUrl2));
  VerifyUse(scripts[1], kJsUrl1);
  VerifyUse(scripts[2], kJsUrl2);
  EXPECT_EQ(kJsUrl3, scripts[3].url);
}

// Make sure we honor <base> properly.
// Note: this test relies on <base> tag implicitly authorizing things,
// which we may wish to change in the future.
TEST_P(JsCombineFilterTest, TestBase) {
  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);
  ParseUrl(kTestDomain, StrCat("<base href=", other_domain_, ">",
                               "<script src=", kJsUrl1, "></script>"
                               "<script src=", kJsUrl2, "></script>"));
  ASSERT_EQ(3, scripts.size());
  VerifyCombinedOnDomain(other_domain_, scripts[0],
                         StrCat(kJsUrl1, "+", kJsUrl2));
  VerifyUseOnDomain(other_domain_, scripts[1], kJsUrl1);
  VerifyUseOnDomain(other_domain_, scripts[2], kJsUrl2);
}

// Make sure we check for cross-domain rejections.
TEST_P(JsCombineFilterTest, TestCrossDomainReject) {
  ValidateNoChanges("xd",
                    StrCat("<script src=", other_domain_, kJsUrl1, "></script>",
                           "<script src=", kJsUrl2, "></script>"));

  ValidateNoChanges(
      "xd.2", StrCat("<script src=", other_domain_, kJsUrl1, "></script>",
                     "<script src=", other_domain_, kJsUrl2, "></script>"));

  ValidateNoChanges(
      "xd.3", StrCat("<script src=", kJsUrl1, "></script>",
                     "<script src=", other_domain_, kJsUrl2, "></script>"));
}

// Validate that we can recover a combination after a cross-domain rejection
TEST_P(JsCombineFilterTest, TestCrossDomainRecover) {
  ASSERT_TRUE(options()->domain_lawyer()->AddDomain(other_domain_,
                                                    &message_handler_));

  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);
  SetupWriter();
  html_parse()->StartParse(kTestDomain);
  // 2 scripts on main domain --- should be combined with each other
  html_parse()->ParseText(StrCat("<script src=", kJsUrl1, "></script>"));
  html_parse()->ParseText(StrCat("<script src=", kJsUrl2, "></script>"));
  // 2 scripts on other domain --- should be combined with each other
  html_parse()->ParseText(
      StrCat("<script src=", other_domain_, kJsUrl1, "></script>"));
  html_parse()->ParseText(
      StrCat("<script src=", other_domain_, kJsUrl2, "></script>"));
  html_parse()->FinishParse();

  ASSERT_EQ(6, scripts.size());
  VerifyCombined(scripts[0], StrCat(kJsUrl1, "+", kJsUrl2));
  VerifyUse(scripts[1], kJsUrl1);
  VerifyUse(scripts[2], kJsUrl2);

  VerifyCombinedOnDomain(other_domain_, scripts[3],
                         StrCat(kJsUrl1, "+", kJsUrl2));
  VerifyUseOnDomain(other_domain_, scripts[4], kJsUrl1);
  VerifyUseOnDomain(other_domain_, scripts[5], kJsUrl2);
}

TEST_P(JsCombineFilterTest, TestCombineStats) {
  Variable* num_reduced =
      statistics()->GetVariable(JsCombineFilter::kJsFileCountReduction);
  EXPECT_EQ(0, num_reduced->Get());

  // Now combine 3 files into one.
  ParseUrl(kTestDomain,
           StrCat(StrCat("<script src=", kJsUrl1, "></script>"),
                  StrCat("<script src=", kJsUrl2, "></script>"),
                  StrCat("<script src=", kJsUrl3, "></script>")));

  EXPECT_EQ(2, num_reduced->Get());
}

TEST_P(JsCombineFilterTest, TestCombineShard) {
  // Make sure we produce consistent output when sharding/serving off a
  // different host.
  GoogleString path = StrCat(kJsUrl1, "+", kJsUrl2, ".pagespeed.jc.0.js");

  GoogleString src1;
  EXPECT_TRUE(ServeResourceUrl(StrCat(kTestDomain, path), &src1));

  const char kOtherDomain[] = "http://cdn.example.com/";
  SimulateJsResourceOnDomain(kOtherDomain, kJsUrl1, kJsText1);
  SimulateJsResourceOnDomain(kOtherDomain, kJsUrl2, kJsText2);

  GoogleString src2;
  EXPECT_TRUE(ServeResourceUrl(StrCat(kOtherDomain, path), &src2));

  EXPECT_EQ(src1, src2);
}

INSTANTIATE_TEST_CASE_P(JsCombineFilterTestInstance, JsCombineFilterTest,
                        ::testing::Bool());

}  // namespace net_instaweb
