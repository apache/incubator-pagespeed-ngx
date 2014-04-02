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

#include <memory>
#include <vector>

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/public/cache_extender.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/charset_util.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/worker_test_base.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"

namespace net_instaweb {

namespace {

const char kJsUrl1[] = "a.js";
const char kJsUrl2[] = "b.js";
const char kJsUrl3[] = "c.js";
const char kJsUrl4[] = "d.js";
const char kStrictUrl1[] = "strict1.js";
const char kStrictUrl2[] = "strict2.js";
const char kIntrospectiveUrl1[] = "introspective1.js";
const char kIntrospectiveUrl2[] = "introspective2.js";
const char kJsText1[] = "// script1\nvar a=\"hello\\nsecond line\"";
const char kMinifiedJs1[] = "var a=\"hello\\nsecond line\"";
const char kJsText2[] = "// script2\r\nvar b=42;\n";
const char kMinifiedJs2[] = "var b=42;";
const char kJsText3[] = "var x = 42;\nvar y = 31459;\n";
const char kJsText4[] = "var m = 'abcd';\n";
const char kStrictText1[] = "'use strict'; var x = 32;";
const char kStrictText2[] = "\"use strict\"; var x = 42;";
const char kIntrospectiveText1[] = "var x = 7; $('script') ; var y = 42;";
const char kIntrospectiveText2[] = "document.getElementsByTagName('script');";
const char kEscapedJs1[] =
    "\"// script1\\nvar a=\\\"hello\\\\nsecond line\\\"\"";
const char kEscapedJs2[] = "\"// script2\\r\\nvar b=42;\\n\"";
const char kMinifiedEscapedJs1[] = "\"var a=\\\"hello\\\\nsecond line\\\"\"";
const char kMinifiedEscapedJs2[] = "\"var b=42;\"";
const char kAlternateDomain[] = "http://alternate.com/";

}  // namespace

// Test fixture for JsCombineFilter unit tests.
class JsCombineFilterTest : public RewriteTestBase {
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
    RewriteTestBase::SetUp();
    UseMd5Hasher();
    SetDefaultLongCacheHeaders(&kContentTypeJavascript, &default_js_header_);
    SimulateJsResource(kJsUrl1, kJsText1);
    SimulateJsResource(kJsUrl2, kJsText2);
    SimulateJsResourceOnDomain(kAlternateDomain, kJsUrl2, kJsText2);
    SimulateJsResource(kJsUrl3, kJsText3);
    SimulateJsResource(kJsUrl4, kJsText4);
    SimulateJsResource(kStrictUrl1, kStrictText1);
    SimulateJsResource(kStrictUrl2, kStrictText2);
    SimulateJsResource(kIntrospectiveUrl1, kIntrospectiveText1);
    SimulateJsResource(kIntrospectiveUrl2, kIntrospectiveText2);

    options()->SoftEnableFilterForTesting(RewriteOptions::kCombineJavascript);
    SetUpExtraFilters();
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

  virtual void SetUpExtraFilters() {
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
    GoogleUrl base_gurl(base_url);
    GoogleUrl combination_url(base_gurl, info.url);
    ASSERT_TRUE(encoded.IsAnyValid()) << encoded.UncheckedSpec();
    ASSERT_TRUE(combination_url.IsAnyValid()) << info.url;
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
    EXPECT_EQ(
        StrCat("eval(",
               JsCombineFilter::VarName(server_context(), abs_url),
               ");"),
        info.text_content);
  }

  void VerifyUse(const ScriptInfo& info, const StringPiece& rel_url) {
    VerifyUseOnDomain(kTestDomain, info, rel_url);
  }

