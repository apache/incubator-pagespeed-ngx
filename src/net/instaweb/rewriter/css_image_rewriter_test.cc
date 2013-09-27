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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/rewriter/public/css_rewrite_test_base.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/test_url_namer.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/util/public/dynamic_annotations.h"  // RunningOnValgrind
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/image/jpeg_utils.h"

using net_instaweb::MockMessageHandler;
using net_instaweb::NullMutex;
using pagespeed::image_compression::JpegUtils;

namespace net_instaweb {

namespace {

// Filenames of resource files.
const char kBikePngFile[] = "BikeCrashIcn.png";
const char kCuppaPngFile[] = "Cuppa.png";
const char kPuzzleJpgFile[] = "Puzzle.jpg";

const char kDummyContent[] = "Invalid PNG but it does not matter for this test";

const ContentType kContentTypeTtf =
    { "application/octet-stream", ".ttf", ContentType::kOther };
const ContentType kContentTypeEot =
    { "application/vnd.ms-fontobject", ".eot", ContentType::kOther };
const ContentType kContentTypeHtc =
    { "text/x-component", ".htc",  ContentType::kOther };

class CssImageRewriterTest : public CssRewriteTestBase {
 protected:
  virtual void SetUp() {
    // We setup the options before the upcall so that the
    // CSS filter is created aware of these.
    options()->EnableFilter(RewriteOptions::kExtendCacheImages);
    options()->EnableFilter(RewriteOptions::kFallbackRewriteCssUrls);
    CssRewriteTestBase::SetUp();
  }

  int RewriteCssImageCheckForQuality(const char* user_agent) {
    const char kCssFile[] = "a.css";
    const char kCssTemplate[] = "div{background-image:url(%s)}";
    AddFileToMockFetcher(StrCat(kTestDomain, kPuzzleJpgFile), kPuzzleJpgFile,
                         kContentTypeJpeg, 100);
    GoogleString in_css = StringPrintf(kCssTemplate, kPuzzleJpgFile);
    SetResponseWithDefaultHeaders(kCssFile, kContentTypeCss, in_css, 100);
    rewrite_driver()->SetUserAgent(user_agent);

    // Using a "0" hash would result in the rewritten url having the same hash
    // for mobile and non-mobile UAs. Hence, using Md5 hasher.
    UseMd5Hasher();
    Parse("image_in_css", CssLinkHref(kCssFile));
    StringVector css_links;
    CollectCssLinks("collect", output_buffer_, &css_links);
    EXPECT_EQ(1, css_links.size());
    GoogleString out_css;
    ResponseHeaders headers;
    EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, css_links[0]),
                                 &out_css, &headers));

    // Find the image URL embedded in the CSS output.
    GoogleString image_url = ExtractCssBackgroundImage(out_css);
    if (image_url.empty()) {
      return -1;
    }
    // FetchResourceUrl clears the rewrite driver. Add back the mobile UA.
    rewrite_driver()->SetUserAgent(user_agent);

    StringPiece out_image;
    HTTPValue value_out;
    ResponseHeaders headers_out;
    EXPECT_EQ(HTTPCache::kFound,
              HttpBlockingFind(StrCat(kTestDomain, image_url), http_cache(),
                               &value_out, &headers_out));
    value_out.ExtractContents(&out_image);
    MockMessageHandler message_handler(new NullMutex);
    int quality = JpegUtils::GetImageQualityFromImage(out_image.data(),
                                                      out_image.size(),
                                                      &message_handler);
    return quality;
  }
};

TEST_F(CssImageRewriterTest, CacheExtendsImagesSimple) {
  // Simplified version of CacheExtendsImages, which doesn't have many copies of
  // the same URL.
  SetResponseWithDefaultHeaders("foo.png", kContentTypePng, kDummyContent, 100);

  static const char css_before[] =
      "body {\n"
      "  background-image: url(foo.png);\n"
      "}\n";
  const GoogleString css_after =
      StrCat("body{background-image:url(",
             Encode("", "ce", "0", "foo.png", "png"),
             ")}");

  ValidateRewrite("cache_extends_images", css_before, css_after,
                  kExpectSuccess | kNoClearFetcher);
}

TEST_F(CssImageRewriterTest, CacheExtendsImagesEmbeddedComma) {
  // Makes sure image-URL rewriting doesn't corrupt URLs with embedded
  // commas.  Earlier, we were escaping commas in URLs by backslashing
  // the "," and IE8 interprets those backslashes as forward slashes,
  // making the URL incorrect.
  static const char kImageUrl[] = "foo,bar.png";
  SetResponseWithDefaultHeaders(kImageUrl, kContentTypePng, kDummyContent, 100);

  static const char css_before[] =
      "body {\n"
      "  background-image: url(foo,bar.png);\n"
      "}\n";
  const GoogleString css_after =
      StrCat("body{background-image:url(",
             Encode("", "ce", "0", kImageUrl, "png"),
             ")}");

  ValidateRewrite("cache_extends_images", css_before, css_after,
                  kExpectSuccess | kNoClearFetcher);
}

TEST_F(CssImageRewriterTest, CacheExtendsImagesEmbeddedSpace) {
  // Note that GoogleUrl will, internal to our system, convert the space to
  // a %20, so we'll be fetching the percentified form.
  SetResponseWithDefaultHeaders("foo%20bar.png", kContentTypePng,
                                kDummyContent, 100);

  static const char css_before[] =
      "body {\n"
      "  background-image: url('foo bar.png');\n"
      "}\n";
  const GoogleString css_after =
      StrCat("body{background-image:url(",
             Encode("", "ce", "0", "foo%20bar.png", "png"),
             ")}");

  ValidateRewrite("cache_extends_images", css_before, css_after,
                  kExpectSuccess | kNoClearFetcher);
}

