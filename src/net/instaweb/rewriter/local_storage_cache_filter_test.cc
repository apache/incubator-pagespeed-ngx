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

// Author: matterbury@google.com (Matt Atterbury)

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/local_storage_cache_filter.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

// Filenames of resource files.
const char kJunkCssFilename[] = "http://example.com/junk.css";  // NOT test.com
const char kStylesCssFilename[] = "styles.css";
const char kCuppaPngFilename[] = "Cuppa.png";
const char kPuzzleJpgFilename[] = "Puzzle.jpg";

// Contents of resource files.
const char kJunkCssContents[] =
    "@import url(junk://junk.com);";
const char kStylesCssContents[] =
    ".background_cyan{background-color:#0ff}"
    ".foreground_pink{color:#ffc0cb}";
const char kCuppaPngInlineData[] =
    "data:image/png;base64,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";

class LocalStorageCacheTest : public ResourceManagerTestBase,
                              public ::testing::WithParamInterface<bool> {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    MySetUp();
  }

  void MySetUp() {
    options()->EnableFilter(RewriteOptions::kInlineCss);
    options()->EnableFilter(RewriteOptions::kInlineImages);
    options()->EnableFilter(RewriteOptions::kLocalStorageCache);
    options()->set_image_inline_max_bytes(2000);
    rewrite_driver()->AddFilters();

    SetResponseWithDefaultHeaders(kJunkCssFilename, kContentTypeCss,
                                  kJunkCssContents, 100);
    SetResponseWithDefaultHeaders(kStylesCssFilename, kContentTypeCss,
                                  kStylesCssContents, 100);
    AddFileToMockFetcher(StrCat(kTestDomain, kCuppaPngFilename),
                         kCuppaPngFilename, kContentTypePng, 100);
    AddFileToMockFetcher(StrCat(kTestDomain, kPuzzleJpgFilename),
                         kPuzzleJpgFilename, kContentTypeJpeg, 100);
    StaticJavascriptManager* static_js_manager =
        resource_manager()->static_javascript_manager();
    local_storage_cache_js_ =
        StrCat("<script pagespeed_no_defer>",
               static_js_manager->GetJsSnippet(
                   StaticJavascriptManager::kLocalStorageCacheJs, options()),
               LocalStorageCacheFilter::kLscInitializer,
               "</script>");
  }

  void TestLocalStorage(const StringPiece& case_id,
                        const GoogleString& head_html_in,
                        const GoogleString& head_html_out,
                        const GoogleString& body_html_in,
                        const GoogleString& body_html_out) {
    StaticJavascriptManager* static_js_manager =
        resource_manager()->static_javascript_manager();
    StringPiece local_storage_cache_js =
        static_js_manager->GetJsSnippet(
            StaticJavascriptManager::kLocalStorageCacheJs, options());
    const char kInWrapperFormat[] =
        "<head>\n"
        "  <title>Local Storage Cache Test</title>\n"
        "%s"
        "</head>\n"
        "<body>\n"
        "%s"
        "</body>\n";
    const GoogleString out_wrapper_format = StrCat(
        "<head>\n"
        "  <title>Local Storage Cache Test</title>\n"
        "%s"
        "</head>\n"
        "<body>",
        kNoScriptRedirectFormatter, "\n"
        "%s"
        "</body>\n");

    GoogleString url = StrCat(
        "http://test.com/", case_id, ".html?ModPagespeed=off");

    GoogleString html_in(StringPrintf(
        kInWrapperFormat, head_html_in.c_str(), body_html_in.c_str()));
    GoogleString html_out(StringPrintf(
        out_wrapper_format.c_str(), head_html_out.c_str(), url.c_str(),
        url.c_str(), body_html_out.c_str()));

    // Set this for every test.
    rewrite_driver()->set_request_headers(&request_headers_);

    Parse(case_id, html_in);
    GoogleString expected_out = doctype_string_ + AddHtmlBody(html_out);

    EXPECT_EQ(expected_out, output_buffer_) << "Test id:" << case_id;
    output_buffer_.clear();
  }

  GoogleString InsertScriptBefore(const StringPiece& snippet) const {
    return StrCat(local_storage_cache_js_, snippet);
  }

  RequestHeaders request_headers_;
  GoogleString local_storage_cache_js_;
};

