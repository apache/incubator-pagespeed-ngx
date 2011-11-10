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

#include "base/scoped_ptr.h"

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/css_filter.h"
#include "net/instaweb/rewriter/public/css_rewrite_test_base.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/test_url_namer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

// Filenames of resource files.
const char kBikePngFile[] = "BikeCrashIcn.png";
const char kCuppaPngFile[] = "Cuppa.png";
const char kPuzzleJpgFile[] = "Puzzle.jpg";

const char kImageData[] = "Invalid PNG but it does not matter for this test";

class CssImageRewriterTest : public CssRewriteTestBase {
 protected:
  virtual void SetUp() {
    // We setup the options before the upcall so that the
    // CSS filter is created aware of these.
    options()->EnableFilter(RewriteOptions::kExtendCache);
    CssRewriteTestBase::SetUp();
  }
};

TEST_P(CssImageRewriterTest, CacheExtendsImagesSimple) {
  // Simplified version of CacheExtendsImages, which doesn't have many copies of
  // the same URL.
  InitResponseHeaders("foo.png", kContentTypePng, kImageData, 100);

  static const char css_before[] =
      "body {\n"
      "  background-image: url(foo.png);\n"
      "}\n";
  const GoogleString css_after =
      StrCat("body{background-image:url(",
             Encode(kTestDomain, "ce", "0", "foo.png", "png"),
             ")}");

  ValidateRewriteInlineCss("cache_extends_images-inline",
                           css_before, css_after,
                           kExpectChange | kExpectSuccess);
  ValidateRewriteExternalCss("cache_extends_images-external",
                             css_before, css_after,
                             kExpectChange | kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);
}

TEST_P(CssImageRewriterTest, CacheExtendsImagesEmbeddedComma) {
  // Makes sure image-URL rewriting doesn't corrupt URLs with embedded
  // commas.  Earlier, we were escaping commas in URLs by backslashing
  // the "," and IE8 interprets those backslashes as forward slashes,
  // making the URL incorrect.
  static const char kImageUrl[] = "foo,bar.png";
  InitResponseHeaders(kImageUrl, kContentTypePng, kImageData, 100);

  static const char css_before[] =
      "body {\n"
      "  background-image: url(foo,bar.png);\n"
      "}\n";
  const GoogleString css_after =
      StrCat("body{background-image:url(",
             Encode(kTestDomain, "ce", "0", kImageUrl, "png"),
             ")}");

  ValidateRewriteInlineCss("cache_extends_images-inline",
                           css_before, css_after,
                           kExpectChange | kExpectSuccess);
  ValidateRewriteExternalCss("cache_extends_images-external",
                             css_before, css_after,
                             kExpectChange | kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);
}

TEST_P(CssImageRewriterTest, CacheExtendsImagesEmbeddedSpace) {
  // Note that GoogleUrl will, internal to our system, convert the space to
  // a %20, so we'll be fetching the percentified form.
  InitResponseHeaders("foo%20bar.png", kContentTypePng, kImageData, 100);

  static const char css_before[] =
      "body {\n"
      "  background-image: url('foo bar.png');\n"
      "}\n";
  const GoogleString css_after =
      StrCat("body{background-image:url(",
             Encode(kTestDomain, "ce", "0", "foo%20bar.png", "png"),
             ")}");

  ValidateRewriteInlineCss("cache_extends_images-inline",
                           css_before, css_after,
                           kExpectChange | kExpectSuccess);
  ValidateRewriteExternalCss("cache_extends_images-external",
                             css_before, css_after,
                             kExpectChange | kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);
}

TEST_P(CssImageRewriterTest, MinifyImagesEmbeddedSpace) {
  options()->ClearSignatureForTesting();
  options()->DisableFilter(RewriteOptions::kExtendCache);
  resource_manager()->ComputeSignature(options());

  static const char css_before[] =
      "body {\n"
      "  background-image: url('foo bar.png');\n"
      "}\n";
  static const char css_after[] =
      "body{background-image:url(foo bar.png)}";

  ValidateRewriteInlineCss("cache_extends_images-inline",
                           css_before, css_after,
                           kExpectChange | kExpectSuccess);
  ValidateRewriteExternalCss("cache_extends_images-external",
                             css_before, css_after,
                             kExpectChange | kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);
}