TEST_F(CssImageRewriterTest, MinifyImagesEmbeddedSpace) {
  options()->ClearSignatureForTesting();
  options()->DisableFilter(RewriteOptions::kExtendCacheImages);
  server_context()->ComputeSignature(options());

  ValidateRewrite("minify",
                  MakeIndentedCssWithImage("'foo bar.png'"),
                  MakeMinifiedCssWithImage("foo\\ bar.png"),
                  kExpectSuccess | kNoClearFetcher);
}

TEST_F(CssImageRewriterTest, RewriteCssImagesVerifyQuality) {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->set_image_max_rewrites_at_once(1);
  options()->set_always_rewrite_css(true);
  options()->set_image_jpeg_recompress_quality(85);
  options()->set_image_jpeg_recompress_quality_for_small_screens(60);
  server_context()->ComputeSignature(options());

  // Recompress quality set for desktop is 85 and mobile is 60. Verify this
  // applied correctly by verifying the quality of the output image.
  int mobile_quality = RewriteCssImageCheckForQuality(
      UserAgentMatcherTestBase::kIPhoneUserAgent);
  EXPECT_EQ(60, mobile_quality);
  int non_mobile_quality = RewriteCssImageCheckForQuality(
      UserAgentMatcherTestBase::kChrome15UserAgent);
  EXPECT_EQ(85, non_mobile_quality);
}

TEST_F(CssImageRewriterTest, CacheExtendsWhenCssGrows) {
  // We run most tests with set_always_rewrite_css(true) which bypasses
  // checks on whether rewriting is worthwhile or not. Test to make sure we make
  // the right decision when we do do the check in the case where the produced
  // CSS is actually larger, but contains rewritten resources.
  // (We want to rewrite the CSS in that case)
  options()->ClearSignatureForTesting();
  options()->set_always_rewrite_css(false);
  server_context()->ComputeSignature(options());
  SetResponseWithDefaultHeaders("foo.png", kContentTypePng, kDummyContent, 100);
  ValidateRewrite("cache_extends_images_growcheck",
                  MakeIndentedCssWithImage("foo.png"),
                  MakeMinifiedCssWithImage(
                      Encode("", "ce", "0", "foo.png", "png")),
                  kExpectSuccess | kNoClearFetcher);
}

TEST_F(CssImageRewriterTest, CacheExtendsRepeatedTopLevel) {
  // Test to make sure that if we cache extend inside CSS we can do it
  // for the same image in HTML at the same time.
  const char kImg[] = "img.png";
  const GoogleString kExtendedImg = Encode("", "ce", "0", "img.png", "png");

  const char kCss[] = "stylesheet.css";
  const GoogleString kRewrittenCss =
      Encode("", "cf", "0", "stylesheet.css", "css");

  SetResponseWithDefaultHeaders(kImg, kContentTypePng, kDummyContent, 100);
  SetResponseWithDefaultHeaders(
      kCss, kContentTypeCss, MakeMinifiedCssWithImage(kImg), 100);

  const char kHtmlTemplate[] =
      "<link rel='stylesheet' href='%s'>"
      "<img src='%s'>";

  ValidateExpected("repeated_top_level",
                   StringPrintf(kHtmlTemplate, kCss, kImg),
                   StringPrintf(kHtmlTemplate,
                                kRewrittenCss.c_str(),
                                kExtendedImg.c_str()));

  GoogleString css_out;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, kRewrittenCss), &css_out));
  EXPECT_EQ(MakeMinifiedCssWithImage(kExtendedImg), css_out);
}

TEST_F(CssImageRewriterTest, CacheExtendsImages) {
  SetResponseWithDefaultHeaders("foo.png", kContentTypePng, kDummyContent, 100);
  SetResponseWithDefaultHeaders("bar.png", kContentTypePng, kDummyContent, 100);
  SetResponseWithDefaultHeaders("baz.png", kContentTypePng, kDummyContent, 100);

  static const char css_before[] =
      "body {\n"
      "  background-image: url(foo.png);\n"
      "  list-style-image: url('bar.png');\n"
      "}\n"
      ".titlebar p.cfoo, #end p {\n"
      "  background: url(\"baz.png\");\n"
      "  list-style: url('foo.png');\n"
      "}\n"
      ".other {\n"
      "  background-image:url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAA"
      "AUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4"
      "OHwAAAABJRU5ErkJggg==);"
      "  -proprietary-background-property: url(foo.png);\n"
      "}";
  const GoogleString css_after = StrCat(StrCat(
      "body{background-image:url(", Encode("", "ce", "0", "foo.png", "png"),
      ");"
      "list-style-image:url(", Encode("", "ce", "0", "bar.png", "png"), ")}"
      ".titlebar p.cfoo,#end p{"
      "background:url(", Encode("", "ce", "0", "baz.png", "png"), ");"
      "list-style:url(", Encode("", "ce", "0", "foo.png", "png")), ")}"
      ".other{"  // data: URLs and unknown properties are not rewritten.
      "background-image:url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAA"
      "AUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4"
      "OHwAAAABJRU5ErkJggg==);"
      "-proprietary-background-property:url(foo.png)}");

  ValidateRewrite("cache_extends_images", css_before, css_after,
                  kExpectSuccess | kNoClearFetcher);
}

