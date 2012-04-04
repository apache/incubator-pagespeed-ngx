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
#include "net/instaweb/rewriter/public/javascript_filter.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
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
const char kMinifiedJs1[] = "var a=\"hello\\nsecond line\"";
const char kJsText2[] = "// script2\r\nvar b=42;\n";
const char kMinifiedJs2[] = "var b=42;";
const char kJsText3[] = "var x = 42;\nvar y = 31459;\n";
const char kStrictText1[] = "'use strict'; var x = 32;";
const char kStrictText2[] = "\"use strict\"; var x = 42;";
const char kEscapedJs1[] =
    "\"// script1\\nvar a=\\\"hello\\\\nsecond line\\\"\"";
const char kEscapedJs2[] = "\"// script2\\r\\nvar b=42;\\n\"";
const char kMinifiedEscapedJs1[] = "\"var a=\\\"hello\\\\nsecond line\\\"\"";
const char kMinifiedEscapedJs2[] = "\"var b=42;\"";
const char kAlternateDomain[] = "http://alternate.com/";

}  // namespace

// Test fixture for JsCombineFilter unit tests.
class JsCombineFilterTest : public ResourceManagerTestBase {
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
    SetUpWithJsFilter(false);
  }

  void SetUpWithJsFilter(bool use_js_filter) {
    ResourceManagerTestBase::SetUp();
    UseMd5Hasher();
    SetDefaultLongCacheHeaders(&kContentTypeJavascript, &default_js_header_);
    SimulateJsResource(kJsUrl1, kJsText1);
    SimulateJsResource(kJsUrl2, kJsText2);
    SimulateJsResourceOnDomain(kAlternateDomain, kJsUrl2, kJsText2);
    SimulateJsResource(kJsUrl3, kJsText3);
    SimulateJsResource(kStrictUrl1, kStrictText1);
    SimulateJsResource(kStrictUrl2, kStrictText2);

    if (use_js_filter) {
      AddRewriteFilter(new JavascriptFilter(rewrite_driver()));
    }
    filter_ = new JsCombineFilter(rewrite_driver());
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
    rewrite_driver()->AddOwnedPostRenderFilter(new ScriptCollector(output));
  }

  // Makes sure that the script looks like a combination.
  void VerifyCombinedOnDomain(const StringPiece& base_url,
                              const StringPiece& domain,
                              const ScriptInfo& info,
                              const StringVector& name_vector) {
    EXPECT_FALSE(info.url.empty());
    // We need to check against the encoded form of the given domain.
    GoogleUrl encoded(EncodeWithBase(base_url, domain, "x", "0", "x", "x"));
    // The combination url should incorporate both names...
    GoogleUrl combination_url(info.url);
    EXPECT_STREQ(encoded.AllExceptLeaf(), combination_url.AllExceptLeaf());
    ResourceNamer namer;
    EXPECT_TRUE(namer.Decode(combination_url.LeafWithQuery()));
    EXPECT_STREQ(RewriteOptions::kJavascriptCombinerId, namer.id());
    GoogleString encoding;
    for (int i = 0, n = name_vector.size(); i < n; ++i) {
      StrAppend(&encoding, (i == 0) ? "" : "+", name_vector[i]);
    }
    EXPECT_STREQ(encoding, namer.name());
    EXPECT_STREQ("js", namer.ext());
  }

  void VerifyCombined(const ScriptInfo& info, const StringVector& name) {
    VerifyCombinedOnDomain(kTestDomain, kTestDomain, info, name);
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

  // Test basic combining of multiple JS files.  The resultant names and
  // hashes may differ depending on whether we are working with rewritten
  // or sharded domains, and whether we minify the js files before combining
  // them.  Thus we must pass in the hashes for the various components.
  //
  // Note that we must use the MD5 hasher for this test because the
  // combiner generates local javascript variable names using the
  // content-hasher.
  void TestCombineJs(const StringVector& combined_name,
                     const StringPiece& combined_hash,
                     const StringPiece& hash1,
                     const StringPiece& hash2,
                     bool minified,
                     const StringPiece& domain) {
    ScriptInfoVector scripts;
    PrepareToCollectScriptsInto(&scripts);
    ParseUrl(kTestDomain, StrCat("<script src=", kJsUrl1, "></script>",
                                 "<script src=", kJsUrl2, "></script>"));

    // This should produce 3 script elements, with the first one referring to
    // the combination, and the second and third using eval.
    ASSERT_EQ(3, scripts.size());
    VerifyCombinedOnDomain(domain, domain, scripts[0], combined_name);
    if (!minified) {
      VerifyUse(scripts[1], kJsUrl1);
      VerifyUse(scripts[2], kJsUrl2);
    }

    // Now check the actual contents. These might change slightly
    // during implementation changes, requiring update of the test;
    // but this is also not dependent on VarName working right.
    GoogleString combined_path =
        Encode("", "jc", combined_hash, combined_name, "js");
    GoogleUrl encoded_domain(Encode(domain, "x", "0", "x", "x"));
    // We can be be given URLs with ',M' in them, which are URL escaped to have
    // two commas, which is not what we want, so reverse that. We can be given
    // such URLs because it's too hard to do the encoding programatically.
    GlobalReplaceSubstring(",,M", ",M", &combined_path);
    EXPECT_STREQ(AddHtmlBody(
        StrCat("<script src=\"", encoded_domain.AllExceptLeaf(),
               combined_path, "\"></script>"
               "<script>eval(mod_pagespeed_", hash1, ");</script>"
               "<script>eval(mod_pagespeed_", hash2, ");</script>")),
        output_buffer_);

    // Now fetch the combined URL.
    GoogleString combination_src;
    ASSERT_TRUE(FetchResourceUrl(scripts[0].url, &combination_src));
    EXPECT_STREQ(StrCat(
        StrCat("var mod_pagespeed_", hash1, " = ",
               (minified ? kMinifiedEscapedJs1 : kEscapedJs1), ";\n"),
        StrCat("var mod_pagespeed_", hash2, " = ",
               (minified ? kMinifiedEscapedJs2 : kEscapedJs2), ";\n")),
                 combination_src);

    ServeResourceFromManyContexts(scripts[0].url, combination_src);
  }

 protected:
  ResponseHeaders default_js_header_;
  GoogleString other_domain_;
  JsCombineFilter* filter_;  // Owned by rewrite_driver_
};