TEST_F(LocalStorageCacheTest, Simple) {
  TestLocalStorage("simple", "", "", "<div/>", "<div/>");
}

TEST_F(LocalStorageCacheTest, Link) {
  TestLocalStorage("link",
                   "<link rel='stylesheet' href='styles.css'>",
                   InsertScriptBefore(
                       "<style "
                       "pagespeed_lsc_url=\"http://test.com/styles.css\" "
                       "pagespeed_lsc_hash=\"0\" "
                       "pagespeed_lsc_expiry=\"Tue, 02 Feb 2010 18:53:06 GMT\">"
                       ".background_cyan{background-color:#0ff}"
                       ".foreground_pink{color:#ffc0cb}"
                       "</style>"),
                   "<div/>", "<div/>");
}

TEST_F(LocalStorageCacheTest, LinkRewriteContextNotExecuted) {
  // The domains are different so the RewriteContext is never kicked off,
  // which should result in no local storage cache changes at all.
  TestLocalStorage("link_rewrite_context_not_executed",
                   "<link rel='stylesheet' href='http://example.com/junk.css'>",
                   "<link rel='stylesheet' href='http://example.com/junk.css'>",
                   "<div/>", "<div/>");
}

TEST_F(LocalStorageCacheTest, LinkUrlTransormationFails) {
  // The CSS rewriting fails so the local storage cache attributes are omitted
  // but because the CSS rewriting is asynchronous we still insert the JS even
  // though it ends up not being used. C'est la vie!
  options()->domain_lawyer()->AddDomain("example.com", message_handler());
  TestLocalStorage("link_url_transormation_fails",
                   "<link rel='stylesheet' href='http://example.com/junk.css'>",
                   InsertScriptBefore(
                       "<link rel='stylesheet' "
                       "href='http://example.com/junk.css'>"),
                   "<div/>", "<div/>");
}

class LocalStorageCacheTinyTest : public LocalStorageCacheTest {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    options()->set_css_inline_max_bytes(10);  // An arbitrary tiny value.
    MySetUp();
  }
};

TEST_F(LocalStorageCacheTinyTest, LinkDontInline) {
  // The CSS inlining fails because we've turned the limit down low. We expect
  // no LSC attributes in the result but because the CSS rewriting is
  // asynchronous we still insert the JS even though it ends up not being used.
  TestLocalStorage("link_dont_inline",
                   "<link rel='stylesheet' href='styles.css'>",
                   InsertScriptBefore(
                       "<link rel='stylesheet' href='styles.css'>"),
                   "<div/>", "<div/>");
}

TEST_F(LocalStorageCacheTest, Img) {
  TestLocalStorage("img", "", "",
                   StrCat("<img src='", kCuppaPngFilename, "'>"),
                   InsertScriptBefore(
                       StrCat("<img src='", kCuppaPngInlineData,
                              "' pagespeed_lsc_url="
                              "\"", kTestDomain, kCuppaPngFilename, "\""
                              " pagespeed_lsc_hash=\"0\""
                              " pagespeed_lsc_expiry="
                              "\"Tue, 02 Feb 2010 18:53:06 GMT\""
                              ">")));
}

TEST_F(LocalStorageCacheTest, ImgTooBig) {
  TestLocalStorage("img_too_big", "", "",
                   StrCat("<img src='", kPuzzleJpgFilename, "'>"),
                   InsertScriptBefore(
                       StrCat("<img src='", kPuzzleJpgFilename, "'>")));
}

TEST_F(LocalStorageCacheTest, ImgLocalStorageDisabled) {
  options()->ClearSignatureForTesting();
  options()->DisableFilter(RewriteOptions::kLocalStorageCache);
  options()->set_ajax_rewriting_enabled(true);
  resource_manager()->ComputeSignature(options());

  TestLocalStorage("img_local_storage_disabled", "", "",
                   StrCat("<img src='", kPuzzleJpgFilename, "'>"),
                   StrCat("<img src='", kPuzzleJpgFilename, "'>"));
}