// See TrimsImageUrls below: change one, change them both!
TEST_F(CssImageRewriterTest, TrimsImageUrls) {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kLeftTrimUrls);
  server_context()->ComputeSignature(options());
  SetResponseWithDefaultHeaders("foo.png", kContentTypePng, kDummyContent, 100);
  ValidateRewriteExternalCss("trims_css_urls",
                             MakeIndentedCssWithImage("foo.png"),
                             MakeMinifiedCssWithImage(
                                 Encode("", "ce", "0", "foo.png", "png")),
                             kExpectSuccess | kNoClearFetcher);
}

class CssImageRewriterTestUrlNamer : public CssImageRewriterTest {
 public:
  CssImageRewriterTestUrlNamer() {
    SetUseTestUrlNamer(true);
  }
};

// See TrimsImageUrls above: change one, change them both!
TEST_F(CssImageRewriterTestUrlNamer, TrimsImageUrls) {
  // Check that we really are using TestUrlNamer and not UrlNamer.
  EXPECT_NE(Encode(kTestDomain, "ce", "0", "foo.png", "png"),
            EncodeNormal(kTestDomain, "ce", "0", "foo.png", "png"));

  // A verbatim copy of the test above but using TestUrlNamer.
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kLeftTrimUrls);
  server_context()->ComputeSignature(options());
  SetResponseWithDefaultHeaders("foo.png", kContentTypePng, kDummyContent, 100);
  ValidateRewriteExternalCss("trims_css_urls",
                             MakeIndentedCssWithImage("foo.png"),
                             MakeMinifiedCssWithImage(
                                 Encode("", "ce", "0", "foo.png", "png")),
                             kExpectSuccess | kNoClearFetcher);
}

TEST_F(CssImageRewriterTest, InlinePaths) {
  // Make sure we properly handle CSS relative references when we have the same
  // inline CSS in different places. This is also a regression test for a bug
  // during development of async + inline case which caused us to do
  // null rewrites from cache.
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kLeftTrimUrls);
  server_context()->ComputeSignature(options());
  SetResponseWithDefaultHeaders("dir/foo.png", kContentTypePng,
                                kDummyContent, 100);

  static const char kCssBefore[] =
      "body {\n"
      "  background-image: url(http://test.com/dir/foo.png);\n"
      "}\n";

  // Force all URL encoding to use normal encoding so that the relative URL
  // trimming logic can work and give us a relative URL result as expected.
  TestUrlNamer::UseNormalEncoding(true);

  // Note: Original URL was absolute, so rewritten one is as well.
  const GoogleString kCssAfter = StrCat(
      "body{background-image:url(",
      Encode("dir/", "ce", "0", "foo.png", "png"),
      ")}");
  ValidateRewriteInlineCss("nosubdir", kCssBefore, kCssAfter, kExpectSuccess);

  const GoogleString kCssAfterRel = StrCat(
      "body{background-image:url(",
      Encode("", "ce", "0", "foo.png", "png"),
      ")}");
  ValidateRewriteInlineCss("dir/yessubdir", kCssBefore, kCssAfterRel,
                           kExpectSuccess);
}

TEST_F(CssImageRewriterTest, RewriteCached) {
  // Make sure we produce the same output from cache.
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kLeftTrimUrls);
  server_context()->ComputeSignature(options());
  SetResponseWithDefaultHeaders("dir/foo.png", kContentTypePng,
                                kDummyContent, 100);

  static const char kCssBefore[] =
      "body {\n"
      "  background-image: url(http://test.com/dir/foo.png);\n"
      "}\n";

  GoogleString kBaseDomain;
  // If using the TestUrlNamer, the rewritten URL won't be relative so
  // set things up so that we check for the correct URL below.
  if (factory()->use_test_url_namer()) {
    kBaseDomain = kTestDomain;
  }

  const GoogleString kCssAfter = StrCat(
      "body{background-image:url(",
      Encode(StrCat(kBaseDomain, "dir/"), "ce", "0", "foo.png", "png"),
      ")}");
  ValidateRewriteInlineCss("nosubdir",
                           kCssBefore, kCssAfter,
                           kExpectSuccess);

  statistics()->Clear();
  ValidateRewriteInlineCss("nosubdir2",
                           kCssBefore, kCssAfter,
                           kExpectSuccess | kNoStatCheck);
  // Should not re-serialize. Works only under the new flow...
  EXPECT_EQ(0, total_bytes_saved_->Get());
}

// Test that we remember parse failures.
TEST_F(CssImageRewriterTest, CacheInlineParseFailures) {
  const char kInvalidCss[] = " div{";

  ValidateRewriteInlineCss("inline-invalid", kInvalidCss, kInvalidCss,
                           kExpectFallback);
  EXPECT_EQ(1, num_parse_failures_->Get());

  // kNoStatCheck because we are explicitly depending on an extra failure
  // not being recorded.
  ValidateRewriteInlineCss("inline-invalid2", kInvalidCss, kInvalidCss,
                           kExpectFallback | kNoStatCheck);
  // Shouldn't reparse -- and stats are reset between runs.
  EXPECT_EQ(0, num_parse_failures_->Get());
}

TEST_F(CssImageRewriterTest, RecompressImages) {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  server_context()->ComputeSignature(options());
  AddFileToMockFetcher(StrCat(kTestDomain, "foo.png"), kBikePngFile,
                       kContentTypePng, 100);
  static const char kCss[] =
      "body {\n"
      "  background-image: url(foo.png);\n"
      "}\n";

  const GoogleString kCssAfter = StrCat(
      "body{background-image:url(",
      Encode("", "ic", "0", "foo.png", "png"),
      ")}");

  ValidateRewriteExternalCss("recompress_css_images", kCss, kCssAfter,
                              kExpectSuccess | kNoClearFetcher);
}