TEST_P(CssImageRewriterTest, CacheExtendsWhenCssGrows) {
  // We run most tests with set_always_rewrite_css(true) which bypasses
  // checks on whether rewriting is worthwhile or not. Test to make sure we make
  // the right decision when we do do the check in the case where the produced
  // CSS is actually larger, but contains rewritten resources.
  // (We want to rewrite the CSS in that case)
  options()->ClearSignatureForTesting();
  options()->set_always_rewrite_css(false);
  resource_manager()->ComputeSignature(options());
  InitResponseHeaders("foo.png", kContentTypePng, kImageData, 100);
  static const char css_before[] =
      "body{background-image: url(foo.png)}";
  const GoogleString css_after =
      StrCat("body{background-image:url(",
             Encode(kTestDomain, "ce", "0", "foo.png", "png)"),
             "}");

  ValidateRewriteInlineCss("cache_extends_images_growcheck-inline",
                           css_before, css_after,
                           kExpectChange | kExpectSuccess);
  ValidateRewriteExternalCss("cache_extends_images_growcheck-external",
                             css_before, css_after,
                             kExpectChange | kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);
}

TEST_P(CssImageRewriterTest, CacheExtendsRepeatedTopLevel) {
  // Test to make sure that if we cache extend inside CSS we can do it
  // for the same image in HTML at the same time.
  const char kImg[] = "img.png";
  const GoogleString kExtendedImg =
      Encode(kTestDomain, "ce", "0", "img.png", "png");

  const char kCss[] = "stylesheet.css";
  const GoogleString kRewrittenCss =
      Encode(kTestDomain, "cf", "0", "stylesheet.css", "css");
  const char kCssTemplate[] = "body{background-image:url(%s)}";

  InitResponseHeaders(kImg, kContentTypePng, kImageData, 100);
  InitResponseHeaders(
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
  EXPECT_TRUE(ServeResourceUrl(kRewrittenCss, &css_out));
  EXPECT_EQ(StringPrintf(kCssTemplate, kExtendedImg.c_str()), css_out);
}

TEST_P(CssImageRewriterTest, CacheExtendsImages) {
  InitResponseHeaders("foo.png", kContentTypePng, kImageData, 100);
  InitResponseHeaders("bar.png", kContentTypePng, kImageData, 100);
  InitResponseHeaders("baz.png", kContentTypePng, kImageData, 100);

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
      "body{background-image:url(",
      Encode(kTestDomain, "ce", "0", "foo.png", "png"),
      ");"
      "list-style-image:url(",
      Encode(kTestDomain, "ce", "0", "bar.png", "png"),
      ")}"
      ".titlebar p.cfoo,#end p{"
      "background:url(",
      Encode(kTestDomain, "ce", "0", "baz.png", "png"),
      ");"
      "list-style:url(",
      Encode(kTestDomain, "ce", "0", "foo.png", "png")),
      ")}"
      ".other{"  // data: URLs and unknown properties are not rewritten.
      "background-image:url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAA"
      "AUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4"
      "OHwAAAABJRU5ErkJggg==);"
      "-proprietary-background-property:url(foo.png)}");

  // Can't serve from new contexts yet, because we're using mock_fetcher_.
  // TODO(sligocki): Resolve that and the just have:
  // ValidateRewriteInlineCss("cache_extends_images", css_before, css_after);
  ValidateRewriteInlineCss("cache_extends_images-inline",
                           css_before, css_after,
                           kExpectChange | kExpectSuccess);
  ValidateRewriteExternalCss("cache_extends_images-external",
                             css_before, css_after,
                             kExpectChange | kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);
}