class JsFilterAndCombineFilterTest : public JsCombineFilterTest {
  virtual void SetUp() {
    SetUpWithJsFilter(true);
  }
};

// Test for basic operation, including escaping and fetch reconstruction.
TEST_F(JsCombineFilterTest, CombineJs) {
  TestCombineJs(MultiUrl("a.js", "b.js"), "g2Xe9o4bQ2", "KecOGCIjKt",
                "dzsx6RqvJJ", false, kTestDomain);
}

TEST_F(JsFilterAndCombineFilterTest, MinifyCombineJs) {
  // These hashes depend on the URL, which is different when using the
  // test url namer, so handle the difference.
  bool test_url_namer = factory()->use_test_url_namer();
  TestCombineJs(MultiUrl("a.js,Mjm.FUEwDOA7jh.js", "b.js,Mjm.Y1kknPfzVs.js"),
                test_url_namer ? "8erozavBF5" : "FA3Pqioukh",
                test_url_namer ? "JO0ZTfFSfI" : "S$0tgbTH0O",
                test_url_namer ? "8QmSuIkgv_" : "ose8Vzgyj9",
                true, kTestDomain);
}

// Issue 308: ModPagespeedShardDomain disables combine_js.  Actually
// the code (in url_partnership.cc) was already doing the right thing,
// but was not previously confirmed in a unit-test.
TEST_F(JsFilterAndCombineFilterTest, MinifyShardCombineJs) {
  DomainLawyer* lawyer = options()->domain_lawyer();
  ASSERT_TRUE(lawyer->AddShard(kTestDomain, "a.com,b.com", &message_handler_));

  // Make sure the shards have the resources, too.
  SimulateJsResourceOnDomain("http://a.com/", kJsUrl1, kJsText1);
  SimulateJsResourceOnDomain("http://a.com/", kJsUrl2, kJsText2);

  SimulateJsResourceOnDomain("http://b.com/", kJsUrl1, kJsText1);
  SimulateJsResourceOnDomain("http://b.com/", kJsUrl2, kJsText2);

  TestCombineJs(MultiUrl("a.js,Mjm.FUEwDOA7jh.js", "b.js,Mjm.Y1kknPfzVs.js"),
                "FA3Pqioukh", "S$0tgbTH0O", "ose8Vzgyj9", true,
                "http://b.com/");
}

TEST_F(JsFilterAndCombineFilterTest, MinifyCombineAcrossHosts) {
  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);
  GoogleString js_url_2(StrCat(kAlternateDomain, kJsUrl2));
  options()->domain_lawyer()->AddDomain(kAlternateDomain, message_handler());
  ParseUrl(kTestDomain, StrCat("<script src=", kJsUrl1, "></script>",
                               "<script src=", js_url_2, "></script>"));
  ASSERT_EQ(2, scripts.size());
  ServeResourceFromManyContexts(scripts[0].url, kMinifiedJs1);
  ServeResourceFromManyContexts(scripts[1].url, kMinifiedJs2);
}