TEST_F(CssImageRewriterTest, CssImagePreserveUrls) {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->set_image_preserve_urls(true);
  server_context()->ComputeSignature(options());
  AddFileToMockFetcher(StrCat(kTestDomain, "foo.png"), kBikePngFile,
                       kContentTypePng, 100);
  static const char kCss[] =
      "body {\n"
      "  background-image: url(foo.png);\n"
      "}\n";

  const GoogleString kCssAfter =  "body{background-image:url(foo.png)}";
  // The CSS should minify but the URL shouldn't change.
  ValidateRewriteExternalCss("compress_preserve_css_images", kCss, kCssAfter,
                             kExpectSuccess | kNoClearFetcher);

  // We should have optimized the image even though we didn't render the URL.
  ClearStats();
  GoogleString out_img_url = Encode(kTestDomain, "ic", "0", "foo.png", "png");
  GoogleString out_img;
  EXPECT_TRUE(FetchResourceUrl(out_img_url, &out_img));
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, static_cast<int>(lru_cache()->num_hits()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_misses()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_inserts()));
}

TEST_F(CssImageRewriterTest, CssImagePreserveUrlsNoPreemptiveRewrite) {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->set_image_preserve_urls(true);
  options()->set_in_place_preemptive_rewrite_css_images(false);
  server_context()->ComputeSignature(options());
  AddFileToMockFetcher(StrCat(kTestDomain, "foo.png"), kBikePngFile,
                       kContentTypePng, 100);
  static const char kCss[] =
      "body {\n"
      "  background-image: url(foo.png);\n"
      "}\n";

  const GoogleString kCssAfter =  "body{background-image:url(foo.png)}";
  // The CSS should minify but the URL shouldn't change.
  ValidateRewriteExternalCss("compress_preserve_css_images", kCss, kCssAfter,
                             kExpectSuccess | kNoClearFetcher);

  // We should not find a cache hit when requesting the image, indicating it
  // has not been optimized, in contrast with the CssImagePreserveUrls test.
  ClearStats();
  GoogleString out_img_url = Encode(kTestDomain, "ic", "0", "foo.png", "png");
  GoogleString out_img;
  EXPECT_TRUE(FetchResourceUrl(out_img_url, &out_img));
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_hits()));
}

class InlineCssImageRewriterTest : public CssImageRewriterTest {
 protected:
  int NumTestImageBytes() { return test_image_file_contents_.size(); }

  GoogleString TestImageFileName() { return kCuppaPngFile; }

  GoogleString TestImageDataUrl() {
    GoogleString data_url;
    DataUrl(kContentTypePng, BASE64, test_image_file_contents_, &data_url);
    return data_url;
  }

  void SetMaxBytes(int image_inline_max_bytes,
                   int css_image_inline_max_bytes) {
    options()->ClearSignatureForTesting();
    options()->set_image_inline_max_bytes(image_inline_max_bytes);
    options()->set_css_image_inline_max_bytes(css_image_inline_max_bytes);
    EXPECT_EQ(image_inline_max_bytes, options()->ImageInlineMaxBytes());
    EXPECT_EQ(css_image_inline_max_bytes, options()->CssImageInlineMaxBytes());
    server_context()->ComputeSignature(options());
  }

  virtual void SetUp() {
    options()->EnableFilter(RewriteOptions::kInlineImages);
    CssImageRewriterTest::SetUp();
    SetUpTestImageFile();
    EXPECT_FALSE(test_image_file_contents_.empty());
  }

 private:
  void SetUpTestImageFile() {
    GoogleString file_name = TestImageFileName();
    AddFileToMockFetcher(StrCat(kTestDomain, file_name),
                         file_name, kContentTypePng, 100);

    StdioFileSystem stdio_file_system;
    GoogleString file_path = StrCat(GTestSrcDir(), kTestData, file_name);
    EXPECT_TRUE(stdio_file_system.ReadFile(
        file_path.c_str(), &test_image_file_contents_, message_handler()));
  }

  GoogleString test_image_file_contents_;
};

TEST_F(InlineCssImageRewriterTest, InlineImages) {
  SetMaxBytes(NumTestImageBytes() + 1,  // image_inline_max_bytes
              NumTestImageBytes() + 1);  // css_image_inline_max_bytes
  GoogleString input_css = StrCat(
      "body {\n"
      "  background-image: url(", TestImageFileName(), ");\n"
      "}\n");
  GoogleString expected_css = StrCat(
      "body{background-image:url(", TestImageDataUrl(), ")}");

  // Skip the stat check because inlining *increases* the CSS size and
  // causes the check to fail. Inlining eliminates a resource fetch, so
  // it should normally be a net win in practice.
  ValidateRewrite("inline_css_images", input_css, expected_css,
                  kExpectSuccess | kNoClearFetcher | kNoStatCheck);
}

TEST_F(InlineCssImageRewriterTest, InlineImagesInFallbackMode) {
  SetMaxBytes(NumTestImageBytes() + 1,  // image_inline_max_bytes
              NumTestImageBytes() + 1);  // css_image_inline_max_bytes
  // This ought to not parse.
  static const char kCssTemplate[] =
      "body {\n"
      "  background-image: url(%s);\n"
      "}}}}}\n";
  GoogleString input_css = StringPrintf(
      kCssTemplate, TestImageFileName().c_str());
  GoogleString expected_css = StringPrintf(
      kCssTemplate, TestImageDataUrl().c_str());

  // Skip the stat check because inlining *increases* the CSS size and
  // causes the check to fail. Inlining eliminates a resource fetch, so
  // it should normally be a net win in practice.
  ValidateRewrite("inline_images_in_fallback_mode", input_css, expected_css,
                  kExpectFallback | kNoClearFetcher | kNoStatCheck);
}