  GoogleString TestHtml() {
    return StrCat("<script src=", kJsUrl1, "></script>",
                  "<script src=", kJsUrl2, "></script>");
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
    GoogleUrl html_url(kTestDomain);
    ParseUrl(html_url.Spec(), TestHtml());

    // This should produce 3 script elements, with the first one referring to
    // the combination, and the second and third using eval.
    ASSERT_EQ(3, scripts.size());
    VerifyCombinedOnDomain(domain, domain, scripts[0], combined_name);
    VerifyUse(scripts[1], kJsUrl1);
    VerifyUse(scripts[2], kJsUrl2);

    // Now check the actual contents. These might change slightly
    // during implementation changes, requiring update of the test;
    // but this is also not dependent on VarName working right.
    EXPECT_STREQ(AddHtmlBody(
        StrCat("<script src=\"", scripts[0].url, "\"></script>"
               "<script>eval(mod_pagespeed_", hash1, ");</script>"
               "<script>eval(mod_pagespeed_", hash2, ");</script>")),
        output_buffer_);

    // Check that the combined URL is what we'd expect.
    GoogleString combined_path =
        Encode("", "jc", combined_hash, combined_name, "js");
    GoogleUrl encoded_domain(Encode(domain, "x", "0", "x", "x"));
    // We can be be given URLs with ',M' in them, which are URL escaped to have
    // two commas, which is not what we want, so reverse that. We can be given
    // such URLs because it's too hard to do the encoding programatically.
    GlobalReplaceSubstring(",,M", ",M", &combined_path);
    GoogleUrl output_url(html_url, scripts[0].url);
    EXPECT_EQ(StrCat(encoded_domain.AllExceptLeaf(), combined_path),
              output_url.Spec());

    // Now fetch the combined URL.
    GoogleString combination_src;
    ASSERT_TRUE(FetchResourceUrl(output_url.Spec(), &combination_src));
    EXPECT_STREQ(StrCat(
        StrCat("var mod_pagespeed_", hash1, " = ",
               (minified ? kMinifiedEscapedJs1 : kEscapedJs1), ";\n"),
        StrCat("var mod_pagespeed_", hash2, " = ",
               (minified ? kMinifiedEscapedJs2 : kEscapedJs2), ";\n")),
                 combination_src);

    ServeResourceFromManyContexts(output_url.Spec().as_string(),
                                  combination_src);
  }