// See TrimsImageUrls below: change one, change them both!
TEST_P(CssImageRewriterTest, TrimsImageUrls) {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kLeftTrimUrls);
  resource_manager()->ComputeSignature(options());
  InitResponseHeaders("foo.png", kContentTypePng, kImageData, 100);
  static const char kCss[] =
      "body {\n"
      "  background-image: url(foo.png);\n"
      "}\n";

  const GoogleString kCssAfter = StrCat(
      "body{background-image:url(",
      Encode("", "ce", "0", "foo.png", "png"),
      ")}");

  ValidateRewriteExternalCss("trims_css_urls", kCss, kCssAfter,
                              kExpectChange | kExpectSuccess |
                              kNoOtherContexts | kNoClearFetcher);
}

class CssImageRewriterTestUrlNamer : public CssImageRewriterTest {
 public:
  CssImageRewriterTestUrlNamer() {
    SetUseTestUrlNamer(true);
  }
};

// See TrimsImageUrls above: change one, change them both!
TEST_P(CssImageRewriterTestUrlNamer, TrimsImageUrls) {
  // Check that we really are using TestUrlNamer and not UrlNamer.
  EXPECT_NE(Encode(kTestDomain, "ce", "0", "foo.png", "png"),
            EncodeNormal(kTestDomain, "ce", "0", "foo.png", "png"));

  // A verbatim copy of the test above but using TestUrlNamer.
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kLeftTrimUrls);
  resource_manager()->ComputeSignature(options());
  InitResponseHeaders("foo.png", kContentTypePng, kImageData, 100);
  static const char kCss[] =
      "body {\n"
      "  background-image: url(foo.png);\n"
      "}\n";

  const GoogleString kCssAfter = StrCat(
      "body{background-image:url(",
      Encode("", "ce", "0", "foo.png", "png"),
      ")}");

  ValidateRewriteExternalCss("trims_css_urls", kCss, kCssAfter,
                              kExpectChange | kExpectSuccess |
                              kNoOtherContexts | kNoClearFetcher);
}

TEST_P(CssImageRewriterTest, InlinePaths) {
  // Make sure we properly handle CSS relative references when we have the same
  // inline CSS in different places. This is also a regression test for a bug
  // during development of async + inline case which caused us to do
  // null rewrites from cache.
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kLeftTrimUrls);
  resource_manager()->ComputeSignature(options());
  InitResponseHeaders("dir/foo.png", kContentTypePng, kImageData, 100);

  static const char kCssBefore[] =
      "body {\n"
      "  background-image: url(http://test.com/dir/foo.png);\n"
      "}\n";

  // Force all URL encoding to use normal encoding so that the relative URL
  // trimming logic can work and give us a relative URL result as expected.
  TestUrlNamer::UseNormalEncoding(true);

  const GoogleString kCssAfter = StrCat(
      "body{background-image:url(",
      Encode("dir/", "ce", "0", "foo.png", "png"),
      ")}");
  ValidateRewriteInlineCss("nosubdir",
                           kCssBefore, kCssAfter,
                           kExpectChange | kExpectSuccess);

  const GoogleString kCssAfterRel = StrCat(
      "body{background-image:url(",
      Encode("", "ce", "0", "foo.png", "png"),
      ")}");
  ValidateRewriteInlineCss("dir/yessubdir",
                           kCssBefore, kCssAfterRel,
                           kExpectChange | kExpectSuccess);
}

TEST_P(CssImageRewriterTest, RewriteCached) {
  // Make sure we produce the same output from cache.
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kLeftTrimUrls);
  resource_manager()->ComputeSignature(options());
  InitResponseHeaders("dir/foo.png", kContentTypePng, kImageData, 100);

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
                           kExpectChange | kExpectSuccess);

  statistics()->Clear();
  ValidateRewriteInlineCss("nosubdir2",
                           kCssBefore, kCssAfter,
                           kExpectChange | kExpectSuccess | kNoStatCheck);
  // Should not re-serialize. Works only under the new flow...
  if (rewrite_driver()->asynchronous_rewrites()) {
    EXPECT_EQ(
        0, statistics()->GetVariable(CssFilter::kMinifiedBytesSaved)->Get());
  }
}