TEST_F(InlineCssImageRewriterTest, NoInlineWhenImageTooLargeForCss) {
  SetMaxBytes(NumTestImageBytes() + 1,  // image_inline_max_bytes
              NumTestImageBytes());  // css_image_inline_max_bytes
  GoogleString file_name = TestImageFileName();
  GoogleString input_css = StrCat(
      "body {\n"
      "  background-image: url(", file_name, ");\n"
      "}\n");
  GoogleString expected_css = StrCat(
      "body{background-image:url(",
      Encode("", "ce", "0", file_name, "png"), ")}");

  ValidateRewrite("no_inline_when_image_too_large_for_css",
                  input_css, expected_css, kExpectSuccess | kNoClearFetcher);
}

TEST_F(InlineCssImageRewriterTest, InlineInExternalCssOnly) {
  SetMaxBytes(NumTestImageBytes(),  // image_inline_max_bytes
              NumTestImageBytes() + 1);  // css_image_inline_max_bytes
  GoogleString file_name = TestImageFileName();
  GoogleString input_css = StrCat(
      "body {\n"
      "  background-image: url(", file_name, ");\n"
      "}\n");
  GoogleString expected_inline_css = StrCat(
      "body{background-image:url(",
      Encode("", "ce", "0", file_name, "png"), ")}");
  GoogleString expected_outline_css = StrCat(
      "body{background-image:url(", TestImageDataUrl(), ")}");

  ValidateRewriteInlineCss(
      "no_inline_in_inline", input_css, expected_inline_css,
      kExpectSuccess | kNoClearFetcher);
  ValidateRewriteExternalCss(
      "inline_in_external", input_css, expected_outline_css,
      kExpectSuccess | kNoClearFetcher | kNoStatCheck);
}

TEST_F(CssImageRewriterTest, UseCorrectBaseUrl) {
  // Initialize resources.
  static const char css_url[] = "http://www.example.com/bar/style.css";
  static const char css_before[] = "body { background: url(image.png); }";
  SetResponseWithDefaultHeaders(css_url, kContentTypeCss, css_before, 100);
  static const char image_url[] = "http://www.example.com/bar/image.png";
  SetResponseWithDefaultHeaders(image_url, kContentTypePng, kDummyContent, 100);

  // Construct URL for rewritten image.
  GoogleString expected_image_url =
      Encode("", RewriteOptions::kCacheExtenderId,
             hasher()->Hash(kDummyContent), "image.png",
             // + 1 Needed to exclude "." from the extension.
             kContentTypePng.file_extension_ + 1);

  GoogleString css_after = StrCat(
      "body{background:url(", expected_image_url, ")}");

  // Construct URL for rewritten CSS.
  GoogleString expected_css_url =
      Encode("bar/", RewriteOptions::kCssFilterId,
             hasher()->Hash(css_after), "style.css",
             // + 1 Needed to exclude "." from the extension.
             kContentTypeCss.file_extension_ + 1);

  static const char html_before[] =
      "<head>\n"
      "  <link rel='stylesheet' href='bar/style.css'>\n"
      "</head>";
  GoogleString html_after = StrCat(
      "<head>\n"
      "  <link rel='stylesheet' href='", expected_css_url, "'>\n"
      "</head>");

  // Make sure that image.png uses http://www.example.com/bar/style.css as
  // base URL instead of http://www.example.com/.
  ValidateExpectedUrl("http://www.example.com/", html_before, html_after);

  GoogleString actual_css_after;
  FetchResourceUrl(StrCat("http://www.example.com/", expected_css_url),
                   &actual_css_after);
  EXPECT_EQ(css_after, actual_css_after);
}

TEST_F(CssImageRewriterTest, CacheExtendsImagesInStyleAttributes) {
  SetResponseWithDefaultHeaders("foo.png", kContentTypePng, kDummyContent, 100);
  SetResponseWithDefaultHeaders("bar.png", kContentTypePng, kDummyContent, 100);
  SetResponseWithDefaultHeaders("baz.png", kContentTypePng, kDummyContent, 100);

  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRewriteStyleAttributes);
  server_context()->ComputeSignature(options());

  ValidateExpected("cache_extend_images_simple",
                   "<div style=\""
                   "  background-image: url(foo.png);\n"
                   "  list-style-image: url('bar.png');\n"
                   "\"/>",
                   StrCat(
                   "<div style=\""
                   "background-image:"
                   "url(", Encode("", "ce", "0", "foo.png", "png"), ");",
                   "list-style-image:"
                   "url(", Encode("", "ce", "0", "bar.png", "png"), ")",
                   "\"/>"));

  ValidateExpected("cache_extend_images",
                   "<div style=\""
                   "  background: url(baz.png);\n"
                   "  list-style: url(&quot;foo.png&quot;);\n"
                   "\"/>",
                   StrCat(
                   "<div style=\""
                   "background:"
                   "url(", Encode("", "ce", "0", "baz.png", "png"), ");"
                   "list-style:"
                   "url(", Encode("", "ce", "0", "foo.png", "png"), ")"
                   "\"/>"));

  ValidateExpected("dont_cache_extend_data_urls",
                   "<div style=\""
                   "  background-image:url(data:image/png;base64,"
                   "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P"
                   "4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==);"
                   "  -proprietary-background-property: url(foo.png);\n"
                   "\"/>",
                   "<div style=\""
                   "background-image:url(data:image/png;base64,"
                   "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P"
                   "4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==);"
                   "-proprietary-background-property:url(foo.png)"
                   "\"/>");
}

// Fallback rewriter tests