 protected:
  ResponseHeaders default_js_header_;
  GoogleString other_domain_;
};

class JsFilterAndCombineFilterTest : public JsCombineFilterTest {
  virtual void SetUpExtraFilters() {
    options()->SoftEnableFilterForTesting(RewriteOptions::kRewriteJavascript);
  }
};

// Test for basic operation, including escaping and fetch reconstruction.
TEST_F(JsCombineFilterTest, CombineJs) {
  TestCombineJs(MultiUrl(kJsUrl1, kJsUrl2), "g2Xe9o4bQ2", "KecOGCIjKt",
                "dzsx6RqvJJ", false, kTestDomain);
}

class JsCombineFilterCustomOptions : public JsCombineFilterTest {
 protected:
  // Derived classes should set their options and then call
  // JsCombineFilterTest::SetUp().
  virtual void SetUp() {}
};

TEST_F(JsCombineFilterCustomOptions, CombineJsPreserveURLsOn) {
  options()->set_js_preserve_urls(true);
  JsCombineFilterTest::SetUp();
  ValidateNoChanges("combine_js_preserve_urls_on",
                    StrCat("<script src=", kJsUrl1, "></script>",
                           "<script src=", kJsUrl2, "></script>"));
}

// When cache is unhealthy, don't rewrite URLs in HTML.
TEST_F(JsCombineFilterTest, CombineJsUnhealthy) {
  lru_cache()->set_is_healthy(false);
  ValidateNoChanges("unhealthy", TestHtml());
}

// But do serve correctly rewritten resources when
// .pagespeed. resources are requested even if cache is unhealthy.
TEST_F(JsCombineFilterTest, ServeFilesUnhealthy) {
  lru_cache()->set_is_healthy(false);
  SetResponseWithDefaultHeaders(kJsUrl1, kContentTypeJavascript, "var a;", 100);
  SetResponseWithDefaultHeaders(kJsUrl2, kContentTypeJavascript, "var b;", 100);
  GoogleString content;
  const GoogleString combined_url = Encode(
      kTestDomain, "jc", "0", MultiUrl(kJsUrl1, kJsUrl2), "js");
  ASSERT_TRUE(FetchResourceUrl(combined_url, &content));
  const char kCombinedContent[] =
      "var mod_pagespeed_KecOGCIjKt = \"var a;\";\n"
      "var mod_pagespeed_dzsx6RqvJJ = \"var b;\";\n";
  EXPECT_EQ(kCombinedContent, content);
}

class JsCombineAndCacheExtendFilterTest : public JsCombineFilterTest {
  virtual void SetUpExtraFilters() {
    options()->SoftEnableFilterForTesting(RewriteOptions::kExtendCacheScripts);
  }
};

TEST_F(JsCombineAndCacheExtendFilterTest, CombineJsNoExtraCacheExtension) {
  // Make sure we don't end up trying to cache extend things
  // the combiner removed. We need to custom-set resources here to give them
  // shorter TTL than the fixture would.
  SetResponseWithDefaultHeaders(kJsUrl1, kContentTypeJavascript, kJsText1, 100);
  SetResponseWithDefaultHeaders(kJsUrl2, kContentTypeJavascript, kJsText2, 100);

  TestCombineJs(MultiUrl(kJsUrl1, kJsUrl2), "g2Xe9o4bQ2", "KecOGCIjKt",
                "dzsx6RqvJJ", false, kTestDomain);
  EXPECT_EQ(0,
            rewrite_driver()->statistics()->GetVariable(
                CacheExtender::kCacheExtensions)->Get());
}

// Turning on AvoidRewritingIntrospectiveJavascript should not affect normal
// rewriting.
TEST_F(JsCombineFilterTest, CombineJsAvoidRewritingIntrospectiveJavascripOn) {
  options()->ClearSignatureForTesting();
  options()->set_avoid_renaming_introspective_javascript(true);
  server_context()->ComputeSignature(options());
  TestCombineJs(MultiUrl(kJsUrl1, kJsUrl2), "g2Xe9o4bQ2", "KecOGCIjKt",
                "dzsx6RqvJJ", false, kTestDomain);
}

TEST_F(JsFilterAndCombineFilterTest, ReconstructNoTimeout) {
  // Nested fetch should not timeout on reconstruction. Note that we still
  // need this to work even though we no longer create nesting; for migration
  // reasons.
  GoogleString rel_url =
      Encode("", "jc", "FA3Pqioukh",
             MultiUrl("a.js.pagespeed.jm.FUEwDOA7jh.js",
                      "b.js.pagespeed.jm.Y1kknPfzVs.js"), "js");
  GoogleString url = StrCat(kTestDomain, rel_url);
  const char kLegacyVar1[] = "mod_pagespeed_S$0tgbTH0O";
  const char kLegacyVar2[] = "mod_pagespeed_ose8Vzgyj9";

  // First rewrite the page, to see what the evals look like.
  // These should actually just look like a.js + b.js these days.
  GoogleString simple_rel_url =
      Encode("", "jc", "HrCUtQsDp_", MultiUrl("a.js", "b.js"), "js");
  const char kVar1[] = "mod_pagespeed_KecOGCIjKt";
  const char kVar2[] = "mod_pagespeed_dzsx6RqvJJ";
  ValidateExpected("no_timeout",
                   StrCat("<script src=", kJsUrl1, "></script>",
                          "<script src=", kJsUrl2, "></script>"),
                   StrCat("<script src=\"", simple_rel_url, "\"></script>",
                          "<script>eval(", kVar1, ");</script>",
                          "<script>eval(", kVar2, ");</script>"));

  // Clear cache..
  lru_cache()->Clear();

  server_context()->global_options()->ClearSignatureForTesting();
  server_context()->global_options()
      ->set_test_instant_fetch_rewrite_deadline(true);
  server_context()->ComputeSignature(server_context()->global_options());

  StringAsyncFetch async_fetch(rewrite_driver_->request_context());

  // Note that here we specifically use a pool'ed rewrite driver, since
  // the bug we were testing for only occurred with them.
  RewriteDriver* driver =
      server_context()->NewRewriteDriver(CreateRequestContext());

  WorkerTestBase::SyncPoint unblock_rewrite(server_context()->thread_system());

  // Wedge the actual rewrite queue to force the timeout to trigger.
  driver->low_priority_rewrite_worker()->Add(
      new WorkerTestBase::WaitRunFunction(&unblock_rewrite));

  driver->FetchResource(url, &async_fetch);
  unblock_rewrite.Notify();
  AdvanceTimeMs(50);

  driver->WaitForShutDown();
  driver->Cleanup();

  // Make sure we have the right hashes. Note that we fetched an old style
  // URL, that had both .js and .jm in it, so the variable names are the old
  // ones, not new ones.
  EXPECT_NE(GoogleString::npos,
            async_fetch.buffer().find(kLegacyVar1));
  EXPECT_NE(GoogleString::npos,
            async_fetch.buffer().find(kLegacyVar2));
}

TEST_F(JsFilterAndCombineFilterTest, MinifyCombineJs) {
  TestCombineJs(MultiUrl("a.js", "b.js"),
                "HrCUtQsDp_",  // combined hash
                "KecOGCIjKt",  // var name for a.js (same as in CombineJs)
                "dzsx6RqvJJ",  // var name for b.js (same as in CombineJs)
                true, kTestDomain);
}

// Even with inline_unauthorized_resources set to true, we should not combine
// unauthorized and authorized resources. Also, we should not allow fetching
// of component minified unauthorized resources even if they were created.
TEST_F(JsFilterAndCombineFilterTest, TestCrossDomainRejectUnauthEnabled) {
  options()->ClearSignatureForTesting();
  options()->AddInlineUnauthorizedResourceType(semantic_type::kScript);
  server_context()->ComputeSignature(options());
  ValidateExpected("xd",
                   StrCat("<script src=", other_domain_, kJsUrl1, "></script>",
                          "<script src=", kJsUrl2, "></script>"),
                   StrCat("<script src=", other_domain_, kJsUrl1, "></script>",
                          "<script src=",
                          Encode("", "jm", "Y1kknPfzVs", kJsUrl2, "js"),
                          ">",
                          "</script>"));
  GoogleString contents;
  ASSERT_FALSE(FetchResourceUrl(StrCat(other_domain_, kJsUrl1), &contents));
}

// Issue 308: ModPagespeedShardDomain disables combine_js.  Actually
// the code (in url_partnership.cc) was already doing the right thing,
// but was not previously confirmed in a unit-test.
TEST_F(JsFilterAndCombineFilterTest, MinifyShardCombineJs) {
  ASSERT_TRUE(AddShard(kTestDomain, "a.com,b.com"));

  // Make sure the shards have the resources, too.
  SimulateJsResourceOnDomain("http://a.com/", kJsUrl1, kJsText1);
  SimulateJsResourceOnDomain("http://a.com/", kJsUrl2, kJsText2);

  SimulateJsResourceOnDomain("http://b.com/", kJsUrl1, kJsText1);
  SimulateJsResourceOnDomain("http://b.com/", kJsUrl2, kJsText2);

  TestCombineJs(MultiUrl("a.js", "b.js"),
                "HrCUtQsDp_", "KecOGCIjKt", "dzsx6RqvJJ", true,
                "http://b.com/");
}

TEST_F(JsFilterAndCombineFilterTest, MinifyCombineAcrossHosts) {
  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);
  GoogleString js_url_2(StrCat(kAlternateDomain, kJsUrl2));
  AddDomain(kAlternateDomain);
  ParseUrl(kTestDomain, StrCat("<script src=", kJsUrl1, "></script>",
                               "<script src=", js_url_2, "></script>"));
  ASSERT_EQ(2, scripts.size());
  GoogleUrl base_url(kTestDomain);
  GoogleUrl url0(base_url, scripts[0].url);
  ServeResourceFromManyContexts(url0.spec_c_str(), kMinifiedJs1);
  GoogleUrl url1(base_url, scripts[1].url);
  ServeResourceFromManyContexts(url1.spec_c_str(), kMinifiedJs2);
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
  AddDomain(kAlternateDomain);
  ParseUrl(kTestDomain, StrCat("<script src=", kJsUrl1, "></script>",
                               "<script src=", js_url_2, "></script>"));
  ASSERT_EQ(2, scripts.size()) << "If combination fails, we get 2 scripts";