TEST_P(CssImageRewriterTest, CacheInlineParseFailures) {
  const char kInvalidCss[] = " div{";

  Variable* num_parse_failures =
      statistics()->GetVariable(CssFilter::kParseFailures);

  ValidateRewriteInlineCss("inline-invalid", kInvalidCss, kInvalidCss,
                           kExpectNoChange | kExpectFailure | kNoOtherContexts);
  EXPECT_EQ(1, num_parse_failures->Get());

  // This works properly only under new flow...
  if (rewrite_driver()->asynchronous_rewrites()) {
    ValidateRewriteInlineCss(
        "inline-invalid2", kInvalidCss, kInvalidCss,
        kExpectNoChange | kExpectFailure | kNoOtherContexts | kNoStatCheck);
    // Shouldn't reparse -- and stats are reset between runs.
    EXPECT_EQ(0, num_parse_failures->Get());
  }
}

TEST_P(CssImageRewriterTest, RecompressImages) {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRecompressImages);
  resource_manager()->ComputeSignature(options());
  AddFileToMockFetcher(StrCat(kTestDomain, "foo.png"), kBikePngFile,
                        kContentTypePng, 100);
  static const char kCss[] =
      "body {\n"
      "  background-image: url(foo.png);\n"
      "}\n";

  const GoogleString kCssAfter = StrCat(
      "body{background-image:url(",
      Encode(kTestDomain, "ic", "0", "foo.png", "png"),
      ")}");

  ValidateRewriteExternalCss("recompress_css_images", kCss, kCssAfter,
                              kExpectChange | kExpectSuccess |
                              kNoOtherContexts | kNoClearFetcher);
}

TEST_P(CssImageRewriterTest, InlineImages) {
  // Make sure we can inline images in any kind of CSS.
  CSS_XFAIL_SYNC();
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kInlineImagesInCss);
  options()->set_image_inline_max_bytes(2000);
  options()->set_css_image_inline_max_bytes(2000);
  EXPECT_EQ(2000, options()->ImageInlineMaxBytes());
  EXPECT_EQ(2000, options()->CssImageInlineMaxBytes());
  resource_manager()->ComputeSignature(options());
  // Here Cuppa.png is 1763 bytes, so should be inlined.
  AddFileToMockFetcher(StrCat(kTestDomain, "Cuppa.png"), kCuppaPngFile,
                       kContentTypePng, 100);
  static const char kCss[] =
      "body {\n"
      "  background-image: url(Cuppa.png);\n"
      "}\n";

  // Read original image file and create data url for comparison purposes.
  GoogleString contents;
  StdioFileSystem stdio_file_system;
  GoogleString filename = StrCat(GTestSrcDir(), kTestData, kCuppaPngFile);
  ASSERT_TRUE(stdio_file_system.ReadFile(
      filename.c_str(), &contents, message_handler()));
  GoogleString data_url;
  DataUrl(kContentTypePng, BASE64, contents, &data_url);

  GoogleString kCssAfter = StrCat("body{background-image:url(", data_url, ")}");

  // Here we skip the stat check because we are *increasing* the size of the CSS
  // (which causes the check to fail).  That eliminates a resource fetch, so it
  // should normally be a net win in practice.
  ValidateRewrite("inline_css_images", kCss, kCssAfter,
                  kExpectChange | kExpectSuccess |
                  kNoClearFetcher | kNoStatCheck);
}