TEST_F(CssImageRewriterTest, CacheExtendsImagesSimpleFallback) {
  SetResponseWithDefaultHeaders("foo.png", kContentTypePng, kDummyContent, 100);

  // Note: Extra }s cause parse failure.
  static const char css_template[] =
      "body {\n"
      "  background-image: url(%s);\n"
      "}}}}}\n";
  const GoogleString css_before = StringPrintf(css_template, "foo.png");
  const GoogleString css_after = StringPrintf(
      css_template, Encode("", "ce", "0", "foo.png", "png").c_str());

  ValidateRewrite("unparseable", css_before, css_after,
                  kExpectFallback | kNoClearFetcher);
}

TEST_F(CssImageRewriterTest, CacheExtendsRepeatedTopLevelFallback) {
  // Test to make sure that if we cache extend inside CSS we can do it
  // for the same image in HTML at the same time.
  const char kImg[] = "img.png";
  const GoogleString kExtendedImg = Encode("", "ce", "0", "img.png", "png");

  const char kCss[] = "stylesheet.css";
  const GoogleString kRewrittenCss =
      Encode("", "cf", "0", "stylesheet.css", "css");
  // Note: Extra }s cause parse failure.
  const char kCssTemplate[] = "body{background-image:url(%s)}}}}}";

  SetResponseWithDefaultHeaders(kImg, kContentTypePng, kDummyContent, 100);
  SetResponseWithDefaultHeaders(
      kCss, kContentTypeCss, StringPrintf(kCssTemplate, kImg), 100);

  const char kHtmlTemplate[] =
      "<link rel='stylesheet' href='%s'>"
      "<img src='%s'>";

  ValidateExpected("repeated_top_level",
                   StringPrintf(kHtmlTemplate, kCss, kImg),
                   StringPrintf(kHtmlTemplate,
                                kRewrittenCss.c_str(),
                                kExtendedImg.c_str()));

  GoogleString css_out;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, kRewrittenCss), &css_out));
  EXPECT_EQ(StringPrintf(kCssTemplate, kExtendedImg.c_str()), css_out);
}

TEST_F(CssImageRewriterTest, CacheExtendsImagesFallback) {
  SetResponseWithDefaultHeaders("foo.png", kContentTypePng, kDummyContent, 100);
  SetResponseWithDefaultHeaders("bar.png", kContentTypePng, kDummyContent, 100);
  SetResponseWithDefaultHeaders("baz.png", kContentTypePng, kDummyContent, 100);

  static const char css_template[] =
      "body {\n"
      "  background-image: url(%s);\n"
      "  list-style-image: url('%s');\n"
      "}\n"
      ".titlebar p.cfoo, #end p {\n"
      "  background: url(\"%s\");\n"
      "  list-style: url('%s');\n"
      "}\n"
      ".other {\n"
      "  background-image:url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAA"
      "AUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4"
      "OHwAAAABJRU5ErkJggg==);"
      "  -proprietary-background-property: url(%s);\n"
      // Note: Extra }s cause parse failure.
      "}}}}}}";
  const GoogleString css_before = StringPrintf(
      css_template, "foo.png", "bar.png", "baz.png", "foo.png", "foo.png");
  const GoogleString css_after = StringPrintf(
      css_template,
      Encode("", "ce", "0", "foo.png", "png").c_str(),
      Encode("", "ce", "0", "bar.png", "png").c_str(),
      Encode("", "ce", "0", "baz.png", "png").c_str(),
      Encode("", "ce", "0", "foo.png", "png").c_str(),
      Encode("", "ce", "0", "foo.png", "png").c_str());


  ValidateRewrite("cache_extends_images", css_before, css_after,
                  kExpectFallback | kNoClearFetcher);
}

TEST_F(CssImageRewriterTest, RecompressImagesFallback) {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  server_context()->ComputeSignature(options());
  AddFileToMockFetcher(StrCat(kTestDomain, "foo.png"), kBikePngFile,
                       kContentTypePng, 100);
  // Note: Extra }s cause parse failure.
  static const char css_template[] =
      "body {\n"
      "  background-image: url(%s);\n"
      "}}}}}\n";
  const GoogleString css_before = StringPrintf(css_template, "foo.png");
  const GoogleString css_after = StringPrintf(
      css_template, Encode("", "ic", "0", "foo.png", "png").c_str());

  ValidateRewriteExternalCss("recompress_css_images", css_before, css_after,
                             kExpectFallback | kNoClearFetcher);
}

// Make sure we don't break import URLs or other non-image URLs.
TEST_F(CssImageRewriterTest, FallbackImportsAndUnknownContentType) {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  server_context()->ComputeSignature(options());

  AddFileToMockFetcher(StrCat(kTestDomain, "image.png"), kBikePngFile,
                       kContentTypePng, 100);
  SetResponseWithDefaultHeaders("style.css", kContentTypeCss,
                                kDummyContent, 100);
  SetResponseWithDefaultHeaders("zero.css", kContentTypeCss, kDummyContent, 0);
  SetResponseWithDefaultHeaders("doc.html", kContentTypeHtml,
                                kDummyContent, 100);

  SetResponseWithDefaultHeaders("behavior.htc", kContentTypeHtc,
                                kDummyContent, 100);
  SetResponseWithDefaultHeaders("font.ttf", kContentTypeTtf,
                                kDummyContent, 100);
  SetResponseWithDefaultHeaders("font.eot", kContentTypeEot,
                                kDummyContent, 100);

  static const char css_template[] =
      "@import '%s';"
      // zero.css doesn't get cache extended because it has 0 TTL.
      "@import url(zero.css);"
      "@font-face {\n"
      "  font-family: name;\n"
      // Unapproved Content-Types do not get cache extended.
      "  src: url('font.ttf'), url(font.eot);\n"
      "}\n"
      "body {\n"
      "  background-image: url(%s);\n"
      // Unapproved Content-Types do not get cache extended.
      "  behavior: url(behavior.htc);\n"
      "  -moz-content-file: url(doc.html);\n"
      // Note: Extra }s cause parse failure.
      "}}}}}\n";
  const GoogleString css_before = StringPrintf(
      css_template, "style.css", "image.png");
  const GoogleString css_after = StringPrintf(
      css_template,
      Encode("", "ce", "0", "style.css", "css").c_str(),
      Encode("", "ic", "0", "image.png", "png").c_str());

  ValidateRewriteExternalCss("recompress_css_images", css_before, css_after,
                             kExpectFallback | kNoClearFetcher);
}

