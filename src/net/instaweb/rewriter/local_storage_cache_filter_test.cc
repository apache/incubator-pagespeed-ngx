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
#include "net/instaweb/rewriter/public/local_storage_cache_filter.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/public/gtest.h"
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
const char kCuppaPng30sqInlineData[] =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB4AAAAeCAAAAAAeW/F+AAABLU"
    "lEQVQoz2P4jxcwUEv6z4H//99u+/7/1U2s0pv0//+tMP74vzUbm/Qvm6D/TyRj/v3XzsIm/U"
    "iy+P8Zxtz/f1nzsUl/VdH/91xe681/G9U32OyOYbz0N4tx+f/FzIuxSc9m7Pm/nSHx3wOGYm"
    "zScxnn/N/M0PR/O+sqbNIJUh/+psjf/x/o8R2L9De19f9faK//f0fmHjaX/5vy4//z5t//7y"
    "3ECNTXxyDg6CEQAcTHPyBLr2FAB72rTv2ES+/DkFZ3NCz4DZO+hCF94fd1jRsw6W8saLIyH/"
    "+/MbsEd7kmmrTfr38bTN7DpQvRpCf+WyYzG+HvSxyo0k56egv/IaT/1DAiSXKlrjv1HSXUPq"
    "Uxw2XZp/zGCNR/m+WhsvpbsCbFi7Mg4BUtsgEZ0gD3t6kusa+ehQAAAABJRU5ErkJggg==";
const char kCuppaPng150sqInlineData[] =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEEAAABGCAAAAAC2maYhAAACyU"
    "lEQVRYw+3XXWjVdRzH8ffZXE5wK4T0SnFCzJYXSlghofiADwQmEyy6G01KSBEKDNMlgXMYyI"
    "aRj3SRESlOnIweoDJR1FLQkEQsrTnFqQg+rrkdz7uLc86F+j/n/H4EQbjvze/m/3vx//8ePl"
    "/++G+LQeFxED7vyI4dH3Wq2tWSjhOOMVvVsxW8r2oTh6OEzIvUq7oAvlDNjGN9lLAP3lM9nY"
    "KTqj/BO1HCy/CZ6mKoSqu+BR/GCL8DZ9RMNcxU9XlojxH2QLXqH0CDaroSDsUIrVB+V+0Cpq"
    "r2lcMnMUIn8L3qaBh6VbUOpsQI3cBi1TeAZtVV5DYldC9GwIh+9RtgzH21qwzejhEWAd9mDx"
    "J0qs6FqtsRQgfQqLoSeFX1K+DLCOEiMFr1F+DJ/NKsiLkXwyD1t3oL4JJ6G1gWczfHAldVn4"
    "Bh93IvszPmHSqhRvU8MEN1G5RfjhD+ApbnF3BD7q4tjdnNjTD0ouo8qLunnoBRN2KEabBS9c"
    "8UqYOqDbAj5lRfKaO2T/XT3EnsqmB+VNKegt2q1lN7J/tVT/dECTeZlFF1PD+rup6tkWn/Qi"
    "5O5r2SHXc92x8pDORDvzs79nb/Vz1r+wcBtfpYEWE6IbWuiDA/SGje39qy5Vqy0EBwVTf3Jg"
    "nvBgPlMD1JWBs0e8mPZ65nGuFogrApSDiiOlDDwgRhdwhQdlfV1UxMEH4LEWqzz37M5AShvy"
    "JAeD37bFu+Cz54qp8LEFryGfRmkvBa6EJ6EA4kCetKA0+lVS/XUZNJEs6WFupVLzwDm5Nv9+"
    "SSwibtaxsJSwrkQ2tJ4bv2pjHAnIECQs+QoGM5YUe6YEY1lZxdNbFhf7GUG3ip6PSpe3tK5u"
    "S5qiJXoi0oafdWFhQ2BGb10bGFgiE47W81Jn7CmvsR/aJ9+CPArF/jOs7XqYeA5dE96/gPD1"
    "bv4P/moPC/Ef4BwgJ0BoZbWwQAAAAASUVORK5CYII=";