TEST_F(LocalStorageCacheTest, CookieSet) {
  // The 2 hash values are Fe1SLPZ14c and du_OhARrJl. Only suppress the first.
  UseMd5Hasher();
  GoogleString cookie = StrCat(LocalStorageCacheFilter::kLscCookieName,
                               "=Fe1SLPZ14c");
  request_headers_.Add(HttpAttributes::kCookie, cookie);
  TestLocalStorage("cookie_set",
                   StrCat("<link rel='stylesheet' href='",
                          kStylesCssFilename,
                          "'>"),
                   InsertScriptBefore(
                       StrCat("<script pagespeed_no_defer>pagespeed.inlineCss("
                              "\"", kTestDomain, kStylesCssFilename, "\""
                              ");</script>")),
                   StrCat("<img src='", kCuppaPngFilename, "'>"),
                   StrCat("<img src='", kCuppaPngInlineData,
                          "' pagespeed_lsc_url="
                          "\"", kTestDomain, kCuppaPngFilename, "\""
                          " pagespeed_lsc_hash=\"du_OhARrJl\""
                          " pagespeed_lsc_expiry="
                          "\"Tue, 02 Feb 2010 18:53:06 GMT\""
                          ">"));
}

TEST_F(LocalStorageCacheTest, RepeatViews) {
  // The 2 hash values are Fe1SLPZ14c (CSS) and du_OhARrJl (image).
  UseMd5Hasher();

  GoogleString css = StrCat("<link rel='stylesheet' href='",
                            kStylesCssFilename,
                            "'>");
  GoogleString img = StrCat("<img src='", kCuppaPngFilename, "'>");

  // First view shouldn't rewrite anything though lsc_url attributes are added.
  // Don't rewrite because in the real world the fetch and processing of the
  // resource could take longer than the rewriting timeout, and we want to
  // simulate that here. We redo it below with the rewriting completing in time.
  GoogleString external_css = StrCat("<link rel='stylesheet' href="
                                     "'", kStylesCssFilename, "'"
                                     " pagespeed_lsc_url="
                                     "\"", kTestDomain, kStylesCssFilename, "\""
                                     ">");
  GoogleString external_img = StrCat("<img src="
                                     "'", kCuppaPngFilename, "'"
                                     " pagespeed_lsc_url="
                                     "\"", kTestDomain, kCuppaPngFilename, "\""
                                     ">");
  SetupWaitFetcher();
  TestLocalStorage("first_view",
                   css, InsertScriptBefore(external_css),
                   img, external_img);
  CallFetcherCallbacks();

  // Second view will inline them both and add an expiry to both.
  GoogleString inlined_css = StrCat("<style pagespeed_lsc_url="
                                    "\"", kTestDomain, kStylesCssFilename, "\""
                                    " pagespeed_lsc_hash=\"Fe1SLPZ14c\""
                                    " pagespeed_lsc_expiry="
                                    "\"Tue, 02 Feb 2010 18:53:06 GMT\""
                                    ">",
                                    kStylesCssContents,
                                    "</style>");
  GoogleString inlined_img = StrCat("<img src='", kCuppaPngInlineData,
                                    "' pagespeed_lsc_url="
                                    "\"", kTestDomain, kCuppaPngFilename, "\""
                                    " pagespeed_lsc_hash=\"du_OhARrJl\""
                                    " pagespeed_lsc_expiry="
                                    "\"Tue, 02 Feb 2010 18:53:06 GMT\""
                                    ">");
  TestLocalStorage("second_view",
                   css, InsertScriptBefore(inlined_css),
                   img, inlined_img);

  // The JavaScript would set these cookies for the next request.
  GoogleString cookie = StrCat(LocalStorageCacheFilter::kLscCookieName,
                               "=Fe1SLPZ14c,du_OhARrJl");
  request_headers_.Add(HttpAttributes::kCookie, cookie);

  // Third view will not send the inlined data and will send scripts in place
  // of the link and img elements.
  GoogleString scripted_css = StrCat("<script pagespeed_no_defer>"
                                     "pagespeed.inlineCss("
                                     "\"", kTestDomain, kStylesCssFilename, "\""
                                     ");</script>");
  GoogleString scripted_img = StrCat("<script pagespeed_no_defer>"
                                     "pagespeed.inlineImg("
                                     "\"", kTestDomain, kCuppaPngFilename, "\""
                                     ");</script>");
  TestLocalStorage("third_view",
                   css, InsertScriptBefore(scripted_css),
                   img, scripted_img);
}