// Test that the fallback fetcher fails smoothly.
TEST_F(CssImageRewriterTest, FallbackFails) {
  // Note: //// is not a valid URL leading to fallback rewrite failure.
  static const char bad_css[] = ".foo { url(////); }}}}}}";
  ValidateRewrite("fallback_fails", bad_css, bad_css, kExpectFailure);
}

// Check that we absolutify URLs when moving CSS.
TEST_F(CssImageRewriterTest, FallbackAbsolutify) {
  options()->ClearSignatureForTesting();
  DomainLawyer* lawyer = options()->WriteableDomainLawyer();
  lawyer->AddRewriteDomainMapping("http://new_domain.com", kTestDomain,
                                  &message_handler_);
  // Turn off trimming to make sure we can see full absolutifications.
  options()->DisableFilter(RewriteOptions::kLeftTrimUrls);
  server_context()->ComputeSignature(options());

  SetResponseWithDefaultHeaders("foo.png", kContentTypePng, kDummyContent, 0);

  // Note: Extra }s cause parse failure.
  const char css_template[] = ".foo { background: url(%s); }}}}";
  const GoogleString css_before = StringPrintf(css_template, "foo.png");
  const GoogleString css_after = StringPrintf(css_template,
                                              "http://new_domain.com/foo.png");

  // We only test inline CSS because ValidateRewriteExternalCss doesn't work
  // with AddRewriteDomainMapping.
  ValidateRewriteInlineCss("change_domain", css_before, css_after,
                           kExpectFallback | kNoClearFetcher);

  // Test loading from other domains.
  SetResponseWithDefaultHeaders("other_domain.css", kContentTypeCss,
                                css_before, 100);

  const char rewritten_url[] =
      "http://test.com/I.other_domain.css.pagespeed.cf.0.css";
  ServeResourceFromManyContexts(rewritten_url, css_after);
}

// Check that we don't absolutify URLs when not moving them.
TEST_F(CssImageRewriterTest, FallbackNoAbsolutify) {
  options()->ClearSignatureForTesting();
  // Turn off trimming to make sure we can see full absolutifications.
  options()->DisableFilter(RewriteOptions::kLeftTrimUrls);
  server_context()->ComputeSignature(options());

  SetResponseWithDefaultHeaders("foo.png", kContentTypePng, kDummyContent, 0);

  // Note: Extra }s cause parse failure.
  const char css[] = ".foo { background: url(foo.png); }}}}";

  ValidateRewrite("change_domain", css, css, kExpectFallback | kNoClearFetcher);
}

// Check that we still absolutify URLs even if we fail to parse CSS while
// rewriting on a fetch. This can come up if you have different rewrite
// options on the HTML and resources-serving servers or if the resource
// changes between the HTML and resource servers (race condition during push).
TEST_F(CssImageRewriterTest, FetchRewriteFailure) {
  options()->ClearSignatureForTesting();
  DomainLawyer* lawyer = options()->WriteableDomainLawyer();
  lawyer->AddRewriteDomainMapping("http://new_domain.com", kTestDomain,
                                  &message_handler_);
  // Turn off trimming to make sure we can see full absolutifications.
  options()->DisableFilter(RewriteOptions::kLeftTrimUrls);
  options()->DisableFilter(RewriteOptions::kFallbackRewriteCssUrls);
  server_context()->ComputeSignature(options());

  SetResponseWithDefaultHeaders("foo.png", kContentTypePng, kDummyContent, 0);

  // Note: Extra }s cause parse failure.
  const char css_template[] = ".foo { background: url(%s); }}}}";
  const GoogleString css_before = StringPrintf(css_template, "foo.png");
  const GoogleString css_after = StringPrintf(css_template,
                                              "http://new_domain.com/foo.png");

  // Test loading from other domains.
  SetResponseWithDefaultHeaders("other_domain.css", kContentTypeCss,
                                css_before, 100);

  GoogleString content;
  FetchResource(kTestDomain, "cf", "other_domain.css", "css", &content);
  EXPECT_STREQ(css_after, content);
  EXPECT_EQ(0, num_fallback_rewrites_->Get());
  EXPECT_EQ(1, num_parse_failures_->Get());

  // Check that this still works correctly the second time (this loads the
  // result from cache and so goes through a different code path).
  content.clear();
  FetchResource(kTestDomain, "cf", "other_domain.css", "css", &content);
  EXPECT_STREQ(css_after, content);
}