  // Note: This absolutifies path because it uses TestUrlNamer which moves
  // it to a different domain.
  EXPECT_EQ(Encode(kTestDomain, "jm", "FUEwDOA7jh", kJsUrl1, "js"),
            scripts[0].url);
  ServeResourceFromManyContexts(scripts[0].url, kMinifiedJs1);

  EXPECT_EQ(Encode(kAlternateDomain, "jm", "Y1kknPfzVs", kJsUrl2, "js"),
            scripts[1].url);
  ServeResourceFromManyContexts(scripts[1].url, kMinifiedJs2);
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

  // UnsafeToRename, with plain and jquery syntax
  options()->ClearSignatureForTesting();
  options()->set_avoid_renaming_introspective_javascript(true);
  server_context()->ComputeSignature(options());
  ValidateNoChanges("introspective1",
                    StrCat("<script src=", kJsUrl1, "></script>",
                           "<script src=", kIntrospectiveUrl1, "></script>"));

  ValidateNoChanges("introspective2",
                    StrCat("<script src=", kJsUrl1, "></script>",
                           "<script src=", kIntrospectiveUrl2, "></script>"));
}

// Make sure that rolling back a <script> that has both a source and inline data
// out of the combination works even when we have more than one filter involved.
// This used to crash under async flow.
TEST_F(JsFilterAndCombineFilterTest, TestScriptInlineTextRollback) {
  ValidateExpected("rollback1",
                   StrCat("<script src=", kJsUrl1, "></script>",
                          "<script src=", kJsUrl2, ">TEXT HERE</script>"),
                   StrCat("<script src=",
                          Encode("", "jm", "FUEwDOA7jh", kJsUrl1, "js"),
                          ">",
                          "</script>",
                          "<script src=",
                          Encode("", "jm", "Y1kknPfzVs", kJsUrl2, "js"),
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

// Flush in the middle of first one will not prevent us from combining it.
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
  VerifyCombined(scripts[0], MultiUrl(kJsUrl1, kJsUrl2, kJsUrl3));
  VerifyUse(scripts[1], kJsUrl1);
  VerifyUse(scripts[2], kJsUrl2);
  VerifyUse(scripts[3], kJsUrl3);
}

// Flush in the middle of a second tag - the flush will just spit out the
// first script tag, and we'll hold back the second one till after we
// see the "</script>", which will then be combined with the third.
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

  ASSERT_EQ(4, scripts.size());
  EXPECT_STREQ(kJsUrl1, scripts[0].url);
  VerifyCombined(scripts[1], MultiUrl(kJsUrl2, kJsUrl3));
  VerifyUse(scripts[2], kJsUrl2);
  VerifyUse(scripts[3], kJsUrl3);
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

// Make sure we check for cross-domain rejections even when
// inline_unauthorized_resources is set to true.
TEST_F(JsCombineFilterTest, TestCrossDomainRejectUnauthEnabled) {
  options()->ClearSignatureForTesting();
  options()->AddInlineUnauthorizedResourceType(semantic_type::kScript);
  server_context()->ComputeSignature(options());
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
  ASSERT_TRUE(AddDomain(other_domain_));

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

  EXPECT_STREQ("jc", AppliedRewriterStringFromLog());
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
  SetResponseWithDefaultHeaders(kJsUrl1, kContentTypeJavascript, "var a;", 100);
  SetResponseWithDefaultHeaders(kJsUrl2, kContentTypeJavascript, "var b;", 100);
  EXPECT_FALSE(
      TryFetchResource(
          Encode(kTestDomain, "jc", "0", MultiUrl(kJsUrl1, kJsUrl2, "404.js"),
                 "js")));
  ValidateNoChanges("partly_invalid",
                    StrCat("<script src=a.js></script>",
                           "<script src=b.js></script>"
                           "<script src=404.js></script>"));
}

TEST_F(JsCombineFilterTest, CharsetDetermination) {
  GoogleString x_js_url = "x.js";
  GoogleString y_js_url = "y.js";
  GoogleString z_js_url = "z.js";
  const char x_js_body[] = "var x;";
  const char y_js_body[] = "var y;";
  const char z_js_body[] = "var z;";
  GoogleString bom_body = StrCat(kUtf8Bom, y_js_body);

  // x.js has no charset header nor a BOM.
  // y.js has no charset header but has a BOM.
  // z.js has a charset header but no BOM.
  ResponseHeaders default_header;
  SetDefaultLongCacheHeaders(&kContentTypeJavascript, &default_header);
  SetFetchResponse(StrCat(kTestDomain, x_js_url), default_header, x_js_body);
  SetFetchResponse(StrCat(kTestDomain, y_js_url), default_header, bom_body);
  default_header.MergeContentType("text/javascript; charset=iso-8859-1");
  SetFetchResponse(StrCat(kTestDomain, z_js_url), default_header, z_js_body);

  ResourcePtr x_js_resource(CreateResource(kTestDomain, x_js_url));
  ResourcePtr y_js_resource(CreateResource(kTestDomain, y_js_url));
  ResourcePtr z_js_resource(CreateResource(kTestDomain, z_js_url));
  EXPECT_TRUE(ReadIfCached(x_js_resource));
  EXPECT_TRUE(ReadIfCached(y_js_resource));
  EXPECT_TRUE(ReadIfCached(z_js_resource));

  StringPiece result;
  const StringPiece kUsAsciiCharset("us-ascii");

  // Nothing set: charset should be empty.
  result = RewriteFilter::GetCharsetForScript(x_js_resource.get(), "", "");
  EXPECT_TRUE(result.empty());

  // Only the containing charset is set.
  result = RewriteFilter::GetCharsetForScript(x_js_resource.get(),
                                              "", kUsAsciiCharset);
  EXPECT_STREQ(result, kUsAsciiCharset);

  // The containing charset is trumped by the resource's BOM.
  result = RewriteFilter::GetCharsetForScript(y_js_resource.get(),
                                              "", kUsAsciiCharset);
  EXPECT_STREQ("utf-8", result);

  // The resource's BOM is trumped by the element's charset attribute.
  result = RewriteFilter::GetCharsetForScript(y_js_resource.get(),
                                              "gb", kUsAsciiCharset);
  EXPECT_STREQ("gb", result);

  // The element's charset attribute is trumped by the resource's header.
  result = RewriteFilter::GetCharsetForScript(z_js_resource.get(),
                                              "gb", kUsAsciiCharset);
  EXPECT_STREQ("iso-8859-1", result);
}

TEST_F(JsCombineFilterTest, AllDifferentCharsets) {
  GoogleString html_url = StrCat(kTestDomain, "bom.html");
  GoogleString a_js_url = kJsUrl1;
  GoogleString b_js_url = kJsUrl2;
  GoogleString c_js_url = kJsUrl3;
  GoogleString d_js_url = kJsUrl4;
  const char a_js_body[] = "var a;";
  const char b_js_body[] = "var b;";
  const char c_js_body[] = "var c;";
  const char d_js_body[] = "var d;";
  GoogleString bom_body = StrCat(kUtf8Bom, c_js_body);

  // a.js has no charset header nor BOM nor an attribute: use the page.
  // b.js has no charset header nor a BOM but has an attribute: use the attr.
  // c.js has no charset header nor attribute but has a BOM: use the BOM.
  // d.js has a charset header but no BOM nor attribute: use the charset.
  ResponseHeaders default_header;
  SetDefaultLongCacheHeaders(&kContentTypeJavascript, &default_header);
  SetFetchResponse(StrCat(kTestDomain, a_js_url), default_header, a_js_body);
  SetFetchResponse(StrCat(kTestDomain, b_js_url), default_header, b_js_body);
  SetFetchResponse(StrCat(kTestDomain, c_js_url), default_header, bom_body);
  default_header.MergeContentType("text/javascript; charset=iso-8859-1");
  SetFetchResponse(StrCat(kTestDomain, d_js_url), default_header, d_js_body);

  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);
  GoogleString input_buffer =
      "<head>\n"
      "  <meta charset=\"gb\">\n"
      "  <script src=a.js></script>"
      "  <script src=b.js charset=us-ascii></script>"
      "  <script src=c.js></script>"
      "  <script src=d.js></script>"
      "</head>\n";
  ParseUrl(html_url, input_buffer);

  // This should leave the same 4 original scripts.
  EXPECT_EQ(4, scripts.size());
  EXPECT_EQ(kJsUrl1, scripts[0].url);
  EXPECT_EQ(kJsUrl2, scripts[1].url);
  EXPECT_EQ(kJsUrl3, scripts[2].url);
  EXPECT_EQ(kJsUrl4, scripts[3].url);
}

TEST_F(JsCombineFilterTest, BomMismatch) {
  GoogleString html_url = StrCat(kTestDomain, "bom.html");
  GoogleString x_js_url = "x.js";
  GoogleString y_js_url = "y.js";

  // BOM documentation: http://www.unicode.org/faq/utf_bom.html
  const char x_js_body[] = "var x;";
  const char y_js_body[] = "var y;";
  GoogleString bom_body = StrCat(kUtf8Bom, y_js_body);

  ResponseHeaders default_header;
  SetDefaultLongCacheHeaders(&kContentTypeJavascript, &default_header);
  SetFetchResponse(StrCat(kTestDomain, x_js_url), default_header, x_js_body);
  SetFetchResponse(StrCat(kTestDomain, y_js_url), default_header, bom_body);

  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);

  // x.js will have an indeterminate charset: it's not in the resource headers,
  // nor the element's attribute, there's no BOM, and the HTML doesn't set it.
  GoogleString input_buffer(StrCat(
      "<head>\n"
      "  <script src=x.js></script>\n",
      "  <script src=y.js></script>\n",
      "</head>\n"));
  ParseUrl(html_url, input_buffer);

  ASSERT_EQ(2, scripts.size());

  GoogleString input_buffer_reversed =
      "<head>\n"
      "  <script src=y.js></script>\n"
      "  <script src=x.js></script>\n"
      "</head>\n";
  scripts.clear();
  ParseUrl(html_url, input_buffer_reversed);
  ASSERT_EQ(2, scripts.size());
}