class LocalStorageCacheTest : public RewriteTestBase,
                              public ::testing::WithParamInterface<bool> {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
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
    StaticAssetManager* static_asset_manager =
        server_context()->static_asset_manager();
    local_storage_cache_js_ =
        StrCat("<script type=\"text/javascript\" pagespeed_no_defer>"
               "//<![CDATA[\n",
               static_asset_manager->GetAsset(
                   StaticAssetManager::kLocalStorageCacheJs, options()),
               LocalStorageCacheFilter::kLscInitializer,
               "\n//]]></script>");
  }

  void TestLocalStorage(const StringPiece& case_id,
                        const GoogleString& head_html_in,
                        const GoogleString& head_html_out,
                        const GoogleString& body_html_in,
                        const GoogleString& body_html_out) {
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
        "http://test.com/", case_id, ".html?PageSpeed=noscript");

    GoogleString html_in(StringPrintf(
        kInWrapperFormat, head_html_in.c_str(), body_html_in.c_str()));
    GoogleString html_out(StringPrintf(
        out_wrapper_format.c_str(), head_html_out.c_str(), url.c_str(),
        url.c_str(), body_html_out.c_str()));

    // Clear request_headers and set them afresh for every test.
    ClearRewriteDriver();
    rewrite_driver()->SetRequestHeaders(request_headers_);

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
  AddDomain("example.com");
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
    RewriteTestBase::SetUp();
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
  // Enabling another filter that triggers the NOSCRIPT tag-insertion in HTML.
  options()->EnableFilter(RewriteOptions::kDeferIframe);
  options()->DisableFilter(RewriteOptions::kLocalStorageCache);
  options()->set_in_place_rewriting_enabled(true);
  server_context()->ComputeSignature(options());

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
                       StrCat("<script pagespeed_no_defer>"
                              "pagespeed.localStorageCache.inlineCss("
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
                               "=", "Fe1SLPZ14c", "!", "du_OhARrJl");
  request_headers_.Add(HttpAttributes::kCookie, cookie);

  // Third view will not send the inlined data and will send scripts in place
  // of the link and img elements.
  GoogleString scripted_css = StrCat("<script pagespeed_no_defer>"
                                     "pagespeed.localStorageCache.inlineCss("
                                     "\"", kTestDomain, kStylesCssFilename, "\""
                                     ");</script>");
  GoogleString scripted_img = StrCat("<script pagespeed_no_defer>"
                                     "pagespeed.localStorageCache.inlineImg("
                                     "\"", kTestDomain, kCuppaPngFilename, "\""
                                     ", \"du_OhARrJl\");</script>");
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
                               "=", "Fe1SLPZ14c", "!", "du_OhARrJl");
  request_headers_.Add(HttpAttributes::kCookie, cookie);

  // Third view will not send the inlined data and will send scripts in place
  // of the link and img elements.
  GoogleString scripted_css = StrCat("<script pagespeed_no_defer>"
                                     "pagespeed.localStorageCache.inlineCss("
                                     "\"", kTestDomain, kStylesCssFilename, "\""
                                     ");</script>");
  GoogleString scripted_img = StrCat("<script pagespeed_no_defer>"
                                     "pagespeed.localStorageCache.inlineImg("
                                     "\"", kTestDomain, kCuppaPngFilename, "\""
                                     ", \"du_OhARrJl\""
                                     ", \"alt=A cup of joe\""
                                     ", \"alt=A cup of joe\""
                                     ", \"alt=A cup of joe\\'s \\\"joe\\\"\""
                                     ", \"alt=A cup of joe\\'s \\\"joe\\\"\""
                                     ");</script>");
  TestLocalStorage("third_view",
                   css, InsertScriptBefore(scripted_css),
                   img, scripted_img);
}