class JsFilterAndCombineProxyTest : public JsFilterAndCombineFilterTest {
 public:
  JsFilterAndCombineProxyTest() {
    SetUseTestUrlNamer(true);
  }
};

TEST_F(JsFilterAndCombineProxyTest, MinifyCombineSameHostProxy) {
  // TODO(jmarantz): This more intrusive test-helper fails.  I'd like
  // to look at it with Matt in the context of the new TestUrlNamer
  // infrastructure.  However that should not block the point of this
  // test which is that the combination should be made if the
  // hosts do match, unlike MinifyCombineAcrossHostsProxy below.
  //
  // Specifically, VerifyCombinedOnDomain appears not to know about
  // TestUrlNamer.
  //
  // bool test_url_namer = factory()->use_test_url_namer();
  // DCHECK(test_url_namer);
  // TestCombineJs("a.js,Mjm.FUEwDOA7jh.js+b.js,Mjm.Y1kknPfzVs.js",
  //               test_url_namer ? "8erozavBF5" : "FA3Pqioukh",
  //               test_url_namer ? "JO0ZTfFSfI" : "S$0tgbTH0O",
  //               test_url_namer ? "8QmSuIkgv_" : "ose8Vzgyj9",
  //               true, kTestDomain);

  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);
  ParseUrl(kTestDomain, StrCat("<script src=", kJsUrl1, "></script>",
                               "<script src=", kJsUrl2, "></script>"));
  ASSERT_EQ(3, scripts.size()) << "successful combination yields 3 scripts";
}

TEST_F(JsFilterAndCombineProxyTest, MinifyCombineAcrossHostsProxy) {
  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);
  GoogleString js_url_2(StrCat(kAlternateDomain, kJsUrl2));
  options()->domain_lawyer()->AddDomain(kAlternateDomain, message_handler());
  ParseUrl(kTestDomain, StrCat("<script src=", kJsUrl1, "></script>",
                               "<script src=", js_url_2, "></script>"));
  ASSERT_EQ(2, scripts.size()) << "If combination fails, we get 2 scripts";
  ServeResourceFromManyContexts(scripts[0].url, kMinifiedJs1);
  EXPECT_EQ(Encode(kTestDomain, "jm", "FUEwDOA7jh", kJsUrl1, "js"),
            scripts[0].url);
  ServeResourceFromManyContexts(scripts[1].url, kMinifiedJs2);
  EXPECT_EQ(Encode(kAlternateDomain, "jm", "Y1kknPfzVs", kJsUrl2, "js"),
            scripts[1].url);
}

TEST_F(JsFilterAndCombineFilterTest, MinifyPartlyCached) {
  // Testcase for case where we have cached metadata for results of JS rewrite,
  // but not its contents easily available.
  SimulateJsResource(kJsUrl1, kJsText1);
  SimulateJsResource(kJsUrl2, kJsText2);

  // Fetch the result of the JS filter (which runs first) filter applied,
  // to pre-cache them.
  GoogleString out_url1(Encode(kTestDomain, "jm", "FUEwDOA7jh", kJsUrl1, "js"));
  GoogleString content;
  EXPECT_TRUE(FetchResourceUrl(out_url1, &content));
  EXPECT_STREQ(kMinifiedJs1, content);

  GoogleString out_url2(Encode(kTestDomain, "jm", "Y1kknPfzVs", kJsUrl2, "js"));
  EXPECT_TRUE(FetchResourceUrl(out_url2, &content));
  EXPECT_STREQ(kMinifiedJs2, content);

  // Make sure the data isn't available in the HTTP cache (while the metadata
  // still is).
  lru_cache()->Delete(out_url1);
  lru_cache()->Delete(out_url2);

  // Now try to get a combination.
  bool test_url_namer = factory()->use_test_url_namer();
  TestCombineJs(MultiUrl("a.js,Mjm.FUEwDOA7jh.js", "b.js,Mjm.Y1kknPfzVs.js"),
                test_url_namer ? "8erozavBF5" : "FA3Pqioukh",
                test_url_namer ? "JO0ZTfFSfI" : "S$0tgbTH0O",
                test_url_namer ? "8QmSuIkgv_" : "ose8Vzgyj9",
                true /*minified*/, kTestDomain);
}