TEST_F(CssImageRewriterTest, DummyRuleset) {
  // Simplified version of CacheExtendsImages, which doesn't have many copies of
  // the same URL.
  SetResponseWithDefaultHeaders("foo.png", kContentTypePng, kDummyContent, 100);

  static const char css_before[] =
      "@font-face { font-family: 'Robotnik'; font-style: normal }\n"
      "body {\n"
      "  background-image: url(foo.png);\n"
      "}\n"
      "@to-infinity and beyond;\n";
  const GoogleString css_after =
      StrCat("@font-face { font-family: 'Robotnik'; font-style: normal }"
             "body{background-image:url(",
             Encode("", "ce", "0", "foo.png", "png"),
             ")}@to-infinity and beyond;");

  ValidateRewrite("cache_extends_images", css_before, css_after,
                  kExpectSuccess | kNoClearFetcher);
}

class CssRecompressImagesInStyleAttributes : public RewriteTestBase {
 protected:
  CssRecompressImagesInStyleAttributes()
      : div_before_(
          "<div style=\""
          "background-image:url(foo.png)"
          "\"/>") {}

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    options()->EnableFilter(RewriteOptions::kRewriteCss);
    options()->EnableFilter(RewriteOptions::kFallbackRewriteCssUrls);
    options()->set_always_rewrite_css(true);
    AddFileToMockFetcher(StrCat(kTestDomain, "foo.png"), kBikePngFile,
                         kContentTypePng, 100);
    div_after_ = StrCat(
        "<div style=\""
        "background-image:url(",
        Encode("", "ic", "0", "foo.png", "png"),
        ")"
        "\"/>");
  }

  GoogleString div_before_;
  GoogleString div_after_;
};

// No rewriting if neither option is enabled.
TEST_F(CssRecompressImagesInStyleAttributes, NeitherEnabled) {
  ValidateNoChanges("options_disabled", div_before_);
}

// No rewriting if only 'style' is enabled.
TEST_F(CssRecompressImagesInStyleAttributes, OnlyStyleEnabled) {
  AddFilter(RewriteOptions::kRewriteStyleAttributesWithUrl);
  ValidateNoChanges("recompress_images_disabled", div_before_);
}

// No rewriting if only 'recompress' is enabled.
TEST_F(CssRecompressImagesInStyleAttributes, OnlyRecompressEnabled) {
  AddRecompressImageFilters();
  rewrite_driver()->AddFilters();
  ValidateNoChanges("recompress_images_disabled", div_before_);
}

// Rewrite iff both options are enabled.
TEST_F(CssRecompressImagesInStyleAttributes, RecompressAndStyleEnabled) {
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kRewriteStyleAttributesWithUrl);
  rewrite_driver()->AddFilters();
  ValidateExpected("options_enabled", div_before_, div_after_);
}

TEST_F(CssRecompressImagesInStyleAttributes, RecompressAndWebpAndStyleEnabled) {
  if (RunningOnValgrind()) {  // Too slow under vg.
    return;
  }

  AddFileToMockFetcher(StrCat(kTestDomain, "foo.jpg"), kPuzzleJpgFile,
                       kContentTypeJpeg, 100);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kRewriteStyleAttributesWithUrl);
  options()->set_image_jpeg_recompress_quality(85);
  rewrite_driver()->SetUserAgent("webp");
  rewrite_driver()->AddFilters();
  ValidateExpected("webp",
      "<div style=\"background-image:url(foo.jpg)\"/>",
      "<div style=\"background-image:url(xfoo.jpg.pagespeed.ic.0.webp)\"/>");
}

TEST_F(CssRecompressImagesInStyleAttributes,
       RecompressAndWebpLosslessAndStyleEnabled) {
  if (RunningOnValgrind()) {  // Too slow under vg.
    return;
  }

  AddFileToMockFetcher(StrCat(kTestDomain, "foo.jpg"), kPuzzleJpgFile,
                       kContentTypeJpeg, 100);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kRewriteStyleAttributesWithUrl);
  options()->set_image_jpeg_recompress_quality(85);
  rewrite_driver()->SetUserAgent("webp-la");
  rewrite_driver()->AddFilters();
  ValidateExpected("webp-lossless",
      "<div style=\"background-image:url(foo.jpg)\"/>",
      "<div style=\"background-image:url(xfoo.jpg.pagespeed.ic.0.webp)\"/>");
}

TEST_F(CssRecompressImagesInStyleAttributes,
       RecompressAndWebpAndStyleEnabledWithMaxCssSize) {
  AddFileToMockFetcher(StrCat(kTestDomain, "foo.jpg"), kPuzzleJpgFile,
                       kContentTypeJpeg, 100);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kRewriteStyleAttributesWithUrl);
  options()->set_image_jpeg_recompress_quality(85);
  options()->set_max_image_bytes_for_webp_in_css(1);
  rewrite_driver()->SetUserAgent("webp");
  rewrite_driver()->AddFilters();
  ValidateExpected("webp",
      "<div style=\"background-image:url(foo.jpg)\"/>",
      "<div style=\"background-image:url(xfoo.jpg.pagespeed.ic.0.jpg)\"/>");
}

TEST_F(CssRecompressImagesInStyleAttributes,
       RecompressAndWebpLosslessAndStyleEnabledWithMaxCssSize) {
  AddFileToMockFetcher(StrCat(kTestDomain, "foo.jpg"), kPuzzleJpgFile,
                       kContentTypeJpeg, 100);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kRewriteStyleAttributesWithUrl);
  options()->set_image_jpeg_recompress_quality(85);
  options()->set_max_image_bytes_for_webp_in_css(1);
  rewrite_driver()->SetUserAgent("webp-la");
  rewrite_driver()->AddFilters();
  ValidateExpected("webp-lossless",
      "<div style=\"background-image:url(foo.jpg)\"/>",
      "<div style=\"background-image:url(xfoo.jpg.pagespeed.ic.0.jpg)\"/>");
}

}  // namespace

}  // namespace net_instaweb