TEST_F(LocalStorageCacheTest, RepeatViewsOfSameImageAtDifferentSizes) {
  // Enable "rewrite_images" so that the first instance of the image is
  // rescaled.
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  options()->EnableFilter(RewriteOptions::kConvertJpegToProgressive);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kJpegSubsampling);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kRecompressWebp);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kStripImageColorProfile);
  options()->EnableFilter(RewriteOptions::kStripImageMetaData);
  server_context()->ComputeSignature(options());

  UseMd5Hasher();
  const char kHash30x30[] = "07FPv8sBor";
  const char kHash150x150[] = "jSr1gEyima";

  GoogleString imgs = StrCat("<img src='", kCuppaPngFilename, "'"
                             "width=\"30\" height=\"30\">"
                             "<img src='", kCuppaPngFilename, "'"
                             "width=\"150\" height=\"150\">");

  // First view shouldn't rewrite anything though lsc_url attributes are added.
  // Don't rewrite because in the real world the fetch and processing of the
  // resource could take longer than the rewriting timeout, and we want to
  // simulate that here. We redo it below with the rewriting completing in time.
  GoogleString external_imgs =
      StrCat(StrCat("<img src='", kCuppaPngFilename, "'"
                    " width=\"30\" height=\"30\""
                    " pagespeed_lsc_url=""\"",
                    kTestDomain, kCuppaPngFilename, "\">"),
             StrCat("<img src='", kCuppaPngFilename, "'"
                    " width=\"150\" height=\"150\""
                    " pagespeed_lsc_url=\"",
                    kTestDomain, kCuppaPngFilename, "\">"));

  SetupWaitFetcher();
  TestLocalStorage("first_view", "", "",
                   imgs, InsertScriptBefore(external_imgs));
  CallFetcherCallbacks();

  // Second view will inline them and add an expiry.
  GoogleString inlined_imgs =
      StrCat(StrCat("<img src='", kCuppaPng30sqInlineData, "'"
                    // This is dropped; see below for why.
                    // " width=\"30\" height=\"30\""
                    " pagespeed_lsc_url=\"",
                    kTestDomain, kCuppaPngFilename, "\""
                    " pagespeed_lsc_hash=\"", kHash30x30, "\""
                    " pagespeed_lsc_expiry="
                    "\"Tue, 02 Feb 2010 18:53:06 GMT\">"),
             StrCat("<img src='", kCuppaPng150sqInlineData, "'"
                    " width=\"150\" height=\"150\""
                    " pagespeed_lsc_url=\"",
                    kTestDomain, kCuppaPngFilename, "\""
                    " pagespeed_lsc_hash=\"", kHash150x150, "\""
                    " pagespeed_lsc_expiry="
                    "\"Tue, 02 Feb 2010 18:53:06 GMT\">"));
  // NOTE: Why are width=30 and height=30 dropped from the first img tag?
  // Because the image rewriter calls DeleteMatchingImageDimsAfterInline for
  // each inlined image, and at this point the cached version of Cuppa.png is
  // the 30x30 version, so the attributes are stripped, but the 150x150 version
  // is different so its attributes are kept.
  // TODO(matterbury): Work out if the image rewriter needs to be smarter about
  // cached versions on inline images in this situation: same image, inlined
  // at different resolutions.
  TestLocalStorage("second_view", "", "",
                   imgs, InsertScriptBefore(inlined_imgs));

  // The JavaScript would set this cookie for the next request.
  GoogleString cookie = StrCat(LocalStorageCacheFilter::kLscCookieName,
                               "=", kHash30x30, "!", kHash150x150);
  request_headers_.Add(HttpAttributes::kCookie, cookie);

  // Third view will not send the inlined data and will send scripts in place
  // of the link and img elements.
  GoogleString scripted_imgs =
      StrCat(StrCat("<script pagespeed_no_defer>"
                    "pagespeed.localStorageCache.inlineImg("
                    "\"", kTestDomain, kCuppaPngFilename,
                    "\", \"", kHash30x30, "\""
                    ", \"width=30\", \"height=30\""
                    ");</script>"),
             StrCat("<script pagespeed_no_defer>"
                    "pagespeed.localStorageCache.inlineImg("
                    "\"", kTestDomain, kCuppaPngFilename,
                    "\", \"", kHash150x150, "\""
                    ", \"width=150\", \"height=150\""
                    ");</script>"));
  TestLocalStorage("third_view", "", "",
                   imgs, InsertScriptBefore(scripted_imgs));
}

}  // namespace

}  // namespace net_instaweb