// Various things that prevent combining
TEST_F(JsCombineFilterTest, TestBarriers) {
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

// Make sure that rolling back a <script> that has both a source and inline data
// out of the combination works even when we have more than one filter involved.
// This used to crash under async flow.
TEST_F(JsFilterAndCombineFilterTest, TestScriptInlineTextRollback) {
  ValidateExpected("rollback1",
                   StrCat("<script src=", kJsUrl1, "></script>",
                          "<script src=", kJsUrl2, ">TEXT HERE</script>"),
                   StrCat("<script src=",
                          Encode(kTestDomain, "jm", "FUEwDOA7jh", "a.js", "js"),
                          ">",
                          "</script>",
                          "<script src=",
                          Encode(kTestDomain, "jm", "Y1kknPfzVs", "b.js", "js"),
                          ">",
                          "TEXT HERE</script>"));
}

// Things between scripts that should not prevent combination
TEST_F(JsCombineFilterTest, TestNonBarriers) {
  StringVector combined_url = MultiUrl(kJsUrl1, kJsUrl2);

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
TEST_F(JsCombineFilterTest, TestFlushMiddle1) {
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
  VerifyCombined(scripts[1], MultiUrl(kJsUrl2, kJsUrl3));
  VerifyUse(scripts[2], kJsUrl2);
  VerifyUse(scripts[3], kJsUrl3);
}

// Flush in the middle of a second tag - should back it out and realize
// that the single entry left should not be touched
TEST_F(JsCombineFilterTest, TestFlushMiddle2) {
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
TEST_F(JsCombineFilterTest, TestFlushMiddle3) {
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
  VerifyCombined(scripts[0], MultiUrl(kJsUrl1, kJsUrl2));
  VerifyUse(scripts[1], kJsUrl1);
  VerifyUse(scripts[2], kJsUrl2);
  EXPECT_EQ(kJsUrl3, scripts[3].url);
}

// Make sure we honor <base> properly.
// Note: this test relies on <base> tag implicitly authorizing things,
// which we may wish to change in the future.
TEST_F(JsCombineFilterTest, TestBase) {
  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);
  ParseUrl(kTestDomain, StrCat("<base href=", other_domain_, ">",
                               "<script src=", kJsUrl1, "></script>"
                               "<script src=", kJsUrl2, "></script>"));
  ASSERT_EQ(3, scripts.size());
  VerifyCombinedOnDomain(other_domain_, other_domain_, scripts[0],
                         MultiUrl(kJsUrl1, kJsUrl2));
  VerifyUseOnDomain(other_domain_, scripts[1], kJsUrl1);
  VerifyUseOnDomain(other_domain_, scripts[2], kJsUrl2);
}

// Make sure we check for cross-domain rejections.
TEST_F(JsCombineFilterTest, TestCrossDomainReject) {
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
TEST_F(JsCombineFilterTest, TestCrossDomainRecover) {
  DomainLawyer* lawyer = options()->domain_lawyer();
  ASSERT_TRUE(lawyer->AddDomain(other_domain_, &message_handler_));

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
  VerifyCombined(scripts[0], MultiUrl(kJsUrl1, kJsUrl2));
  VerifyUse(scripts[1], kJsUrl1);
  VerifyUse(scripts[2], kJsUrl2);

  VerifyCombinedOnDomain(kTestDomain, other_domain_, scripts[3],
                         MultiUrl(kJsUrl1, kJsUrl2));
  VerifyUseOnDomain(other_domain_, scripts[4], kJsUrl1);
  VerifyUseOnDomain(other_domain_, scripts[5], kJsUrl2);
}

TEST_F(JsCombineFilterTest, TestCombineStats) {
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

TEST_F(JsCombineFilterTest, TestCombineShard) {
  // Make sure we produce consistent output when sharding/serving off a
  // different host.
  GoogleString path =
      Encode("", "jc", "0", MultiUrl(kJsUrl1, kJsUrl2), "js");

  GoogleString src1;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, path), &src1));

  const char kOtherDomain[] = "http://cdn.example.com/";
  SimulateJsResourceOnDomain(kOtherDomain, kJsUrl1, kJsText1);
  SimulateJsResourceOnDomain(kOtherDomain, kJsUrl2, kJsText2);

  GoogleString src2;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kOtherDomain, path), &src2));

  EXPECT_EQ(src1, src2);
}

TEST_F(JsCombineFilterTest, PartlyInvalidFetchCache) {
  // Regression test where a combination involving a 404 gets fetched,
  // and then rewritten --- incorrectly.
  // Note: arguably this shouldn't get cached at all; but it certainly
  // should not result in an inappropriate result.
  SetFetchResponse404("404.js");
  SetResponseWithDefaultHeaders("a.js", kContentTypeJavascript, "var a;", 100);
  SetResponseWithDefaultHeaders("b.js", kContentTypeJavascript, "var b;", 100);
  EXPECT_FALSE(
      TryFetchResource(
          Encode(kTestDomain, "jc", "0", MultiUrl("a.js", "b.js", "404.js"),
                 "js")));
  ValidateNoChanges("partly_invalid",
                    StrCat("<script src=a.js></script>",
                           "<script src=b.js></script>"
                           "<script src=404.js></script>"));
}

}  // namespace net_instaweb