TEST_F(JsCombineFilterTest, EmbeddedBom) {
  // Test that we can combine 2 JS, one with a BOM and one without, and that
  // the BOM is retained in the combination.
  GoogleUrl html_url(StrCat(kTestDomain, "bom.html"));
  GoogleString x_js_url = "x.js";
  GoogleString y_js_url = "y.js";

  // BOM documentation: http://www.unicode.org/faq/utf_bom.html
  const char x_js_body[] = "var x;";
  const char y_js_body[] = "var y;";
  GoogleString bom_body = StrCat(kUtf8Bom, y_js_body);

  ResponseHeaders default_header;
  SetDefaultLongCacheHeaders(&kContentTypeJavascript, &default_header);
  SetFetchResponse(StrCat(kTestDomain, x_js_url), default_header, x_js_body);
  SetFetchResponse(StrCat(kTestDomain, y_js_url), default_header, bom_body);

  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);

  // x.js now has a charset of utf-8 thanks to the meta tag.
  GoogleString input_buffer =
      "<head>\n"
      "  <meta charset=\"UTF-8\">\n"
      "  <script src=x.js></script>\n"
      "  <script src=y.js></script>\n"
      "</head>\n";
  ParseUrl(html_url.Spec(), input_buffer);

  ASSERT_EQ(3, scripts.size());
  VerifyCombined(scripts[0], MultiUrl(x_js_url, y_js_url));
  VerifyUse(scripts[1], x_js_url);
  VerifyUse(scripts[2], y_js_url);

  GoogleString actual_combination;
  GoogleUrl output_url(html_url, scripts[0].url);
  EXPECT_TRUE(FetchResourceUrl(output_url.Spec(), &actual_combination));
  int bom_pos = actual_combination.find(kUtf8Bom);
  EXPECT_EQ(73, bom_pos);  // WARNING: MAGIC VALUE!

  GoogleString input_buffer_reversed =
      "<head>\n"
      "  <meta charset=\"UTF-8\">\n"
      "  <script src=y.js></script>\n"
      "  <script src=x.js></script>\n"
      "</head>\n";
  scripts.clear();
  ParseUrl(html_url.Spec(), input_buffer_reversed);
  actual_combination.clear();
  ASSERT_EQ(3UL, scripts.size());
  VerifyCombined(scripts[0], MultiUrl(y_js_url, x_js_url));
  VerifyUse(scripts[1], y_js_url);
  VerifyUse(scripts[2], x_js_url);
  output_url.Reset(html_url, scripts[0].url);
  EXPECT_TRUE(FetchResourceUrl(output_url.Spec(), &actual_combination));
  bom_pos = actual_combination.find(kUtf8Bom);
  EXPECT_EQ(32, bom_pos);  // WARNING: MAGIC VALUE!
}