TEST_F(LocalStorageCacheTest, RepeatViewsWithOtherAttributes) {
  // The 2 hash values are Fe1SLPZ14c (CSS) and du_OhARrJl (image).
  UseMd5Hasher();

  GoogleString css = StrCat("<link rel='stylesheet' href='",
                            kStylesCssFilename,
                            "'>");
  GoogleString img = StrCat("<img src='", kCuppaPngFilename, "'"
                            " alt='A cup of joe'"
                            " alt=\"A cup of joe\""
                            " alt='A cup of joe&#39;s \"joe\"'"
                            " alt=\"A cup of joe's &quot;joe&quot;\">");

  // First view shouldn't rewrite anything though lsc_url attributes are added.
  // Don't rewrite because in the real world the fetch and processing of the
  // resource could take longer than the rewriting timeout, and we want to
  // simulate that here. We redo it below with the rewriting completing in time.
  GoogleString external_css = StrCat("<link rel='stylesheet' href="
                                     "'", kStylesCssFilename, "'"
                                     " pagespeed_lsc_url="
                                     "\"", kTestDomain, kStylesCssFilename, "\""
                                     ">");
  GoogleString external_img = StrCat("<img src="
                                     "'", kCuppaPngFilename, "'"
                                     " alt='A cup of joe'"
                                     " alt=\"A cup of joe\""
                                     " alt='A cup of joe&#39;s \"joe\"'"
                                     " alt=\"A cup of joe's &quot;joe&quot;\""
                                     " pagespeed_lsc_url="
                                     "\"", kTestDomain, kCuppaPngFilename, "\""
                                     ">");
  SetupWaitFetcher();
  TestLocalStorage("first_view",
                   css, InsertScriptBefore(external_css),
                   img, external_img);
  CallFetcherCallbacks();

  // Second view will inline them both and add an expiry to both.
  GoogleString inlined_css = StrCat("<style"
                                    " pagespeed_lsc_url="
                                    "\"", kTestDomain, kStylesCssFilename, "\""
                                    " pagespeed_lsc_hash=\"Fe1SLPZ14c\""
                                    " pagespeed_lsc_expiry="
                                    "\"Tue, 02 Feb 2010 18:53:06 GMT\""
                                    ">",
                                    kStylesCssContents,
                                    "</style>");
  GoogleString inlined_img = StrCat("<img src='", kCuppaPngInlineData, "'"
                                    " alt='A cup of joe'"
                                    " alt=\"A cup of joe\""
                                    " alt='A cup of joe&#39;s \"joe\"'"
                                    " alt=\"A cup of joe's &quot;joe&quot;\""
                                    " pagespeed_lsc_url="
                                    "\"", kTestDomain, kCuppaPngFilename, "\""
                                    " pagespeed_lsc_hash=\"du_OhARrJl\""
                                    " pagespeed_lsc_expiry="
                                    "\"Tue, 02 Feb 2010 18:53:06 GMT\""
                                    ">");
  TestLocalStorage("second_view",
                   css, InsertScriptBefore(inlined_css),
                   img, inlined_img);

  // The JavaScript would set these cookies for the next request.
  GoogleString cookie = StrCat(LocalStorageCacheFilter::kLscCookieName,
                               "=Fe1SLPZ14c,du_OhARrJl");
  request_headers_.Add(HttpAttributes::kCookie, cookie);

  // Third view will not send the inlined data and will send scripts in place
  // of the link and img elements.
  GoogleString scripted_css = StrCat("<script pagespeed_no_defer>"
                                     "pagespeed.inlineCss("
                                     "\"", kTestDomain, kStylesCssFilename, "\""
                                     ");</script>");
  GoogleString scripted_img = StrCat("<script pagespeed_no_defer>"
                                     "pagespeed.inlineImg("
                                     "\"", kTestDomain, kCuppaPngFilename, "\""
                                     ", \"alt=A cup of joe\""
                                     ", \"alt=A cup of joe\""
                                     ", \"alt=A cup of joe's \\\"joe\\\"\""
                                     ", \"alt=A cup of joe's \\\"joe\\\"\""
                                     ");</script>");
  TestLocalStorage("third_view",
                   css, InsertScriptBefore(scripted_css),
                   img, scripted_img);
}

}  // namespace

}  // namespace net_instaweb