TEST_P(CssImageRewriterTest, InlineImageOnlyInOutlineCss) {
  // Make sure that we use image_inline_max_bytes to determine image inlining in
  // inline css (css that occurs in an html file), but that we use
  // css_image_inline_max_bytes for standalone css.
  CSS_XFAIL_SYNC();
  options()->ClearSignatureForTesting();
  // Do inline in CSS file but not in inline CSS.
  options()->EnableFilter(RewriteOptions::kInlineImagesInCss);
  options()->set_image_inline_max_bytes(2000);
  options()->set_css_image_inline_max_bytes(2000);
  EXPECT_EQ(0, options()->ImageInlineMaxBytes());  // This is disabled...
  ASSERT_EQ(2000, options()->CssImageInlineMaxBytes());  // But this is enabled.
  resource_manager()->ComputeSignature(options());
  // Here Cuppa.png is 1763 bytes, so should be inlined.
  AddFileToMockFetcher(StrCat(kTestDomain, "Cuppa.png"), kCuppaPngFile,
                       kContentTypePng, 100);
  static const char kCss[] =
      "body {\n"
      "  background-image: url(Cuppa.png);\n"
      "}\n";

  // Read original image file and create data url for comparison purposes.
  GoogleString contents;
  StdioFileSystem stdio_file_system;
  GoogleString filename = StrCat(GTestSrcDir(), kTestData, kCuppaPngFile);
  ASSERT_TRUE(stdio_file_system.ReadFile(
      filename.c_str(), &contents, message_handler()));
  GoogleString data_url;
  DataUrl(kContentTypePng, BASE64, contents, &data_url);

  GoogleString kCssInlineAfter =
      StrCat("body{background-image:url(",
             Encode(kTestDomain, "ce", "0", "Cuppa.png", "png"),
             ")}");
  GoogleString kCssExternalAfter =
      StrCat("body{background-image:url(", data_url, ")}");

  ValidateRewriteInlineCss(
      "no_inline_in_inline", kCss, kCssInlineAfter,
      kExpectChange | kExpectSuccess | kNoClearFetcher);
  // Again skip the stat check because we are *increasing* the size of the CSS
  ValidateRewriteExternalCss(
      "inline_in_outline", kCss, kCssExternalAfter,
      kExpectChange | kExpectSuccess | kNoClearFetcher | kNoStatCheck);
}

TEST_P(CssImageRewriterTest, UseCorrectBaseUrl) {
  // Initialize resources.
  static const char css_url[] = "http://www.example.com/bar/style.css";
  static const char css_before[] = "body { background: url(image.png); }";
  InitResponseHeaders(css_url, kContentTypeCss, css_before, 100);
  static const char image_url[] = "http://www.example.com/bar/image.png";
  InitResponseHeaders(image_url, kContentTypePng, kImageData, 100);

  // Construct URL for rewritten image.
  GoogleString expected_image_url = ExpectedRewrittenUrl(
      image_url, kImageData, RewriteDriver::kCacheExtenderId,
      kContentTypePng);

  GoogleString css_after = StrCat(
      "body{background:url(", expected_image_url, ")}");

  // Construct URL for rewritten CSS.
  GoogleString expected_css_url = ExpectedRewrittenUrl(
      css_url, css_after, RewriteDriver::kCssFilterId, kContentTypeCss);

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
  ServeResourceUrl(expected_css_url, &actual_css_after);
  EXPECT_EQ(css_after, actual_css_after);
}