TEST_F(JsCombineFilterTest, EmbeddedBomReconstruct) {
  // Make sure we that BOMs are retained when reconstructing.
  const char kJsX[] = "x.js";
  const char kJsY[] = "y.js";
  const GoogleString kJsText = StrCat(kUtf8Bom, "var z;");
  SetResponseWithDefaultHeaders(kJsX, kContentTypeJavascript, kJsText, 300);
  SetResponseWithDefaultHeaders(kJsY, kContentTypeJavascript, kJsText, 300);
  GoogleString js_url =
      Encode(kTestDomain, "jc", "0", MultiUrl(kJsX, kJsY), "js");
  GoogleString js_min =
      StrCat("var mod_pagespeed_CpWSqUZO1U = \"", kJsText, "\";\n"
             "var mod_pagespeed_YdaXhTyTOx = \"", kJsText, "\";\n");
  GoogleString js_out;
  EXPECT_TRUE(FetchResourceUrl(js_url, &js_out));
  EXPECT_EQ(js_min, js_out);
}

TEST_F(JsCombineFilterTest, TestMaxCombinedJsSize) {
  // Make sure we don't produce combined js resource bigger than the
  // max_combined_js_bytes().

  options()->ClearSignatureForTesting();
  options()->set_max_combined_js_bytes(
      STATIC_STRLEN(kJsText1) + STATIC_STRLEN(kJsText2));
  server_context()->ComputeSignature(options());

  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);
  SetupWriter();
  html_parse()->StartParse(kTestDomain);
  html_parse()->ParseText(StrCat("<script src=", kJsUrl1, "></script>"));
  html_parse()->ParseText(StrCat("<script src=", kJsUrl2, "></script>"));
  html_parse()->ParseText(StrCat("<script src=", kJsUrl3, "></script>"));
  html_parse()->ParseText(StrCat("<script src=", kJsUrl4, "></script>"));
  html_parse()->FinishParse();

  ASSERT_EQ(6, scripts.size());
  VerifyCombined(scripts[0], MultiUrl(kJsUrl1, kJsUrl2));
  VerifyUse(scripts[1], kJsUrl1);
  VerifyUse(scripts[2], kJsUrl2);
  VerifyCombined(scripts[3], MultiUrl(kJsUrl3, kJsUrl4));
  VerifyUse(scripts[4], kJsUrl3);
  VerifyUse(scripts[5], kJsUrl4);
}

TEST_F(JsCombineFilterTest, NoCombineNoDeferAttribute) {
  ValidateNoChanges(
      "pagespeed_no_defer",
      StrCat("<script src=", kJsUrl1, " pagespeed_no_defer></script>",
             "<script src=", kJsUrl2, "></script>"));
}

TEST_F(JsCombineFilterTest, PreserveUrlRelativity) {
  options()->ClearSignatureForTesting();
  options()->set_preserve_url_relativity(true);
  server_context()->ComputeSignature(options());

  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);
  Parse("preserve_url_relativity",
        StrCat("<script src=", kJsUrl1, "></script>"
               "<script src=", kJsUrl2, "></script>"));

  ASSERT_EQ(3, scripts.size());  // Combine URL script + 2 eval scripts.
  StringPiece combine_url(scripts[0].url);
  EXPECT_TRUE(combine_url.starts_with("a.js+b.js.pagespeed.jc")) << combine_url;
}

TEST_F(JsCombineFilterTest, NoPreserveUrlRelativity) {
  options()->ClearSignatureForTesting();
  options()->set_preserve_url_relativity(false);
  server_context()->ComputeSignature(options());

  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);
  Parse("preserve_url_relativity",
        StrCat("<script src=", kJsUrl1, "></script>"
               "<script src=", kJsUrl2, "></script>"));

  ASSERT_EQ(3, scripts.size());  // Combine URL script + 2 eval scripts.
  StringPiece combine_url(scripts[0].url);
  EXPECT_TRUE(combine_url.starts_with("http://test.com/a.js+b.js.pagespeed.jc"))
      << combine_url;
}