TEST_P(CssImageRewriterTest, CacheExtendsImagesInStyleAttributes) {
  InitResponseHeaders("foo.png", kContentTypePng, kImageData, 100);
  InitResponseHeaders("bar.png", kContentTypePng, kImageData, 100);
  InitResponseHeaders("baz.png", kContentTypePng, kImageData, 100);

  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRewriteStyleAttributes);
  resource_manager()->ComputeSignature(options());

  ValidateExpected("cache_extend_images_simple",
                   "<div style=\""
                   "  background-image: url(foo.png);\n"
                   "  list-style-image: url('bar.png');\n"
                   "\"/>",
                   StrCat(
                   "<div style=\""
                   "background-image:"
                   "url(",
                   Encode(kTestDomain, "ce", "0", "foo.png", "png"),
                   ");",
                   "list-style-image:"
                   "url(",
                   Encode(kTestDomain, "ce", "0", "bar.png", "png"),
                   ")",
                   "\"/>"));

  ValidateExpected("cache_extend_images",
                   "<div style=\""
                   "  background: url(baz.png);\n"
                   "  list-style: url(&quot;foo.png&quot;);\n"
                   "\"/>",
                   StrCat(
                   "<div style=\""
                   "background:url(",
                   Encode(kTestDomain, "ce", "0", "baz.png", "png"),
                   ");"
                   "list-style:url(",
                   Encode(kTestDomain, "ce", "0", "foo.png", "png"),
                   ")"
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

TEST_P(CssImageRewriterTest, RecompressImagesInStyleAttributes) {
  static const char div_before[] =
      "<div style=\""
      "background-image:url(foo.png)"
      "\"/>";
  const GoogleString div_after = StrCat(
      "<div style=\""
      "background-image:url(",
      Encode(kTestDomain, "ic", "0", "foo.png", "png"),
      ")"
      "\"/>");

  scoped_ptr<RewriteOptions> default_options(factory()->NewRewriteOptions());
  default_options.get()->DisableFilter(RewriteOptions::kExtendCache);
  AddFileToMockFetcher(StrCat(kTestDomain, "foo.png"), kBikePngFile,
                       kContentTypePng, 100);

  // No rewriting if neither option is enabled.
  ValidateNoChanges("options_disabled", div_before);

  // No rewriting if only one option is enabled.
  options()->CopyFrom(*default_options.get());
  options()->EnableFilter(RewriteOptions::kRewriteStyleAttributesWithUrl);
  resource_manager()->ComputeSignature(options());
  ValidateNoChanges("recompress_images_disabled", div_before);

  // No rewriting if only one option is enabled.
  options()->CopyFrom(*default_options.get());
  options()->EnableFilter(RewriteOptions::kRecompressImages);
  resource_manager()->ComputeSignature(options());
  ValidateNoChanges("rewrite_style_attrs_disabled", div_before);

  // Rewrite iff both options are enabled.
  options()->CopyFrom(*default_options.get());
  options()->EnableFilter(RewriteOptions::kRewriteStyleAttributesWithUrl);
  options()->EnableFilter(RewriteOptions::kRecompressImages);
  resource_manager()->ComputeSignature(options());
  ValidateExpected("options_enabled", div_before, div_after);
}

// We test with asynchronous_rewrites() == GetParam() as both true and false.
INSTANTIATE_TEST_CASE_P(CssImageRewriterTestInstance,
                        CssImageRewriterTest,
                        ::testing::Bool());

INSTANTIATE_TEST_CASE_P(CssImageRewriterTestUrlNamerInstance,
                        CssImageRewriterTestUrlNamer,
                        ::testing::Bool());

// Note that these values of "10" and "20" are very tight.  This is a
// feature.  It serves as an early warning system because extra cache
// lookups will induce time-advancement from
// MemFileSystem::UpdateAtime, which can make these resources expire
// before they are used.  So if you find tests in this module failing
// unexpectedly, you may be tempted to bump up these values.  Don't.
// Figure out how to make fewer cache lookups.
static const int kMinExpirationTimeMs = 10 * Timer::kSecondMs;
static const int kExpireAPngSec = 10;
static const int kExpireBPngSec = 20;

// These tests are to make sure our TTL considers that of subresources.
class CssFilterSubresourceTest : public CssRewriteTestBase {
 public:
  virtual void SetUp() {
    // We setup the options before the upcall so that the
    // CSS filter is created aware of these.
    options()->EnableFilter(RewriteOptions::kExtendCache);
    options()->EnableFilter(RewriteOptions::kRecompressImages);
    CssRewriteTestBase::SetUp();

    // As we use invalid payloads, we expect image rewriting to
    // fail but cache extension to succeed.
    InitResponseHeaders("a.png", kContentTypePng, "notapng", kExpireAPngSec);
    InitResponseHeaders("b.png", kContentTypePng, "notbpng", kExpireBPngSec);
  }

  void ValidateExpirationTime(const char* id, const char* output,
                              int64 expected_expire_ms) {
    GoogleString css_url = ExpectedUrlForCss(id, output);

    // See what cache information we have
    bool use_async_flow = false;
    OutputResourcePtr output_resource(
        rewrite_driver()->CreateOutputResourceWithPath(
            kTestDomain, RewriteDriver::kCssFilterId, StrCat(id, ".css"),
            &kContentTypeCss, kRewrittenResource, use_async_flow));
    ASSERT_TRUE(output_resource.get() != NULL);
    // output_resource's hash will not be set unless its cached_result could
    // be loaded.
    ASSERT_TRUE(output_resource->cached_result() != NULL);
    EXPECT_EQ(css_url, output_resource->url());

    EXPECT_EQ(expected_expire_ms,
              output_resource->cached_result()->origin_expiration_time_ms());
  }

  GoogleString ExpectedUrlForPng(const StringPiece& name,
                                 const GoogleString& expected_output) {
    return Encode(kTestDomain, RewriteDriver::kCacheExtenderId,
                  hasher()->Hash(expected_output),
                  name, "png");
  }
};

// Test to make sure expiration time for cached result is the
// smallest of subresource and CSS times, not just CSS time.
TEST_P(CssFilterSubresourceTest, SubResourceDepends) {
  // These tests rely on the guts of the old expiration machinery.
  CSS_XFAIL_ASYNC();

  const char kInput[] = "div { background-image: url(a.png); }"
                        "span { background-image: url(b.png); }";

  // Figure out where cache-extended PNGs will go.
  GoogleString image_url1 = ExpectedUrlForPng("a.png", "notapng");
  GoogleString image_url2 = ExpectedUrlForPng("b.png", "notbpng");
  GoogleString output = StrCat("div{background-image:url(", image_url1, ")}",
                               "span{background-image:url(", image_url2, ")}");

  // Here we don't use the other contexts since it has different
  // synchronicity, and we presently do best-effort for loaded subresources
  // even in Fetch.
  ValidateRewriteExternalCss(
      "ext", kInput, output, kNoOtherContexts | kNoClearFetcher |
                             kExpectChange | kExpectSuccess);

  // 10 is the smaller of expiration times of a.png, b.png and ext.css
  ValidateExpirationTime("ext", output.c_str(),
                         start_time_ms() + kMinExpirationTimeMs);
}

// Test to make sure we don't cache for long if the rewrite was based
// on not-yet-loaded resources.
TEST_P(CssFilterSubresourceTest, SubResourceDependsNotYetLoaded) {
  // These tests rely on the guts of the old expiration machinery.
  CSS_XFAIL_ASYNC();

  SetupWaitFetcher();

  // Disable atime simulation so that the clock doesn't move on us.
  file_system()->set_atime_enabled(false);

  const char kInput[] = "div { background-image: url(a.png); }"
                        "span { background-image: url(b.png); }";
  const char kOutput[] = "div{background-image:url(http://test.com/a.png)}"
                         "span{background-image:url(http://test.com/b.png)}";

  // At first try, not even the CSS gets loaded, so nothing gets
  // changed at all.
  ValidateRewriteExternalCss(
      "wip", kInput, kInput, kNoOtherContexts | kNoClearFetcher |
                             kExpectNoChange | kExpectSuccess);

  // Get the CSS to load (resources are still unavailable).
  CallFetcherCallbacks();
  ValidateRewriteExternalCss(
      "wip", kInput, kOutput, kNoOtherContexts | kNoClearFetcher |
                              kExpectChange | kExpectSuccess);

  // Since resources haven't loaded, the output cache should have a very small
  // expiration time.
  ValidateExpirationTime("wip", kOutput, start_time_ms() + Timer::kSecondMs);

  // Make sure the subresource callbacks fire for leak cleanliness
  CallFetcherCallbacks();
}

// We test with asynchronous_rewrites() == GetParam() as both true and false.
INSTANTIATE_TEST_CASE_P(CssFilterSubresourceTestInstance,
                        CssFilterSubresourceTest,
                        ::testing::Bool());


}  // namespace

}  // namespace net_instaweb