TEST_F(JsCombineFilterTest, LoadShedPartition) {
  // We want both primary and secondary contexts to use the same cache, as we'll
  // need to use secondary to look at results of primary.
  SetupSharedCache();

  // Arrange for partition to get canceled, by outright shutting down the
  // thread where it's supposed to run.
  server_context()->low_priority_rewrite_workers()->ShutDown();

  // That obviously results in no rewrites.
  ValidateNoChanges(
      "pagespeed_load_shed",
      StrCat("<script src=", kJsUrl1, "></script>",
             "<script src=", kJsUrl2, "></script>"));

  // Flip over to the alternate server, since we broke the primary one's
  // threads.
  SetActiveServer(kSecondary);

  // Need to re-enable stuff since the fixture only turned it on on primary.
  AddFilter(RewriteOptions::kCombineJavascript);

  ScriptInfoVector scripts;
  PrepareToCollectScriptsInto(&scripts);
  Parse("pagespeed_try_again",
        StrCat("<script src=", kJsUrl1, "></script>"
               "<script src=", kJsUrl2, "></script>"));

  ASSERT_EQ(3, scripts.size());  // Combine URL script + 2 eval scripts.
  StringPiece combine_url(scripts[0].url);
  EXPECT_TRUE(combine_url.starts_with("a.js+b.js.pagespeed.jc"))
      << combine_url;
}

}  // namespace net_instaweb
