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

// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/rewriter/public/image_rewrite_filter.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/log_record_test_helper.h"
#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/wait_url_async_fetcher.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/image_testing_peer.h"
#include "net/instaweb/rewriter/public/dom_stats_filter.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/image.h"
#include "net/instaweb/rewriter/public/mock_critical_images_finder.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/rewriter/rendered_image.pb.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/controller/work_bound_expensive_operation_controller.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/dynamic_annotations.h"  // RunningOnValgrind
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/md5_hasher.h"  // for MD5Hasher
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_thread_system.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"  // for Timer, etc
#include "pagespeed/kernel/cache/lru_cache.h"
#include "pagespeed/kernel/html/empty_html_filter.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_parse.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/http_options.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/semantic_type.h"
#include "pagespeed/kernel/http/user_agent_matcher_test_base.h"
#include "pagespeed/kernel/image/test_utils.h"
#include "pagespeed/opt/logging/enums.pb.h"

namespace net_instaweb {

using net_instaweb::ImageRewriteFilter;
using pagespeed::image_compression::kMessagePatternPixelFormat;
using pagespeed::image_compression::kMessagePatternStats;
using pagespeed::image_compression::kMessagePatternWritingToWebp;
using ::testing::HasSubstr;

namespace {

// Filenames of resource files.
const char kAnimationGifFile[] = "PageSpeedAnimationSmall.gif";
const char kBikePngFile[] = "BikeCrashIcn.png";  // photo; no alpha
const char kChromium24[] = "chromium-24.webp";
const char kChefGifFile[] = "IronChef2.gif";     // photo; no alpha
const char kCradleAnimation[] = "CradleAnimation.gif";
const char kCuppaPngFile[] = "Cuppa.png";        // graphic; no alpha
const char kCuppaOPngFile[] = "CuppaO.png";      // graphic; no alpha; no opt
const char kCuppaTPngFile[] = "CuppaT.png";      // graphic; alpha; no opt
const char kEmptyScreenGifFile[] = "red_empty_screen.gif";  // Empty screen
const char kLargePngFile[] = "Large.png";        // blank image; gray scale
const char kPuzzleJpgFile[] = "Puzzle.jpg";      // photo; no alpha
const char kPuzzleUrl[] = "http://rewrite_image.test/Puzzle.jpg";
const char kRedbrushAlphaPngFile[] = "RedbrushAlpha-0.5.png";  // photo; alpha
const char kSmallDataFile[] = "small-data.png";   // not an image
const char k1x1GifFile[] = "o.gif";               // unoptimizable gif
const char kResolutionLimitPngFile[] = "ResolutionLimit.png";
const char kResolutionLimitJpegFile[] = "ResolutionLimit.jpg";

// Both ResolutionLimit.png and ResolutionLimit.jpg have 4096 x 2048 pixels.
// We assume that each pixel has 4 bytes when we check whether the images are
// within the limit, so
//   width * height * pixel_depth = 4096 x 2048 x 4 = 33554432 =
//       kResolutionLimitBytes.
// 33554432 is also the default resolution limit (in bytes) in mod_pagespeed.
const int kResolutionLimitBytes = 33554432;

const char kChefDims[] = " width=\"192\" height=\"256\"";

// Size of a 1x1 image.
const char kPixelDims[] = " width='1' height='1'";

// If the expected value of a size is set to -1, this size will be ignored in
// the test.
const int kIgnoreSize = -1;

const char kCriticalImagesCohort[] = "critical_images";

// Message to ignore.
const char kMessagePatternFailedToEncodeWebp[] = "*Could not encode webp data*";
const char kMessagePatternRecompressing[] = "*Recompressing image*";
const char kMessagePatternResizedImage[] = "*Resized image*";
const char kMessagePatternShrinkingImage[] = "*Shrinking image*";
const char kMessagePatternWebpTimeOut[] = "*WebP conversion timed out*";

struct OptimizedImageInfo {
  const ContentType* content_type;
  const char* vary_header;
  int content_length;
};

struct OptimizedImageInfoList {
  const struct OptimizedImageInfo with_via;
  const struct OptimizedImageInfo with_none;
  const struct OptimizedImageInfo with_savedata_via;
  const struct OptimizedImageInfo with_savedata;
};

struct OptimizedImageInfoListInputs {
  const char* user_agent;
  const char* image_name;
  const OptimizedImageInfoList* optimized_info;
};

const OptimizedImageInfoList kPuzzleOptimizedForWebpUa = {
  // [Save-Data: no, Via: yes]: Convert to WebP desktop quality.
  {&kContentTypeWebp, "Accept,Save-Data", 33108},
  // [Save-Data: no, Via: no]: Convert to WebP mobile quality.
  {&kContentTypeWebp, "User-Agent,Save-Data", 25774},
  // [Save-Data: yes, Via: yes]: Convert to WebP Save-Data quality.
  {&kContentTypeWebp, "Accept,Save-Data", 19124},
  // [Save-Data: yes, Via: no]: Convert to WebP Save-Data quality.
  {&kContentTypeWebp, "User-Agent,Save-Data", 19124},
};

const OptimizedImageInfoList kPuzzleOptimizedForSafariUa = {
  // [Save-Data: no, Via: yes]: Convert to JPEG desktop quality.
  {&kContentTypeJpeg, "Accept,Save-Data", 73096},
  // [Save-Data: no, Via: no]: Convert to JPEG mobile quality.
  {&kContentTypeJpeg, "User-Agent,Save-Data", 51452},
  // [Save-Data: yes, Via: yes]: Convert to JPEG Save-Data quality.
  {&kContentTypeJpeg, "Accept,Save-Data", 38944},
  // [Save-Data: yes, Via: no]: Convert to JPEG Save-Data quality.
  {&kContentTypeJpeg, "User-Agent,Save-Data", 38944},
};

const OptimizedImageInfoList kPuzzleOptimizedForDesktopUa = {
  // [Save-Data: no, Via: yes]: Convert to JPEG desktop quality.
  {&kContentTypeJpeg, "Accept,Save-Data", 73096},
  // [Save-Data: no, Via: no]: Convert to JPEG desktop quality.
  {&kContentTypeJpeg, "User-Agent,Save-Data", 73096},
  // [Save-Data: yes, Via: yes]: Convert to JPEG Save-Data quality.
  {&kContentTypeJpeg, "Accept,Save-Data", 38944},
  // [Save-Data: yes, Via: no]: Convert to JPEG Save-Data quality.
  {&kContentTypeJpeg, "User-Agent,Save-Data", 38944},
};

const OptimizedImageInfoList kBikeOptimizedForWebpUa = {
  // [Save-Data: no, Via: yes]: Convert to WebP desktop quality.
  {&kContentTypeWebp, "Accept,Save-Data", 2454},
  // [Save-Data: no, Via: no]: Convert to WebP mobile quality.
  {&kContentTypeWebp, "User-Agent,Save-Data", 2014},
  // [Save-Data: yes, Via: yes]: Convert to WebP Save-Data quality.
  {&kContentTypeWebp, "Accept,Save-Data", 1476},
  // [Save-Data: yes, Via: no]: Convert to WebP Save-Data quality.
  {&kContentTypeWebp, "User-Agent,Save-Data", 1476},
};

const OptimizedImageInfoList kBikeOptimizedForSafariUa = {
  // [Save-Data: no, Via: yes]: Convert to JPEG desktop quality.
  {&kContentTypeJpeg, "Accept,Save-Data", 3536},
  // [Save-Data: no, Via: no]: Convert to JPEG mobile quality.
  {&kContentTypeJpeg, "User-Agent,Save-Data", 2606},
  // [Save-Data: yes, Via: yes]: Convert to JPEG Save-Data quality.
  {&kContentTypeJpeg, "Accept,Save-Data", 2069},
  // [Save-Data: yes, Via: no]: Convert to JPEG Save-Data quality.
  {&kContentTypeJpeg, "User-Agent,Save-Data", 2069},
};

const OptimizedImageInfoList kBikeOptimizedForDesktopUa = {
  // [Save-Data: no, Via: yes]: Convert to JPEG desktop quality.
  {&kContentTypeJpeg, "Accept,Save-Data", 3536},
  // [Save-Data: no, Via: no]: Convert to JPEG desktop quality.
  {&kContentTypeJpeg, "User-Agent,Save-Data", 3536},
  // [Save-Data: yes, Via: yes]: Convert to JPEG Save-Data quality.
  {&kContentTypeJpeg, "Accept,Save-Data", 2069},
  // [Save-Data: yes, Via: no]: Convert to JPEG Save-Data quality.
  {&kContentTypeJpeg, "User-Agent,Save-Data", 2069},
};

const OptimizedImageInfoList kCuppaOptimizedForWebpUa = {
  // [Save-Data: no, Via: yes]: Convert to PNG.
  {&kContentTypePng, nullptr, 770},
  // [Save-Data: no, Via: no]: Convert to WebP lossless.
  {&kContentTypeWebp, "User-Agent", 694},
  // [Save-Data: yes, Via: yes]: Convert to PNG.
  {&kContentTypePng, nullptr, 770},
  // [Save-Data: yes, Via: no]: Convert to WebP lossless.
  {&kContentTypeWebp, "User-Agent", 694},
};

const OptimizedImageInfoList kCuppaOptimizedForDesktopUa = {
  // [Save-Data: no, Via: yes]: Convert to PNG.
  {&kContentTypePng, nullptr, 770},
  // [Save-Data: no, Via: no]: Convert to PNG.
  {&kContentTypePng, "User-Agent", 770},
  // [Save-Data: yes, Via: yes]: Convert to PNG.
  {&kContentTypePng, nullptr, 770},
  // [Save-Data: yes, Via: no]: Convert to PNG.
  {&kContentTypePng, "User-Agent", 770},
};

const OptimizedImageInfoList kAnimationOptimizedForWebpUa = {
  // [Save-Data: no, Via: yes]: Cannot optimize.
  {&kContentTypeGif, nullptr, 26251},
  // [Save-Data: no, Via: no]: Convert to WebP desktop/mobile quality.
  {&kContentTypeWebp, "User-Agent,Save-Data", 7232},
  // [Save-Data: yes, Via: yes]: Cannot optimize.
  {&kContentTypeGif, nullptr, 26251},
  // [Save-Data: yes, Via: no]: Convert to WebP Save-Data quality.
  {&kContentTypeWebp, "User-Agent,Save-Data", 3036},
};

const OptimizedImageInfoList kAnimationOptimizedForDesktopUa = {
  // [Save-Data: no, Via: yes]: Cannot optimize.
  {&kContentTypeGif, nullptr, 26251},
  // [Save-Data: no, Via: no]: Cannot optimize.
  {&kContentTypeGif, nullptr, 26251},
  // [Save-Data: yes, Via: yes]: Cannot optimize.
  {&kContentTypeGif, nullptr, 26251},
  // [Save-Data: yes, Via: no]: Cannot optimize.
  {&kContentTypeGif, nullptr, 26251},
};

const OptimizedImageInfoListInputs kOptimizedImageInfoList[] {
  // JPEG image, optimized for Chrome on Android.
  {UserAgentMatcherTestBase::kNexus6Chrome44UserAgent, kPuzzleJpgFile,
   &kPuzzleOptimizedForWebpUa},
  // JPEG image, optimized for Safari on iOS.
  {UserAgentMatcherTestBase::kCriOS31UserAgent, kPuzzleJpgFile,
   &kPuzzleOptimizedForSafariUa},
  // JPEG image, optimized for Firefox on desktop.
  {UserAgentMatcherTestBase::kFirefoxUserAgent, kPuzzleJpgFile,
   &kPuzzleOptimizedForDesktopUa},
  // Photographic PNG image, optimized for Chrome on Android.
  {UserAgentMatcherTestBase::kNexus6Chrome44UserAgent, kBikePngFile,
   &kBikeOptimizedForWebpUa},
  // Photographic PNG image, optimized for Safari on iOS.
  {UserAgentMatcherTestBase::kCriOS31UserAgent, kBikePngFile,
   &kBikeOptimizedForSafariUa},
  // Photographic PNG image, optimized for Firefox on desktop.
  {UserAgentMatcherTestBase::kFirefoxUserAgent, kBikePngFile,
   &kBikeOptimizedForDesktopUa},
  // Non-photographic PNG image, optimized for Chrome on Android.
  {UserAgentMatcherTestBase::kNexus6Chrome44UserAgent, kCuppaPngFile,
   &kCuppaOptimizedForWebpUa},
  // Non-photographic PNG image, optimized for Safari on iOS.
  {UserAgentMatcherTestBase::kCriOS31UserAgent, kCuppaPngFile,
   &kCuppaOptimizedForDesktopUa},
  // Non-photographic PNG image, optimized for Firefox on desktop.
  {UserAgentMatcherTestBase::kFirefoxUserAgent, kCuppaPngFile,
   &kCuppaOptimizedForDesktopUa},
  // Animated GIF image, optimized for Chrome on Android.
  {UserAgentMatcherTestBase::kNexus6Chrome44UserAgent, kAnimationGifFile,
   &kAnimationOptimizedForWebpUa},
  // Animated GIF image, optimized for Safari on iOS.
  {UserAgentMatcherTestBase::kCriOS31UserAgent, kAnimationGifFile,
   &kAnimationOptimizedForDesktopUa},
  // Animated GIF image, optimized for Firefox on desktop.
  {UserAgentMatcherTestBase::kFirefoxUserAgent, kAnimationGifFile,
   &kAnimationOptimizedForDesktopUa},
};

const OptimizedImageInfoList kPuzzleOptimizedForWebpUaAllowSaveDataAccept = {
  // [Save-Data: no, Via: yes]: Convert to WebP desktop quality.
  {&kContentTypeWebp, "Accept,Save-Data", 33108},
  // [Save-Data: no, Via: no]: Convert to WebP desktop quality.
  {&kContentTypeWebp, "Accept,Save-Data", 33108},
  // [Save-Data: yes, Via: yes]: Convert to WebP Save-Data quality.
  {&kContentTypeWebp, "Accept,Save-Data", 19124},
  // [Save-Data: yes, Via: no]: Convert to WebP Save-Data quality.
  {&kContentTypeWebp, "Accept,Save-Data", 19124},
};

const OptimizedImageInfoList kPuzzleOptimizedForWebpUaAllowUserAgent = {
  // [Save-Data: no, Via: yes]: Convert to WebP mobile quality.
  {&kContentTypeWebp, "User-Agent", 25774},
  // [Save-Data: no, Via: no]: Convert to WebP mobile quality.
  {&kContentTypeWebp, "User-Agent", 25774},
  // [Save-Data: yes, Via: yes]: Convert to WebP mobile quality.
  {&kContentTypeWebp, "User-Agent", 25774},
  // [Save-Data: yes, Via: no]: Convert to WebP mobile quality.
  {&kContentTypeWebp, "User-Agent", 25774},
};

const OptimizedImageInfoList kPuzzleOptimizedForWebpUaAllowAccept = {
  // [Save-Data: no, Via: yes]: Convert to WebP desktop quality.
  {&kContentTypeWebp, "Accept", 33108},
  // [Save-Data: no, Via: no]: Convert to WebP desktop quality.
  {&kContentTypeWebp, "Accept", 33108},
  // [Save-Data: yes, Via: yes]: Convert to WebP desktop quality.
  {&kContentTypeWebp, "Accept", 33108},
  // [Save-Data: yes, Via: no]: Convert to WebP desktop quality.
  {&kContentTypeWebp, "Accept", 33108},
};

const OptimizedImageInfoList kPuzzleOptimizedForWebpUaAllowSaveData = {
  // [Save-Data: no, Via: yes]: Convert to JPEG desktop quality.
  {&kContentTypeJpeg, "Save-Data", 73096},
  // [Save-Data: no, Via: no]: Convert to JPEG desktop quality.
  {&kContentTypeJpeg, "Save-Data", 73096},
  // [Save-Data: yes, Via: yes]: Convert to JPEG Save-Data quality.
  {&kContentTypeJpeg, "Save-Data", 38944},
  // [Save-Data: yes, Via: no]: Convert to JPEG Save-Data quality.
  {&kContentTypeJpeg, "Save-Data", 38944},
};

const OptimizedImageInfoList kPuzzleOptimizedForWebpUaAllowNone = {
  // [Save-Data: no, Via: yes]: Convert to JPEG desktop quality.
  {&kContentTypeJpeg, nullptr, 73096},
  // [Save-Data: no, Via: no]: Convert to JPEG desktop quality.
  {&kContentTypeJpeg, nullptr, 73096},
  // [Save-Data: yes, Via: yes]: Convert to JPEG desktop quality.
  {&kContentTypeJpeg, nullptr, 73096},
  // [Save-Data: yes, Via: no]: Convert to JPEG desktop quality.
  {&kContentTypeJpeg, nullptr, 73096},
};

const OptimizedImageInfoList kPuzzleOptimizedForWebpUaNoSaveDataQualities = {
  // [Save-Data: no, Via: yes]: Convert to WebP desktop quality.
  {&kContentTypeWebp, "Accept", 33108},
  // [Save-Data: no, Via: no]: Convert to WebP mobile quality.
  {&kContentTypeWebp, "User-Agent", 25774},
  // [Save-Data: yes, Via: yes]: Convert to WebP desktop quality.
  {&kContentTypeWebp, "Accept", 33108},
  // [Save-Data: yes, Via: no]: Convert to WebP mobile quality.
  {&kContentTypeWebp, "User-Agent", 25774},
};

const OptimizedImageInfoList kPuzzleOptimizedForWebpUaNoSmallScreenQualities = {
  // [Save-Data: no, Via: yes]: Convert to WebP desktop quality.
  {&kContentTypeWebp, "Accept,Save-Data", 33108},
  // [Save-Data: no, Via: no]: Convert to WebP desktop quality.
  {&kContentTypeWebp, "User-Agent,Save-Data", 33108},
  // [Save-Data: yes, Via: yes]: Convert to WebP Save-Data quality.
  {&kContentTypeWebp, "Accept,Save-Data", 19124},
  // [Save-Data: yes, Via: no]: Convert to WebP Save-Data quality.
  {&kContentTypeWebp, "User-Agent,Save-Data", 19124},
};

const OptimizedImageInfoList kPuzzleOptimizedForWebpUaNoSpecialQualities = {
  // [Save-Data: no, Via: yes]: Convert to WebP desktop quality.
  {&kContentTypeWebp, "Accept", 33108},
  // [Save-Data: no, Via: no]: Convert to WebP desktop quality.
  {&kContentTypeWebp, "User-Agent", 33108},
  // [Save-Data: yes, Via: yes]: Convert to WebP desktop quality.
  {&kContentTypeWebp, "Accept", 33108},
  // [Save-Data: yes, Via: no]: Convert to WebP desktop quality.
  {&kContentTypeWebp, "User-Agent", 33108},
};

// A callback for HTTP cache that stores body and string representation
// of headers into given strings.
class HTTPCacheStringCallback : public OptionsAwareHTTPCacheCallback {
 public:
  HTTPCacheStringCallback(const RewriteOptions* options,
                          const RequestContextPtr& request_ctx,
                          GoogleString* body_out, GoogleString* headers_out)
      : OptionsAwareHTTPCacheCallback(options, request_ctx),
        body_out_(body_out),
        headers_out_(headers_out), found_(false) {}

  virtual ~HTTPCacheStringCallback() {}

  virtual void Done(HTTPCache::FindResult find_result) {
    StringPiece contents;
    if ((find_result.status == HTTPCache::kFound) &&
        http_value()->ExtractContents(&contents)) {
      found_ = true;
      contents.CopyToString(body_out_);
      *headers_out_ = response_headers()->ToString();
    }
  }

  void ExpectFound() {
    EXPECT_TRUE(found_);
  }

 private:
  GoogleString* body_out_;
  GoogleString* headers_out_;
  bool found_;
  DISALLOW_COPY_AND_ASSIGN(HTTPCacheStringCallback);
};

}  // namespace

// TODO(huibao): Move CopyOnWriteLogRecord and TestRequestContext to a shared
// file.

// RequestContext that overrides NewSubordinateLogRecord to return a
// CopyOnWriteLogRecord that copies to a logging_info given at construction
// time.
class TestRequestContext : public RequestContext {
 public:
  TestRequestContext(LoggingInfo* logging_info,
                     AbstractMutex* mutex)
      : RequestContext(kDefaultHttpOptionsForTests, mutex, nullptr),
        logging_info_copy_(logging_info) {
  }

  virtual AbstractLogRecord* NewSubordinateLogRecord(
      AbstractMutex* logging_mutex) {
    return new CopyOnWriteLogRecord(logging_mutex, logging_info_copy_);
  }

 private:
  LoggingInfo* logging_info_copy_;

  DISALLOW_COPY_AND_ASSIGN(TestRequestContext);
};
typedef RefCountedPtr<TestRequestContext> TestRequestContextPtr;

class ImageRewriteTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    PropertyCache* pcache = page_property_cache();
    server_context_->set_enable_property_cache(true);
    const PropertyCache::Cohort* cohort =
        SetupCohort(pcache, RewriteDriver::kDomCohort);
    server_context()->set_dom_cohort(cohort);
    RewriteTestBase::SetUp();
    MockPropertyPage* page = NewMockPage(kTestDomain);
    pcache->set_enabled(true);
    rewrite_driver()->set_property_page(page);
    pcache->Read(page);

    // Ignore trivial message.
    MockMessageHandler* handler = message_handler();
    handler->AddPatternToSkipPrinting(kMessagePatternFailedToEncodeWebp);
    handler->AddPatternToSkipPrinting(kMessagePatternPixelFormat);
    handler->AddPatternToSkipPrinting(kMessagePatternRecompressing);
    handler->AddPatternToSkipPrinting(kMessagePatternResizedImage);
    handler->AddPatternToSkipPrinting(kMessagePatternShrinkingImage);
    handler->AddPatternToSkipPrinting(kMessagePatternStats);
    handler->AddPatternToSkipPrinting(kMessagePatternWebpTimeOut);
    handler->AddPatternToSkipPrinting(kMessagePatternWritingToWebp);
  }

  void RewriteImageFromHtml(const GoogleString& tag_string,
                            const ContentType& content_type,
                            GoogleString* img_src) {
    options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
    AddRecompressImageFilters();
    options()->set_image_inline_max_bytes(2000);
    rewrite_driver()->AddFilters();

    // URLs and content for HTML document and resources.
    const GoogleUrl domain(EncodeWithBase("http://rewrite_image.test/",
                                          "http://rewrite_image.test/",
                                          "x", "0", "x", "x"));
    static const char kHtmlUrl[] =
        "http://rewrite_image.test/RewriteImage.html";

    const GoogleString kImageHtml =
        StrCat("<head/><body><", tag_string, " src=\"Puzzle.jpg\"/></body>");

    ParseUrl(kHtmlUrl, kImageHtml);
    StringVector img_srcs;
    CollectImgSrcs("RewriteImage/collect_sources", output_buffer_, &img_srcs);
    // output_buffer_ should have exactly one image file (Puzzle.jpg).
    EXPECT_EQ(1UL, img_srcs.size());
    const GoogleUrl img_gurl(html_gurl(), img_srcs[0]);
    EXPECT_TRUE(img_gurl.IsWebValid());
    EXPECT_EQ(domain.AllExceptLeaf(), img_gurl.AllExceptLeaf());
    EXPECT_TRUE(img_gurl.LeafSansQuery().ends_with(
        content_type.file_extension()));
    *img_src = img_srcs[0];
  }

  // Simple image rewrite test to check resource fetching functionality.
  void RewriteImage(const GoogleString& tag_string,
                    const ContentType& content_type) {
    static const char kCacheFragment[] = "a-cache-fragment";
    options()->set_cache_fragment(kCacheFragment);

    // Store image contents into fetcher.
    AddFileToMockFetcher(kPuzzleUrl, kPuzzleJpgFile, kContentTypeJpeg, 100);

    // Capture normal headers for comparison. We need to do it now
    // since the clock -after- rewrite is non-deterministic, but it must be
    // at the initial value at the time of the rewrite.
    GoogleString expect_headers;
    AppendDefaultHeadersWithCanonical(content_type, kPuzzleUrl,
                                      &expect_headers);

    GoogleString src_string;

    Histogram* rewrite_latency_ok = statistics()->GetHistogram(
        ImageRewriteFilter::kImageRewriteLatencyOkMs);
    Histogram* rewrite_latency_failed = statistics()->GetHistogram(
        ImageRewriteFilter::kImageRewriteLatencyFailedMs);
    rewrite_latency_ok->Clear();
    rewrite_latency_failed->Clear();

    RewriteImageFromHtml(tag_string, content_type, &src_string);

    EXPECT_EQ(1, rewrite_latency_ok->Count());
    EXPECT_EQ(0, rewrite_latency_failed->Count());

    const GoogleString expected_output =
        StrCat("<head/><body><", tag_string, " src=\"", src_string,
               "\" width=\"1023\" height=\"766\"/></body>");
    EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);

    GoogleUrl img_gurl(html_gurl(), src_string);

    // Fetch the version we just put into the cache, so we can
    // make sure we produce it consistently.
    GoogleString rewritten_image;
    GoogleString rewritten_headers;
    HTTPCacheStringCallback cache_callback(
        options(), rewrite_driver()->request_context(),
        &rewritten_image, &rewritten_headers);
    http_cache()->Find(img_gurl.Spec().as_string(), kCacheFragment,
                       message_handler(), &cache_callback);
    cache_callback.ExpectFound();

    // Make sure the headers produced make sense.
    EXPECT_STREQ(expect_headers, rewritten_headers);

    // Also fetch the resource to ensure it can be created dynamically
    ExpectStringAsyncFetch expect_callback(true, CreateRequestContext());
    lru_cache()->Clear();

    // New time --- new timestamp.
    expect_headers.clear();
    AppendDefaultHeadersWithCanonical(content_type, kPuzzleUrl,
                                      &expect_headers);

    EXPECT_TRUE(rewrite_driver()->FetchResource(img_gurl.Spec(),
                                                &expect_callback));
    rewrite_driver()->WaitForCompletion();
    EXPECT_EQ(HttpStatus::kOK,
              expect_callback.response_headers()->status_code()) <<
        "Looking for " << src_string;
    EXPECT_STREQ(rewritten_image, expect_callback.buffer());
    EXPECT_STREQ(expect_headers,
                 expect_callback.response_headers()->ToString());
    // Try to fetch from an independent server.
    ServeResourceFromManyContextsWithUA(
        img_gurl.Spec().as_string(), rewritten_image,
        rewrite_driver()->user_agent());

    // Check that filter application was logged.
    EXPECT_STREQ("ic", AppliedRewriterStringFromLog());
  }

  void TestInlining(bool convert_to_webp, const char* user_agent,
                    const StringPiece& file_name, const ContentType& input_type,
                    const ContentType& output_type, bool expect_inline) {
    ClearRewriteDriver();

    SetCurrentUserAgent(user_agent);
    if (convert_to_webp) {
      options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
      options()->EnableFilter(RewriteOptions::kConvertToWebpLossless);
      AddRequestAttribute(HttpAttributes::kAccept, "image/webp");
    }
    SetDriverRequestHeaders();

    options()->set_image_inline_max_bytes(1000000);
    options()->EnableFilter(RewriteOptions::kInlineImages);
    options()->EnableFilter(RewriteOptions::kConvertGifToPng);
    options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
    options()->EnableFilter(RewriteOptions::kRecompressJpeg);
    options()->EnableFilter(RewriteOptions::kRecompressPng);
    rewrite_driver()->AddFilters();

    TestSingleRewrite(file_name, input_type, output_type, "", "",
                      true /*expect_rewritten*/, expect_inline);
  }

  void SetupIproTests(const char* allow_vary_on_string) {
    EXPECT_TRUE(options()->EnableFiltersByCommaSeparatedList(
        "recompress_images,convert_to_webp_lossless,convert_to_webp_animated,"
        "convert_png_to_jpeg,in_place_optimize_for_browser",
        message_handler()));

    GoogleString puzzleUrl = StrCat(kTestDomain, kPuzzleJpgFile);
    GoogleString bikeUrl   = StrCat(kTestDomain, kBikePngFile);
    GoogleString cuppaUrl  = StrCat(kTestDomain, kCuppaPngFile);
    GoogleString animationUrl  = StrCat(kTestDomain, kAnimationGifFile);
    AddFileToMockFetcher(puzzleUrl, kPuzzleJpgFile, kContentTypeJpeg, 100);
    AddFileToMockFetcher(bikeUrl, kBikePngFile, kContentTypePng, 100);
    AddFileToMockFetcher(cuppaUrl, kCuppaPngFile, kContentTypePng, 100);
    AddFileToMockFetcher(animationUrl, kAnimationGifFile, kContentTypeGif, 100);

    UseMd5Hasher();
    options()->set_image_preserve_urls(true);
    options()->set_in_place_rewriting_enabled(true);
    options()->set_in_place_wait_for_optimized(true);
    options()->set_image_recompress_quality(90);
    options()->set_image_jpeg_recompress_quality(75);
    options()->set_image_jpeg_recompress_quality_for_small_screens(55);
    options()->set_image_jpeg_quality_for_save_data(35);
    options()->set_image_webp_recompress_quality(70);
    options()->set_image_webp_recompress_quality_for_small_screens(50);
    options()->set_image_webp_quality_for_save_data(30);

    RewriteOptions::AllowVaryOn allow_vary_on;
    EXPECT_TRUE(RewriteOptions::ParseFromString(allow_vary_on_string,
                                                &allow_vary_on));
    options()->set_allow_vary_on(allow_vary_on);
  }

  void IproFetchAndValidateWithHeaders(
      const char* image_name, const char* user_agent,
      const OptimizedImageInfoList& optimized_info_list) {
    IproFetchAndValidate(image_name, user_agent,
                         false /* save-data header */,
                         true /* via header */,
                         optimized_info_list.with_via);

    IproFetchAndValidate(image_name, user_agent,
                         false /* save-data header */,
                         false /* via header */,
                         optimized_info_list.with_none);

    IproFetchAndValidate(image_name, user_agent,
                         true /* save-data header */,
                         true /* via header */,
                         optimized_info_list.with_savedata_via);

    IproFetchAndValidate(image_name, user_agent,
                         true /* save-data header */,
                         false /* via header */,
                         optimized_info_list.with_savedata);
  }

  // Helper class to collect image srcs.
  class ImageCollector : public EmptyHtmlFilter {
   public:
    ImageCollector(HtmlParse* html_parse, StringVector* img_srcs)
        : img_srcs_(img_srcs) {
    }

    virtual void StartElement(HtmlElement* element) {
      resource_tag_scanner::UrlCategoryVector attributes;
      NullThreadSystem thread_system;
      RewriteOptions options(&thread_system);
      resource_tag_scanner::ScanElement(element, &options, &attributes);
      for (int i = 0, n = attributes.size(); i < n; ++i) {
        if (attributes[i].category == semantic_type::kImage) {
          img_srcs_->push_back(attributes[i].url->DecodedValueOrNull());
        }
      }
    }

    virtual const char* Name() const { return "ImageCollector"; }

   private:
    StringVector* img_srcs_;

    DISALLOW_COPY_AND_ASSIGN(ImageCollector);
  };

  // Fills `img_srcs` with the urls in img src attributes in `html`
  void CollectImgSrcs(const StringPiece& id, const StringPiece& html,
                        StringVector* img_srcs) {
    HtmlParse html_parse(&message_handler_);
    ImageCollector collector(&html_parse, img_srcs);
    html_parse.AddFilter(&collector);
    GoogleString dummy_url = StrCat("http://collect.css.links/", id, ".html");
    html_parse.StartParse(dummy_url);
    html_parse.ParseText(html.data(), html.size());
    html_parse.FinishParse();
  }

  void DataUrlResource() {
    static const char* kCuppaData = "data:image/png;base64,"
        "iVBORw0KGgoAAAANSUhEUgAAAEEAAABGCAAAAAC2maYhAAAC00lEQVQY0+3PTUhUYR"
        "QG4HdmMhUaC6FaKSqEZS2MsEJEsaKSwMKgot2QkkKFUFBYWgSpGIhSZH+0yAgLDQ3p"
        "ByoLRS2DjCjEfm0MzQhK08wZ5/Sde12kc8f5DrXLs3lfPs55uBf0t4MZ4X8QLjeY2X"
        "C80cieUq9M6MB6I7tDcMgoRWgVCb5VyDLKFuCK8RCHMpFwEzjA+coGdHJ5COwRCSnA"
        "Jc4cwOnlshs4KhFeA+jib48A1hovK4A6iXADiOB8oyQXF28Y0CIRKgDHsMoeJaTyw6"
        "gDOC0RGtXlPS5RQOgAlwQgWSK4lZDDZacqxVyOqNIpECgSiBxTeVsdRo/z/9iBXImw"
        "TV3eUemLU6WRXzYCziGB0KAOs7kUqLKZS40qVwVCr9qP4vJElblc3KocFAi+cMD2U5"
        "VBdYhPqgyp3CcQKEYdDHCZDYT/mviYa5JvCANiubxTh2u4XAAcfQLhgzrM51KjSjmX"
        "FGAvCYRTQGgvlwwggX/iGbDwm0RIAwo439tga+biAqpJIHy2I36Uyxkgl7MnBJkkEV"
        "4AtUbJQvwP86/m94uE71juM8piPDayDOdJJNDKFjMzNpl5fcmYUPBMZIfbzBE3CQXB"
        "TBIuHtaYwo5phHToTMk0QqaWUNxUUXrui7XggvZEFI9YCfu1AQeQbiWc0LrOe9D11Z"
        "cNtFsIVVpCG696YrHVQqjVAezDxm4hEi2ElzpCvLl7EkkWwliIhrDD3K1EsoVASzWE"
        "UnM1DbushO0aQpux2Qw8shJKggPzvLzYl4BYn5XQHVzI4r2Pi4CzZCVQUlChimi0cg"
        "GQR9ZCRVDhbl1RtIoNngBC/yzozLJqLwUQqCjotTPR1fTnxVTBs3ra89T6/ikHfgK9"
        "dQa+t1eS//gJVB8WUCgnLYHaYwIAeaQp0GC25S8cG9cWiOrm+AHrnhMJBLplmwLkE8"
        "kEenp/8oyIBf2ZEWaEfyv8BsICdAZ/XeTCAAAAAElFTkSuQmCC";
    GoogleString cuppa_string(kCuppaData);
    ResourcePtr cuppa_resource(
        rewrite_driver()->CreateInputResourceAbsoluteUncheckedForTestsOnly(
            cuppa_string));
    ASSERT_TRUE(cuppa_resource.get() != nullptr);
    EXPECT_TRUE(ReadIfCached(cuppa_resource));
    GoogleString cuppa_contents;
    cuppa_resource->ExtractUncompressedContents().CopyToString(&cuppa_contents);
    // Now make sure axing the original cuppa_string doesn't affect the
    // internals of the cuppa_resource.
    ResourcePtr other_resource(
        rewrite_driver()->CreateInputResourceAbsoluteUncheckedForTestsOnly(
            cuppa_string));
    ASSERT_TRUE(other_resource.get() != nullptr);
    cuppa_string.clear();
    EXPECT_TRUE(ReadIfCached(other_resource));
    GoogleString other_contents;
    cuppa_resource->ExtractUncompressedContents().CopyToString(&other_contents);
    ASSERT_EQ(cuppa_contents, other_contents);
  }

  // Helper to test for how we handle trailing junk in URLs
  void TestCorruptUrl(StringPiece junk, bool append_junk) {
    const char kHtml[] =
        "<img src=\"a.jpg\"><img src=\"b.png\"><img src=\"c.gif\">";
    AddFileToMockFetcher(StrCat(kTestDomain, "a.jpg"), kPuzzleJpgFile,
                         kContentTypeJpeg, 100);

    AddFileToMockFetcher(StrCat(kTestDomain, "b.png"), kBikePngFile,
                         kContentTypePng, 100);

    AddFileToMockFetcher(StrCat(kTestDomain, "c.gif"), kChefGifFile,
                         kContentTypeGif, 100);

    options()->EnableFilter(RewriteOptions::kConvertGifToPng);
    options()->EnableFilter(RewriteOptions::kRecompressPng);
    options()->EnableFilter(RewriteOptions::kRecompressJpeg);
    rewrite_driver()->AddFilters();

    StringVector img_srcs;
    ImageCollector image_collect(rewrite_driver(), &img_srcs);
    rewrite_driver()->AddFilter(&image_collect);

    ParseUrl(kTestDomain, kHtml);
    ASSERT_EQ(3, img_srcs.size());
    GoogleString normal_output = output_buffer_;
    GoogleString url1 = img_srcs[0];
    GoogleString url2 = img_srcs[1];
    GoogleString url3 = img_srcs[2];

    GoogleUrl gurl1(html_gurl(), url1);
    GoogleUrl gurl2(html_gurl(), url2);
    GoogleUrl gurl3(html_gurl(), url3);

    // Fetch messed up versions. Currently image rewriter doesn't actually
    // fetch them.
    GoogleString out;
    EXPECT_TRUE(FetchResourceUrl(ChangeSuffix(gurl1.Spec(), append_junk,
                                              ".jpg", junk), &out));
    EXPECT_TRUE(FetchResourceUrl(ChangeSuffix(gurl2.Spec(), append_junk,
                                              ".png", junk), &out));
    // This actually has .png in the output since we convert gif -> png.
    EXPECT_TRUE(FetchResourceUrl(ChangeSuffix(gurl3.Spec(), append_junk,
                                              ".png", junk), &out));

    // Now run through again to make sure we didn't cache the messed up URL
    img_srcs.clear();
    ParseUrl(kTestDomain, kHtml);
    EXPECT_EQ(normal_output, output_buffer_);
    ASSERT_EQ(3, img_srcs.size());
    EXPECT_EQ(url1, img_srcs[0]);
    EXPECT_EQ(url2, img_srcs[1]);
    EXPECT_EQ(url3, img_srcs[2]);
  }

  // Fetch a simple document referring to an image with filename "name" on a
  // mock domain.  Check that final dimensions are as expected, that rewriting
  // occurred as expected, and that inlining occurred if that was anticipated.
  // Assumes rewrite_driver has already been appropriately configured for the
  // image rewrites under test.
  void TestSingleRewrite(const StringPiece& name,
                         const ContentType& input_type,
                         const ContentType& output_type,
                         const char* initial_attributes,
                         const char* final_attributes,
                         bool expect_rewritten,
                         bool expect_inline) {
    GoogleString initial_url = StrCat(kTestDomain, name);
    TestSingleRewriteWithoutAbs(initial_url, name, input_type, output_type,
        initial_attributes, final_attributes, expect_rewritten, expect_inline);
  }

  void TestSingleRewriteWithoutAbs(const GoogleString& initial_url,
                                   const StringPiece& name,
                                   const ContentType& input_type,
                                   const ContentType& output_type,
                                   const char* initial_attributes,
                                   const char* final_attributes,
                                   bool expect_rewritten,
                                   bool expect_inline) {
    GoogleString page_url = StrCat(kTestDomain, "test.html");
    AddFileToMockFetcher(initial_url, name, input_type, 100);

    const char html_boilerplate[] = "<img src='%s'%s>";
    GoogleString html_input =
        StringPrintf(html_boilerplate, initial_url.c_str(), initial_attributes);

    ParseUrl(page_url, html_input);

    // Check for single image file in the rewritten page.
    StringVector image_urls;
    CollectImgSrcs(initial_url, output_buffer_, &image_urls);
    EXPECT_EQ(1, image_urls.size());
    const GoogleString& rewritten_url = image_urls[0];
    const GoogleUrl rewritten_gurl(rewritten_url);
    EXPECT_TRUE(rewritten_gurl.IsWebOrDataValid()) << rewritten_url;

    if (expect_inline) {
      EXPECT_TRUE(rewritten_gurl.SchemeIs("data"))
          << rewritten_gurl.spec_c_str();
      GoogleString expected_start =
          StrCat("data:", output_type.mime_type(), ";base64,");
      EXPECT_TRUE(rewritten_gurl.Spec().starts_with(expected_start))
          << "expected " << expected_start << " got " << rewritten_url;
    } else if (expect_rewritten) {
      EXPECT_NE(initial_url, rewritten_url);
      EXPECT_TRUE(rewritten_gurl.LeafSansQuery().ends_with(
          output_type.file_extension()))
          << "expected end " << output_type.file_extension()
          << " got " << rewritten_gurl.LeafSansQuery();
    } else {
      EXPECT_EQ(initial_url, rewritten_url);
      EXPECT_TRUE(rewritten_gurl.LeafSansQuery().ends_with(
          output_type.file_extension()))
          << "expected end " << output_type.file_extension()
          << " got " << rewritten_gurl.LeafSansQuery();
    }

    GoogleString html_expected_output =
        StringPrintf(html_boilerplate, rewritten_url.c_str(), final_attributes);
    EXPECT_EQ(AddHtmlBody(html_expected_output), output_buffer_);
  }

  // Returns the property cache value for kInlinableImageUrlsPropertyName,
  // or nullptr if it is not present.
  const PropertyValue* FetchInlinablePropertyCacheValue() {
    PropertyCache* pcache = page_property_cache();
    if (pcache == nullptr) {
      return nullptr;
    }
    const PropertyCache::Cohort* cohort = pcache->GetCohort(
        RewriteDriver::kDomCohort);
    if (cohort == nullptr) {
      return nullptr;
    }
    PropertyPage* property_page = rewrite_driver()->property_page();
    if (property_page == nullptr) {
      return nullptr;
    }
    return property_page->GetProperty(
        cohort, ImageRewriteFilter::kInlinableImageUrlsPropertyName);
  }

  // Test dimensions of an optimized image by fetching it.
  void TestDimensionRounding(
      StringPiece leaf, int expected_width, int expected_height) {
    GoogleString initial_url = StrCat(kTestDomain, kPuzzleJpgFile);
    GoogleString fetch_url = StrCat(kTestDomain, leaf);
    AddFileToMockFetcher(initial_url, kPuzzleJpgFile, kContentTypeJpeg, 100);
    // Set up resizing
    options()->EnableFilter(RewriteOptions::kResizeImages);
    rewrite_driver()->AddFilters();
    // Perform resource fetch
    ExpectStringAsyncFetch expect_callback(true, CreateRequestContext());
    EXPECT_TRUE(rewrite_driver()->FetchResource(fetch_url, &expect_callback));
    rewrite_driver()->WaitForCompletion();
    EXPECT_EQ(HttpStatus::kOK,
              expect_callback.response_headers()->status_code()) <<
        "Looking for " << fetch_url;
    // Look up dimensions of resulting image
    scoped_ptr<Image> image(
        NewImage(expect_callback.buffer(),
                 fetch_url, server_context_->filename_prefix(),
                 new Image::CompressionOptions(),
                 timer(), &message_handler_));
    ImageDim image_dim;
    image->Dimensions(&image_dim);
    EXPECT_EQ(expected_width, image_dim.width());
    EXPECT_EQ(expected_height, image_dim.height());
  }

  void TestTranscodeAndOptimizePng(bool expect_rewritten,
                                   const char* width_height_tags,
                                   const ContentType& expected_type) {
    // Make sure we convert png to jpeg if we requested that.
    // We lower compression quality to ensure the jpeg is smaller.
    options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
    options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
    options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
    options()->set_image_jpeg_recompress_quality(85);
    rewrite_driver()->AddFilters();
    TestSingleRewrite(kBikePngFile, kContentTypePng, expected_type,
                      "", width_height_tags, expect_rewritten, false);
  }

  void TestConversionVariables(int gif_webp_timeout,
                               int gif_webp_success,
                               int gif_webp_failure,

                               int png_webp_timeout,
                               int png_webp_success,
                               int png_webp_failure,

                               int jpeg_webp_timeout,
                               int jpeg_webp_success,
                               int jpeg_webp_failure,

                               int gif_webp_animated_timeout,
                               int gif_webp_animated_success,
                               int gif_webp_animated_failure,

                               bool is_opaque) {
    EXPECT_EQ(
        gif_webp_timeout,
        statistics()->GetVariable(
            ImageRewriteFilter::kImageWebpFromGifTimeouts)->
        Get());
    EXPECT_EQ(
        gif_webp_success,
        statistics()->GetHistogram(
            ImageRewriteFilter::kImageWebpFromGifSuccessMs)->
        Count());
    EXPECT_EQ(
        gif_webp_failure,
        statistics()->GetHistogram(
            ImageRewriteFilter::kImageWebpFromGifFailureMs)->
        Count());

    EXPECT_EQ(
        png_webp_timeout,
        statistics()->GetVariable(
            ImageRewriteFilter::kImageWebpFromPngTimeouts)->
        Get());
    EXPECT_EQ(
        png_webp_success,
        statistics()->GetHistogram(
            ImageRewriteFilter::kImageWebpFromPngSuccessMs)->
        Count());
    EXPECT_EQ(
        png_webp_failure,
        statistics()->GetHistogram(
            ImageRewriteFilter::kImageWebpFromPngFailureMs)->
        Count());

    EXPECT_EQ(
        jpeg_webp_timeout,
        statistics()->GetVariable(
            ImageRewriteFilter::kImageWebpFromJpegTimeouts)->
        Get());
    EXPECT_EQ(
        jpeg_webp_success,
        statistics()->GetHistogram(
            ImageRewriteFilter::kImageWebpFromJpegSuccessMs)->
        Count());
    EXPECT_EQ(
        jpeg_webp_failure,
        statistics()->GetHistogram(
            ImageRewriteFilter::kImageWebpFromJpegFailureMs)->
        Count());

    EXPECT_EQ(
        gif_webp_animated_timeout,
        statistics()->GetVariable(
            ImageRewriteFilter::kImageWebpFromGifAnimatedTimeouts)->
        Get());
    EXPECT_EQ(
        gif_webp_animated_success,
        statistics()->GetHistogram(
            ImageRewriteFilter::kImageWebpFromGifAnimatedSuccessMs)->
        Count());
    EXPECT_EQ(
        gif_webp_animated_failure,
        statistics()->GetHistogram(
            ImageRewriteFilter::kImageWebpFromGifAnimatedFailureMs)->
        Count());

    int total_timeout =
        gif_webp_timeout +
        png_webp_timeout +
        jpeg_webp_timeout +
        gif_webp_animated_timeout;
    int total_success =
        gif_webp_success +
        png_webp_success +
        jpeg_webp_success +
        gif_webp_animated_success;
    int total_failure =
        gif_webp_failure +
        png_webp_failure +
        jpeg_webp_failure +
        gif_webp_animated_failure;

    EXPECT_EQ(
        total_timeout,
        statistics()->GetVariable(
            is_opaque ?
            ImageRewriteFilter::kImageWebpOpaqueTimeouts :
            ImageRewriteFilter::kImageWebpWithAlphaTimeouts)->Get());
    EXPECT_EQ(
        total_success,
        statistics()->GetHistogram(
            is_opaque ?
            ImageRewriteFilter::kImageWebpOpaqueSuccessMs :
            ImageRewriteFilter::kImageWebpWithAlphaSuccessMs)->Count());
    EXPECT_EQ(
        total_failure,
        statistics()->GetHistogram(
            is_opaque ?
            ImageRewriteFilter::kImageWebpOpaqueFailureMs :
            ImageRewriteFilter::kImageWebpWithAlphaFailureMs)->Count());
  }

  // Verify log for background image rewriting. To skip url, pass in an empty
  // string. To skip original_size or optimized_size,  pass in kIgnoreSize.
  void TestBackgroundRewritingLog(
      int rewrite_info_size,
      int rewrite_info_index,
      RewriterApplication::Status status,
      const char* id,
      const GoogleString& url,
      ImageType original_type,
      ImageType optimized_type,
      int original_size,
      int optimized_size,
      bool is_recompressed,
      bool is_resized,
      int original_width,
      int original_height,
      bool is_resized_using_rendered_dimensions,
      int resized_width,
      int resized_height) {
    // Check URL.
    net_instaweb::ResourceUrlInfo* url_info =
        logging_info_.mutable_resource_url_info();
    if (!url.empty()) {
      EXPECT_LT(0, url_info->url_size());
      if (url_info->url_size() > 0) {
        EXPECT_EQ(url, url_info->url(0));
      }
    } else {
      EXPECT_EQ(0, url_info->url_size());
    }

    EXPECT_EQ(rewrite_info_size, logging_info_.rewriter_info_size());
    const RewriterInfo& rewriter_info =
        logging_info_.rewriter_info(rewrite_info_index);
    EXPECT_EQ(id, rewriter_info.id());
    EXPECT_EQ(status, rewriter_info.status());

    ASSERT_TRUE(rewriter_info.has_rewrite_resource_info());
    const RewriteResourceInfo& resource_info =
        rewriter_info.rewrite_resource_info();

    if (original_size != kIgnoreSize) {
      EXPECT_EQ(original_size, resource_info.original_size());
    }
    if (optimized_size != kIgnoreSize) {
      EXPECT_EQ(optimized_size, resource_info.optimized_size());
    }
    EXPECT_EQ(is_recompressed, resource_info.is_recompressed());

    ASSERT_TRUE(rewriter_info.has_image_rewrite_resource_info());
    const ImageRewriteResourceInfo& image_info =
        rewriter_info.image_rewrite_resource_info();
    EXPECT_EQ(original_type, image_info.original_image_type());
    EXPECT_EQ(optimized_type, image_info.optimized_image_type());
    EXPECT_EQ(is_resized, image_info.is_resized());
    EXPECT_EQ(original_width, image_info.original_width());
    EXPECT_EQ(original_height, image_info.original_height());
    EXPECT_EQ(is_resized_using_rendered_dimensions,
              image_info.is_resized_using_rendered_dimensions());
    EXPECT_EQ(resized_width, image_info.resized_width());
    EXPECT_EQ(resized_height, image_info.resized_height());
  }

  void TestForRenderedDimensions(MockCriticalImagesFinder* finder,
                                 int width, int height,
                                 int expected_width, int expected_height,
                                 const char* dimensions_attribute,
                                 const GoogleString& expected_rewritten_url,
                                 int num_rewrites_using_rendered_dimensions) {
    RenderedImages* rendered_images = new RenderedImages;
    RenderedImages_Image* images = rendered_images->add_image();
    images->set_src(StrCat(kTestDomain, kChefGifFile));
    if (width != 0) {
      images->set_rendered_width(width);
    }
    if (height != 0) {
      images->set_rendered_height(height);
    }

    // Original size of kChefGifFile is 192x256
    finder->set_rendered_images(rendered_images);
    TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypePng,
                      dimensions_attribute, dimensions_attribute, true, false);

    // Check for single image file in the rewritten page.
    StringVector image_urls;
    CollectImgSrcs(kChefGifFile, output_buffer_, &image_urls);
    EXPECT_EQ(1, image_urls.size());
    const GoogleString& rewritten_url = image_urls[0];

    EXPECT_STREQ(rewritten_url, expected_rewritten_url);
    GoogleString output_png;
    EXPECT_TRUE(FetchResourceUrl(rewritten_url, &output_png));
    // Check if we resized to rendered dimensions.
    scoped_ptr<Image> image(
        NewImage(output_png, rewritten_url, server_context_->filename_prefix(),
                 new Image::CompressionOptions(),
                 timer(), &message_handler_));
    ImageDim image_dim;
    image->Dimensions(&image_dim);
    EXPECT_EQ(expected_width, image_dim.width());
    EXPECT_EQ(expected_height, image_dim.height());
    Variable* resized_using_rendered_dimensions = statistics()->GetVariable(
        ImageRewriteFilter::kImageResizedUsingRenderedDimensions);
    EXPECT_EQ(num_rewrites_using_rendered_dimensions,
              resized_using_rendered_dimensions->Get());
    resized_using_rendered_dimensions->Clear();
  }

  // Override CreateRequestContext so that we are always pointing it at
  // LoggingInfo structure that we retain across request lifetime.
  virtual RequestContextPtr CreateRequestContext() {
    return RequestContextPtr(new TestRequestContext(
        &logging_info_, factory()->thread_system()->NewMutex()));
  }

  // Fetches a URL for the given user-agent, returning success-status,
  // and modifying content and response if successful.  Statistics are
  // cleared on each call.
  bool FetchWebp(StringPiece url, StringPiece user_agent,
                 GoogleString* content, ResponseHeaders* response) {
    content->clear();
    response->Clear();
    ClearStats();
    if (user_agent == "webp") {
      ResetForWebp();
    } else {
      ResetUserAgent(user_agent);
    }
    return FetchResourceUrl(url, content, response);
  }

  void IProFetchAndValidate(
      StringPiece url, StringPiece user_agent, StringPiece accept,
      ResponseHeaders* response) {
    ClearRewriteDriver();
    if (!user_agent.empty()) {
      SetCurrentUserAgent(user_agent);
    }
    if (!accept.empty()) {
      AddRequestAttribute(HttpAttributes::kAccept, accept);
    }
    GoogleString content_ignored;
    response->Clear();
    EXPECT_TRUE(FetchResourceUrl(url, &content_ignored, response));
    const char* etag = response->Lookup1(HttpAttributes::kEtag);
    EXPECT_EQ(0, GoogleString(etag).find("W/\"PSA-aj-")) << etag;
  }

  void IproFetchAndValidate(
      const char* image_name, StringPiece user_agent, bool has_save_data_header,
      bool has_via_header,
      const OptimizedImageInfo& expected_optimized_image_info) {
    GoogleString url = StrCat(kTestDomain, image_name);
    const ContentType* expected_content_type =
        expected_optimized_image_info.content_type;
    const char* expected_vary_header =
        expected_optimized_image_info.vary_header;
    int expected_content_length = expected_optimized_image_info.content_length;

    GoogleString response_content;
    ResponseHeaders response_headers;
    ClearRewriteDriver();
    if (!user_agent.empty()) {
      SetCurrentUserAgent(user_agent);
    }
    if (user_agent.find("Chrome/") != StringPiece::npos) {
      AddRequestAttribute(HttpAttributes::kAccept, "image/webp");
    }
    if (has_save_data_header) {
      AddRequestAttribute(HttpAttributes::kSaveData, "on");
    }
    if (has_via_header) {
      AddRequestAttribute(HttpAttributes::kVia, "proxy");
    }

    EXPECT_TRUE(FetchResourceUrl(url, &response_content, &response_headers));

    EXPECT_EQ(expected_content_type->type(),
              response_headers.DetermineContentType()->type()) <<
        response_headers.DetermineContentType()->mime_type();

    if (expected_vary_header != nullptr) {
      ConstStringStarVector vary_header_vector;
      EXPECT_TRUE(response_headers.Lookup(HttpAttributes::kVary,
                                          &vary_header_vector));
      GoogleString vary_header = JoinStringStar(vary_header_vector, ",");
      EXPECT_STREQ(expected_vary_header, vary_header);
    } else {
      EXPECT_FALSE(response_headers.Has(HttpAttributes::kVary));
    }

    // Because the image encoder may change behavior, content length of the
    // optimized image may change value slightly. To be resistant to such
    // change, we check the content size in a range, in stead of the exact
    // value. The range is defined by variable "threshold".
    const int threshold = 80;
    int content_length = response_content.length();
    EXPECT_LE(expected_content_length - threshold, content_length)
        << content_length;
    EXPECT_GE(expected_content_length + threshold, content_length)
        << content_length;
  }

  void TestResolutionLimit(int resolution, const char* image_file,
                           const ContentType& content_type, bool try_webp,
                           bool try_resize, bool expect_rewritten) {
    SetupForWebpLossless();
    options()->set_image_resolution_limit_bytes(resolution);
    options()->set_image_jpeg_recompress_quality(85);
    options()->EnableFilter(RewriteOptions::kRecompressPng);
    options()->EnableFilter(RewriteOptions::kRecompressJpeg);

    ContentType rewritten_type = content_type;
    if (try_webp) {
      options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
      options()->EnableFilter(RewriteOptions::kConvertToWebpLossless);
      if (expect_rewritten) {
        rewritten_type = kContentTypeWebp;
      }
    }

    const char* dimension = nullptr;
    if (try_resize) {
      options()->EnableFilter(RewriteOptions::kResizeImages);
      dimension = " width=4000 height=2000";
    } else {
      dimension = "";
    }
    rewrite_driver()->AddFilters();

    TestSingleRewrite(image_file, content_type, rewritten_type, dimension,
                      dimension, expect_rewritten, false);

    Variable* image_rewrites = statistics()->GetVariable(
        ImageRewriteFilter::kImageRewrites);
    Variable* no_rewrites = statistics()->GetVariable(
        ImageRewriteFilter::kImageNoRewritesHighResolution);
    if (expect_rewritten) {
      EXPECT_EQ(1, image_rewrites->Get());
      EXPECT_EQ(0, no_rewrites->Get());
    } else {
      EXPECT_EQ(0, image_rewrites->Get());
      EXPECT_EQ(1, no_rewrites->Get());
    }
  }

  void ResetUserAgent(StringPiece user_agent) {
    ClearRewriteDriver();
    SetCurrentUserAgent(user_agent);
    SetDriverRequestHeaders();
  }

  void ResetForWebp() {
    ClearRewriteDriver();
    SetupForWebp();
    SetDriverRequestHeaders();
  }

  void MarkTooBusyToWork() {
    // Set the current # of rewrites very high, so we stop doing more
    // due to "load".
    UpDownCounter* ongoing_rewrites = statistics()->GetUpDownCounter(
        WorkBoundExpensiveOperationController::kCurrentExpensiveOperations);
    ongoing_rewrites->Set(100);
  }

  void UnMarkTooBusyToWork() {
    UpDownCounter* ongoing_rewrites = statistics()->GetUpDownCounter(
        WorkBoundExpensiveOperationController::kCurrentExpensiveOperations);
    ongoing_rewrites->Set(0);
  }

 private:
  LoggingInfo logging_info_;
};

TEST_F(ImageRewriteTest, ImgTag) {
  RewriteImage("img", kContentTypeJpeg);
}

TEST_F(ImageRewriteTest, ImgSrcSet) {
  AddFileToMockFetcher("a.png", kBikePngFile, kContentTypePng, 100);
  AddFileToMockFetcher("b.png", kCuppaPngFile, kContentTypePng, 100);

  options()->EnableFilter(RewriteOptions::kRecompressPng);
  rewrite_driver()->AddFilters();

  ValidateExpected(
      "srcset",
      "<img src=\"a.png\" srcset=\"a.png 1x, b.png 2x\">",
      "<img src=\"xa.png.pagespeed.ic.0.png\" "
      "srcset=\"xa.png.pagespeed.ic.0.png 1x, xb.png.pagespeed.ic.0.png 2x\">");
}

TEST_F(ImageRewriteTest, ImgSrcSetWithCacheExtender) {
  // Makes sure cache extender properly shares the slot.
  options()->EnableExtendCacheFilters();
  AddFileToMockFetcher("a.png", kBikePngFile, kContentTypePng, 100);
  AddFileToMockFetcher("b.png", kCuppaPngFile, kContentTypePng, 100);

  options()->EnableFilter(RewriteOptions::kRecompressPng);
  rewrite_driver()->AddFilters();

  ValidateExpected(
      "srcset",
      "<img src=\"a.png\" srcset=\"a.png 1x, b.png 2x\">",
      "<img src=\"xa.png.pagespeed.ic.0.png\" "
      "srcset=\"xa.png.pagespeed.ic.0.png 1x, xb.png.pagespeed.ic.0.png 2x\">");
}

TEST_F(ImageRewriteTest, ImgTagWithComputeStatistics) {
  options()->EnableFilter(RewriteOptions::kComputeStatistics);
  RewriteImage("img", kContentTypeJpeg);
  EXPECT_EQ(1, rewrite_driver()->dom_stats_filter()->num_img_tags());
  EXPECT_EQ(0, rewrite_driver()->dom_stats_filter()->num_inlined_img_tags());
}

TEST_F(ImageRewriteTest, ImgTagWebp) {
  if (RunningOnValgrind()) {
    return;
  }
  // We use the webp testing user agent; real webp-capable user agents are
  // tested as part of user_agent_matcher_test and are likely to remain in flux
  // over time.
  SetupForWebp();
  RewriteImage("img", kContentTypeWebp);
}

TEST_F(ImageRewriteTest, ImgTagWebpLa) {
  if (RunningOnValgrind()) {
    return;
  }
  // We use the webp testing user agent; real webp-capable user agents are
  // tested as part of user_agent_matcher_test and are likely to remain in flux
  // over time.
  SetupForWebpLossless();
  options()->EnableFilter(RewriteOptions::kConvertToWebpLossless);

  RewriteImage("img", kContentTypeWebp);
}

TEST_F(ImageRewriteTest, InputTag) {
  RewriteImage("input type=\"image\"", kContentTypeJpeg);
}

TEST_F(ImageRewriteTest, InputTagWebp) {
  if (RunningOnValgrind()) {
    return;
  }
  // We use the webp testing user agent; real webp-capable user agents are
  // tested as part of user_agent_matcher_test and are likely to remain in flux
  // over time.
  SetupForWebp();
  RewriteImage("input type=\"image\"", kContentTypeWebp);
}

TEST_F(ImageRewriteTest, InputTagWebpLa) {
  if (RunningOnValgrind()) {
    return;
  }
  // We use the webp-la testing user agent; real webp-capable user agents are
  // tested as part of user_agent_matcher_test and are likely to remain in flux
  // over time.
  SetupForWebpLossless();

  // Note that, currently, images that are originally jpegs are
  // converted to webp lossy regardless of this filter below.
  options()->EnableFilter(RewriteOptions::kConvertToWebpLossless);

  RewriteImage("input type=\"image\"", kContentTypeWebp);
}

TEST_F(ImageRewriteTest, DataUrlTest) {
  DataUrlResource();
}

TEST_F(ImageRewriteTest, AddDimTest) {
  Histogram* rewrite_latency_ok = statistics()->GetHistogram(
      ImageRewriteFilter::kImageRewriteLatencyOkMs);
  Histogram* rewrite_latency_failed = statistics()->GetHistogram(
      ImageRewriteFilter::kImageRewriteLatencyFailedMs);
  rewrite_latency_ok->Clear();
  rewrite_latency_failed->Clear();

  // Make sure optimizable image isn't optimized, but
  // dimensions are inserted.
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  rewrite_driver()->AddFilters();
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng,
                    "", " width=\"100\" height=\"100\"", false, false);
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, rewrite_latency_ok->Count());
  EXPECT_EQ(1, rewrite_latency_failed->Count());

  // Force any image read to be a fetch.
  lru_cache()->Delete(HttpCacheKey(StrCat(kTestDomain, kBikePngFile)));

  // .. Now make sure we cached dimension insertion properly, and can do it
  // without re-fetching the image.
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng,
                    "", " width=\"100\" height=\"100\"", false, false);
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

TEST_F(ImageRewriteTest, NoDimsInNonImg) {
  // As above, only with an icon.  See:
  // https://github.com/pagespeed/mod_pagespeed/issues/629
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  rewrite_driver()->AddFilters();
  GoogleString initial_url = StrCat(kTestDomain, kBikePngFile);
  GoogleString page_url = StrCat(kTestDomain, "test.html");
  AddFileToMockFetcher(initial_url, kBikePngFile, kContentTypePng, 100);

  const char html_boilerplate[] =
      "<link rel='apple-touch-icon-precomposed' sizes='100x100' href='%s'>";
  GoogleString html_input =
      StringPrintf(html_boilerplate, initial_url.c_str());

  ParseUrl(page_url, html_input);

  GoogleString html_expected_output =
      StringPrintf(html_boilerplate, initial_url.c_str());
  EXPECT_EQ(AddHtmlBody(html_expected_output), output_buffer_);
}

TEST_F(ImageRewriteTest, PngToJpeg) {
  TestTranscodeAndOptimizePng(true, " width=\"100\" height=\"100\"",
                              kContentTypeJpeg);
}

TEST_F(ImageRewriteTest, PngToJpegUnhealthy) {
  lru_cache()->set_is_healthy(false);
  TestTranscodeAndOptimizePng(false, "", kContentTypePng);
}

TEST_F(ImageRewriteTest, PngToWebpWithWebpUa) {
  if (RunningOnValgrind()) {
    return;
  }
  // Make sure we convert png to webp if user agent permits.
  // We lower compression quality to ensure the webp is smaller.
  options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->set_image_recompress_quality(85);
  rewrite_driver()->AddFilters();
  SetupForWebp();
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypeWebp,
                    "", " width=\"100\" height=\"100\"", true, false);
  TestConversionVariables(0, 0, 0,   // gif
                          0, 1, 0,   // png
                          0, 0, 0,   // jpg
                          0, 0, 0,   // gif animated
                          true);
}

TEST_F(ImageRewriteTest, PngToWebpWithWebpLaUa) {
  if (RunningOnValgrind()) {
    return;
  }
  // Make sure we convert png to webp if user agent permits.
  // We lower compression quality to ensure the webp is smaller.
  options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->set_image_recompress_quality(85);
  rewrite_driver()->AddFilters();
  SetupForWebpLossless();
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypeWebp,
                    "", " width=\"100\" height=\"100\"", true, false);
  TestConversionVariables(0, 0, 0,   // gif
                          0, 1, 0,   // png
                          0, 0, 0,   // jpg
                          0, 0, 0,   // gif animated
                          true);
}

TEST_F(ImageRewriteTest, PngToWebpWithWebpLaUaAndFlag) {
  if (RunningOnValgrind()) {
    return;
  }
  // Make sure we convert png to webp if user agent permits.
  // We lower compression quality to ensure the webp is smaller.
  options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->EnableFilter(RewriteOptions::kConvertToWebpLossless);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->set_allow_logging_urls_in_log_record(true);
  options()->set_image_recompress_quality(85);
  options()->set_log_background_rewrites(true);
  rewrite_driver()->AddFilters();
  SetupForWebpLossless();

  TestSingleRewrite(kRedbrushAlphaPngFile, kContentTypePng, kContentTypeWebp,
                    "", " width=\"512\" height=\"480\"", true, false);
  TestConversionVariables(0, 0, 0,   // gif
                          0, 1, 0,   // png
                          0, 0, 0,   // jpg
                          0, 0, 0,   // gif animated
                          false);

  // Imge is recompressed but not resized.
  rewrite_driver()->Clear();
  TestBackgroundRewritingLog(
      1, /* rewrite_info_size */
      0, /* rewrite_info_index */
      RewriterApplication::APPLIED_OK, /* status */
      "ic", /* rewrite ID */
      "", /* URL */
      IMAGE_PNG, /* original_type */
      IMAGE_WEBP_LOSSLESS_OR_ALPHA, /* optimized_type */
      115870, /* original_size */
      kIgnoreSize, /* optimized_size */
      true, /* is_recompressed */
      false, /* is_resized */
      512, /* original width */
      480, /* original height */
      false, /* is_resized_using_rendered_dimensions */
      -1, /* resized_width */
      -1 /* resized_height */);
}

// The settings are the same as "PngToWebpWithWebpLaUaAndFlag" except
// WebP lossless user agent. So conversion falls back to PNG.
TEST_F(ImageRewriteTest, PngFallbackToPngLackOfWebpLaUa) {
  if (RunningOnValgrind()) {
    return;
  }

  options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->EnableFilter(RewriteOptions::kConvertToWebpLossless);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->set_allow_logging_urls_in_log_record(true);
  options()->set_image_recompress_quality(85);
  options()->set_log_background_rewrites(true);
  rewrite_driver()->AddFilters();

  TestSingleRewrite(kRedbrushAlphaPngFile, kContentTypePng, kContentTypePng,
                    "", " width=\"512\" height=\"480\"", true, false);
  TestConversionVariables(0, 0, 0,   // gif
                          0, 0, 0,   // png
                          0, 0, 0,   // jpg
                          0, 0, 0,   // gif animated
                          false);
}

TEST_F(ImageRewriteTest, PngToWebpWithWebpLaUaAndFlagTimesOut) {
  if (RunningOnValgrind()) {
    return;
  }
  // Make sure we convert png to webp if user agent permits.
  // We lower compression quality to ensure the webp is smaller.
  options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->EnableFilter(RewriteOptions::kConvertToWebpLossless);
  options()->set_image_recompress_quality(85);
  options()->set_image_webp_timeout_ms(0);
  rewrite_driver()->AddFilters();
  SetupForWebpLossless();
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypeJpeg,
                    "", " width=\"100\" height=\"100\"", true, false);
  TestConversionVariables(0, 0, 0,   // gif
                          1, 0, 0,   // png
                          0, 0, 0,   // jpg
                          0, 0, 0,   // gif animated
                          true);
}

TEST_F(ImageRewriteTest, ImageRewritePreserveURLsOnSoftEnable) {
  // Make sure that the image URL stays the same when optimization is enabled
  // due to core filters.
  options()->SoftEnableFilterForTesting(RewriteOptions::kRecompressPng);
  options()->SoftEnableFilterForTesting(RewriteOptions::kResizeImages);
  options()->set_image_preserve_urls(true);
  rewrite_driver()->AddFilters();
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng,
                    " width=10 height=10",  // initial_dims,
                    " width=10 height=10",  // final_dims,
                    false,   // expect_rewritten
                    false);  // expect_inline
  // The URL wasn't changed but the image should have been compressed and cached
  // anyway (prefetching for IPRO).
  ClearStats();
  GoogleString out_png_url(Encode(kTestDomain, "ic", "0", kBikePngFile, "png"));
  GoogleString out_png;
  EXPECT_TRUE(FetchResourceUrl(out_png_url, &out_png));
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, static_cast<int>(lru_cache()->num_hits()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_misses()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_inserts()));

  // Make sure that we didn't resize (original image is 100x100).
  scoped_ptr<Image> image(
      NewImage(out_png, out_png_url, server_context_->filename_prefix(),
               new Image::CompressionOptions(),
               timer(), &message_handler_));
  ImageDim image_dim;
  image->Dimensions(&image_dim);
  EXPECT_EQ(100, image_dim.width());
  EXPECT_EQ(100, image_dim.height());
}

TEST_F(ImageRewriteTest, ImageRewritePreserveURLsExplicitResizeOn) {
  // Explicitly enabling resize_images is a strong signal from the user that
  // it's OK to rename image URLs, so go ahead and do it in the image rewriter.
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->set_image_preserve_urls(true);  // Explicit filters override.
  rewrite_driver()->AddFilters();
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng,
                    " width=10 height=10",  // initial_dims,
                    " width=10 height=10",  // final_dims,
                    true,    // expect_rewritten: explicit cache_extend_images
                    false);  // expect_inline
  ClearStats();
  GoogleString out_png_url(StrCat(
      kTestDomain, EncodeImage(10, 10, kBikePngFile, "0", "png")));

  GoogleString out_png;
  EXPECT_TRUE(FetchResourceUrl(out_png_url, &out_png));
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, static_cast<int>(lru_cache()->num_hits()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_misses()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_inserts()));

  // Make sure that we did the resize to 10x10 from 100x100.
  scoped_ptr<Image> image(
      NewImage(out_png, out_png_url, server_context_->filename_prefix(),
               new Image::CompressionOptions(),
               timer(), &message_handler_));
  ImageDim image_dim;
  image->Dimensions(&image_dim);
  EXPECT_EQ(10, image_dim.width());
  EXPECT_EQ(10, image_dim.height());
}

TEST_F(ImageRewriteTest, ImageRewritePreserveURLsDisablePreemptiveRewrite) {
  // Make sure that the image URL stays the same.
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->set_image_preserve_urls(true);
  options()->set_in_place_preemptive_rewrite_images(false);
  rewrite_driver()->AddFilters();
  ClearStats();
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng,
                    " width=10 height=10",  // initial_dims,
                    " width=10 height=10",  // final_dims,
                    false,   // expect_rewritten
                    false);  // expect_inline

  // We should not have attempted any rewriting.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_hits()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_misses()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_inserts()));

  // But, a direct fetch should work.
  ClearStats();
  GoogleString out_png_url(Encode(kTestDomain, "ic", "0", kBikePngFile, "png"));
  GoogleString out_png;
  EXPECT_TRUE(FetchResourceUrl(out_png_url, &out_png));
  // Make sure that we didn't resize (original image is 100x100).
  scoped_ptr<Image> image(
      NewImage(out_png, out_png_url, server_context_->filename_prefix(),
               new Image::CompressionOptions(),
               timer(), &message_handler_));
  ImageDim image_dim;
  image->Dimensions(&image_dim);
  EXPECT_EQ(100, image_dim.width());
  EXPECT_EQ(100, image_dim.height());
}

TEST_F(ImageRewriteTest, ImageRewriteInlinePreserveURLsOnSoftEnable) {
  // Willing to inline large files.
  options()->set_image_inline_max_bytes(1000000);
  options()->SoftEnableFilterForTesting(RewriteOptions::kInlineImages);
  options()->SoftEnableFilterForTesting(RewriteOptions::kInsertImageDimensions);
  options()->SoftEnableFilterForTesting(RewriteOptions::kConvertGifToPng);
  options()->DisableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->set_image_preserve_urls(true);
  rewrite_driver()->AddFilters();
  const char kResizedDims[] = " width=48 height=64";
  // File would be inlined without preserve urls, make sure it's not,
  // because turning on image_preserve_urls overrides the implicit filter
  // selection from Core filters.
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    kResizedDims, kResizedDims,
                    false,   // expect_rewritten
                    false);  // expect_inline
  // The optimized file should be in the cache now.
  ClearStats();
  GoogleString out_gif_url = Encode(kTestDomain, "ic", "0", kChefGifFile,
                                    "png");
  GoogleString out_gif;
  EXPECT_TRUE(FetchResourceUrl(out_gif_url, &out_gif));
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, static_cast<int>(lru_cache()->num_hits()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_misses()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_inserts()));
}

TEST_F(ImageRewriteTest, ImageRewriteInlinePreserveURLsExplicit) {
  // Willing to inline large files.
  options()->set_image_inline_max_bytes(1000000);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  options()->set_image_preserve_urls(true);
  rewrite_driver()->AddFilters();
  const char kResizedDims[] = " width=48 height=64";
  // In this case, since we have explicitly requested inline images,
  // we will get them despite the preserve URLs setting.
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypePng,
                    kResizedDims, kResizedDims,
                    true,   // expect_rewritten
                    true);  // expect_inline
  // The optimized file should be in the cache now.
  ClearStats();
  GoogleString out_gif_url = Encode(kTestDomain, "ic", "0", kChefGifFile,
                                    "png");
  GoogleString out_gif;
  EXPECT_TRUE(FetchResourceUrl(out_gif_url, &out_gif));
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, static_cast<int>(lru_cache()->num_hits()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_misses()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_inserts()));
}

TEST_F(ImageRewriteTest, NoTransform) {
  // Make sure that the image stays the same and that the attribute is stripped.
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  rewrite_driver()->AddFilters();
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng,
                    " pagespeed_no_transform",      // initial attributes
                    "",                             // final attributes
                    false,   // expect_rewritten
                    false);  // expect_inline
}

TEST_F(ImageRewriteTest, DataNoTransform) {
  // Make sure that the image stays the same and that the attribute is stripped.
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  rewrite_driver()->AddFilters();
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng,
                    " data-pagespeed-no-transform",  // initial attributes
                    "",                              // final attributes
                    false,   // expect_rewritten
                    false);  // expect_inline
}

TEST_F(ImageRewriteTest, NoTransformWithDims) {
  // Make sure that the image stays the same and that the attribute is stripped.
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  rewrite_driver()->AddFilters();
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng,
                    // initial attributes
                    " width=10 height=10 data-pagespeed-no-transform",
                    " width=10 height=10",  // final attributes
                    false,   // expect_rewritten
                    false);  // expect_inline
}

TEST_F(ImageRewriteTest, ImageRewriteDropAll) {
  // Test that randomized optimization doesn't rewrite when drop % set to 100
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->set_rewrite_random_drop_percentage(100);
  rewrite_driver()->AddFilters();

  for (int i = 0; i < 100; ++i) {
    TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng,
                      "",      // initial attributes
                      "",      // final attributes
                      false,   // expect_rewritten
                      false);  // expect_inline
    lru_cache()->Clear();
    ClearStats();
  }
  // Try some rewrites without clearing the cache to make sure that that
  // works too.
  for (int i = 0; i < 100; ++i) {
    TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng,
                      "",      // initial attributes
                      "",      // final attributes
                      false,   // expect_rewritten
                      false);  // expect_inline
  }
}

TEST_F(ImageRewriteTest, ImageRewriteDropNone) {
  // Test that randomized optimization always rewrites when drop % set to 0.
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->set_rewrite_random_drop_percentage(0);
  rewrite_driver()->AddFilters();

  for (int i = 0; i < 100; ++i) {
    TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng,
                      "",      // initial attributes
                      "",      // final attributes
                      true,   // expect_rewritten
                      false);  // expect_inline
    lru_cache()->Clear();
    ClearStats();
  }
  // Try some rewrites without clearing the cache to make sure that that
  // works too.
  for (int i = 0; i < 5; ++i) {
    TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng,
                      "",      // initial attributes
                      "",      // final attributes
                      true,   // expect_rewritten
                      false);  // expect_inline
  }
}

TEST_F(ImageRewriteTest, ImageRewriteDropSometimes) {
  // Test that randomized optimization sometimes rewrites and sometimes doesn't.
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->set_rewrite_random_drop_percentage(50);
  rewrite_driver()->AddFilters();

  bool found_rewritten = false;
  bool found_not_rewritten = false;

  // Boiler-plate fetching stuff.
  GoogleString initial_url = StrCat(kTestDomain, kBikePngFile);
  GoogleString page_url = StrCat(kTestDomain, "test.html");
  AddFileToMockFetcher(initial_url, kBikePngFile, kContentTypePng, 100);
  const char html_boilerplate[] = "<img src='%s'%s>";
  GoogleString html_input =
      StringPrintf(html_boilerplate, initial_url.c_str(), "");

  // Note that this could flake, but for it to flake we'd have to have 100
  // heads or 100 tails in a row, a probability of 1.6e-30 when
  // image_rewrite_percentage is 50.
  for (int i = 0; i < 100; i++) {
    ParseUrl(page_url, html_input);

    // Check for single image file in the rewritten page.
    StringVector image_urls;
    CollectImgSrcs(initial_url, output_buffer_, &image_urls);
    EXPECT_EQ(1, image_urls.size());
    const GoogleString& rewritten_url = image_urls[0];
    const GoogleUrl rewritten_gurl(rewritten_url);
    EXPECT_TRUE(rewritten_gurl.IsWebValid());

    if (initial_url == rewritten_url) {
      found_not_rewritten = true;
    } else {
      found_rewritten = true;
    }

    if (found_rewritten && found_not_rewritten) {
      break;
    }
  }
}

// For Issue 748 where duplicate images in the same document with RandomDrop on
// caused the duplicate urls to be removed if the first image is not optimized.
// NOTE: This test only works if the first image is deterministically dropped.
// We set the drop_percentage to 100 to guarantee that.
TEST_F(ImageRewriteTest, ImageRewriteRandomDropRepeatedImages) {
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->set_rewrite_random_drop_percentage(100);
  rewrite_driver()->AddFilters();
  GoogleString initial_url = StrCat(kTestDomain, kBikePngFile);
  GoogleString page_url = StrCat(kTestDomain, "test.html");
  AddFileToMockFetcher(initial_url, kBikePngFile, kContentTypePng, 100);
  const char html_boilerplate[] =
      "<img src='%s'> <img src='%s'> <img src='%s'>";
  GoogleString html_input =
      StringPrintf(html_boilerplate, initial_url.c_str(), initial_url.c_str(),
                   initial_url.c_str());
  ParseUrl(page_url, html_input);
  StringVector image_urls;
  CollectImgSrcs(initial_url, output_buffer_, &image_urls);
  EXPECT_EQ(3, image_urls.size());
  EXPECT_EQ(initial_url, image_urls[0]);
  EXPECT_EQ(initial_url, image_urls[1]);
  EXPECT_EQ(initial_url, image_urls[2]);
}

TEST_F(ImageRewriteTest, ResizeTest) {
  // Make sure we resize images, but don't optimize them in place.
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  // Without explicit resizing, we leave the image alone.
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    "", "", false, false);
  // With resizing, we optimize.
  const char kResizedDims[] = " width=\"256\" height=\"192\"";
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    kResizedDims, kResizedDims, true, false);
}

TEST_F(ImageRewriteTest, ResizeIsReallyPrefetch) {
  // Make sure we don't resize a large image to 1x1, as it's
  // really an image prefetch request.
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    kPixelDims, kPixelDims, false, false);
}

TEST_F(ImageRewriteTest, OptimizeRequestedPrefetch) {
  // We shouldn't resize this image, but we should optimize it.
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  rewrite_driver()->AddFilters();
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    kPixelDims, kPixelDims, true, false);
}

TEST_F(ImageRewriteTest, ResizeHigherDimensionTest) {
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  const char kOriginalDims[] = " width=\"100000\" height=\"100000\"";
  TestSingleRewrite(kLargePngFile, kContentTypePng, kContentTypePng,
                    kOriginalDims, kOriginalDims, false, false);
  Variable* no_rewrites = statistics()->GetVariable(
      ImageRewriteFilter::kImageNoRewritesHighResolution);
  EXPECT_EQ(1, no_rewrites->Get());
}

TEST_F(ImageRewriteTest, DimensionParsingOK) {
  // First some tests that should succeed.
  int value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute("5", &value));
  EXPECT_EQ(value, 5);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(" 341  ", &value));
  EXPECT_EQ(value, 341);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(" 000743  ", &value));
  EXPECT_EQ(value, 743);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute("\n\r\t \f62",
                                                          &value));
  EXPECT_EQ(value, 62);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute("+40", &value));
  EXPECT_EQ(value, 40);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(" +41", &value));
  EXPECT_EQ(value, 41);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute("54px", &value));
  EXPECT_EQ(value, 54);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute("  70.", &value));
  EXPECT_EQ(value, 70);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute("71.3", &value));
  EXPECT_EQ(value, 71);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute("71.523", &value));
  EXPECT_EQ(value, 72);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(
      "73.4999990982589729048572938579287459874", &value));
  EXPECT_EQ(value, 73);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute("75.px", &value));
  EXPECT_EQ(value, 75);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute("75.6 px", &value));
  EXPECT_EQ(value, 76);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute("77.34px", &value));
  EXPECT_EQ(value, 77);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute("78px ", &value));
  EXPECT_EQ(value, 78);
}

TEST_F(ImageRewriteTest, DimensionParsingFail) {
  int value = -34;
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("0", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("+0", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "+0.9", &value));  // Bizarrely not allowed!
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("  0  ", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("junk5", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("  junk10", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("junk  50", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("-43", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("+ 43", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("21px%", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("21px junk",
                                                           &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "9123948572038209720561049018365037891046", &value));
  EXPECT_EQ(-34, value);
  // We don't handle percentages because we can't resize them.
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("73%", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("43.2 %", &value));
  EXPECT_EQ(-34, value);
  // Trailing junk OK according to spec, but older browsers flunk / treat
  // inconsistently
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "5junk", &value));  // Doesn't ignore
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "25p%x", &value));  // 25% on FF9!
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "26px%", &value));  // 25% on FF9!
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "45 643", &value));  // 45 today
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("21%px", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("59 .", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "60 . 9", &value));  // 60 today
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "+61. 9", &value));  // 61 today
  EXPECT_EQ(-34, value);
  // Some other units that some old browsers treat as px, but we just ignore
  // to avoid confusion / inconsistency.
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("29in", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("30cm", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute("43pt", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "99em", &value));  // FF9 screws this up
  EXPECT_EQ(-34, value);
}

TEST_F(ImageRewriteTest, ResizeWidthOnly) {
  // Make sure we resize images, but don't optimize them in place.
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  // Without explicit resizing, we leave the image alone.
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    "", "", false, false);
  // With resizing, we optimize.
  const char kResizedDims[] = " width=\"256\"";
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    kResizedDims, kResizedDims, true, false);
}

TEST_F(ImageRewriteTest, ResizeHeightOnly) {
  // Make sure we resize images, but don't optimize them in place.
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  // Without explicit resizing, we leave the image alone.
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    "", "", false, false);
  // With resizing, we optimize.
  const char kResizedDims[] = " height=\"192\"";
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    kResizedDims, kResizedDims, true, false);
}

TEST_F(ImageRewriteTest, ResizeHeightRounding) {
  // Make sure fractional heights are rounded.  We used to truncate, but this
  // didn't match WebKit's behavior.  To check this we need to fetch the resized
  // image and verify its dimensions.  The original image is 1023 x 766.
  const char kLeafNoHeight[] = "256xNxPuzzle.jpg.pagespeed.ic.0.jpg";
  TestDimensionRounding(kLeafNoHeight, 256, 192);
}

TEST_F(ImageRewriteTest, ResizeWidthRounding) {
  // Make sure fractional widths are rounded, as above (with the same image).
  const char kLeafNoWidth[] = "Nx383xPuzzle.jpg.pagespeed.ic.0.jpg";
  TestDimensionRounding(kLeafNoWidth, 512, 383);
}

TEST_F(ImageRewriteTest, ResizeStyleTest) {
  // Make sure we resize images, but don't optimize them in place.
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  const char kResizedDims[] = " style=\"width:256px;height:192px;\"";
  // Without explicit resizing, we leave the image alone.
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    "", "", false, false);
  // With resizing, we optimize.
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    kResizedDims, kResizedDims, true, false);

  const char kMixedDims[] = " width=\"256\" style=\"height:192px;\"";
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    kMixedDims, kMixedDims, true, false);

  const char kMoreMixedDims[] =
      " height=\"197\" style=\"width:256px;broken:true;\"";
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    kMoreMixedDims, kMoreMixedDims, true, false);

  const char kNonPixelDims[] =
      " style=\"width:256cm;height:192cm;\"";
    TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                      kNonPixelDims, kNonPixelDims, false, false);

  const char kNoDims[] =
      " style=\"width:256;height:192;\"";
    TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                      kNoDims, kNoDims, false, false);
}

TEST_F(ImageRewriteTest, ResizeWithPxInHtml) {
  // Make sure we resize images if the html width and/or height specifies px.
  // We rely on ImageRewriteTest.DimensionParsing above to test all the
  // corner cases we might encounter and to cross-check the numbers.
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  // Things that ought to work (ie result in resizing)
  const char kResizedPx[] = " width='256px' height='192px'";
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    kResizedPx, kResizedPx, true, false);
  const char kResizedWidthDot[] = " width='256.'";
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    kResizedWidthDot, kResizedWidthDot, true, false);
  const char kResizedWidthDec[] = " width='255.536'";
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    kResizedWidthDec, kResizedWidthDec, true, false);
  const char kResizedWidthPx[] = " width='256px'";
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    kResizedWidthPx, kResizedWidthPx, true, false);
  const char kResizedWidthPxDot[] = " width='256.px'";
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    kResizedWidthPxDot, kResizedWidthPxDot, true, false);
  const char kResizedWidthPxDec[] = " width='255.5px'";
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    kResizedWidthPxDec, kResizedWidthPxDec, true, false);
  const char kResizedSpacePx[] = " width='256  px'";
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    kResizedSpacePx, kResizedSpacePx, true, false);
  // Things that ought not to work (ie not result in resizing)
  const char kResizedJunk[] = " width='256earths' height='192earths'";
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    kResizedJunk, kResizedJunk, false, false);
  const char kResizedPercent[] = " width='20%' height='20%'";
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    kResizedPercent, kResizedPercent, false, false);
}

TEST_F(ImageRewriteTest, NullResizeTest) {
  // Make sure we don't crash on a value-less style attribute.
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    " style", " style", false, false);
}

TEST_F(ImageRewriteTest, DebugResizeTest) {
  options()->EnableFilter(RewriteOptions::kDebug);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  const char kResizedDims[] = " width=\"256\" height=\"192\"";
  GoogleString initial_url = StrCat(kTestDomain, kPuzzleJpgFile);
  GoogleString page_url = StrCat(kTestDomain, "test.html");
  AddFileToMockFetcher(initial_url, kPuzzleJpgFile, kContentTypeJpeg, 100);
  const char html_boilerplate[] = "<img src='%s'%s>";
  GoogleString html_input =
      StringPrintf(html_boilerplate, initial_url.c_str(), kResizedDims);
  ParseUrl(page_url, html_input);
  EXPECT_THAT(
      output_buffer_,
      testing::HasSubstr(
          "<!--Resized image http://test.com/Puzzle.jpg "
          "from 1023x766 to 256x192-->"));
}

TEST_F(ImageRewriteTest, DebugNoResizeTest) {
  options()->EnableFilter(RewriteOptions::kDebug);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  GoogleString initial_url = StrCat(kTestDomain, kPuzzleJpgFile);
  GoogleString page_url = StrCat(kTestDomain, "test.html");
  AddFileToMockFetcher(initial_url, kPuzzleJpgFile, kContentTypeJpeg, 100);
  const char html_boilerplate[] = "<img src='%s'>";
  GoogleString html_input = StringPrintf(html_boilerplate, initial_url.c_str());
  ParseUrl(page_url, html_input);
  EXPECT_THAT(
      output_buffer_,
      testing::HasSubstr(
          "<!--Image http://test.com/Puzzle.jpg "
          "does not appear to need resizing.-->"));
}

TEST_F(ImageRewriteTest, DebugWithMapRewriteDomain) {
  options()->EnableFilter(RewriteOptions::kDebug);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kRewriteDomains);
  options()->WriteableDomainLawyer()->AddRewriteDomainMapping(
      "external.example.com", kTestDomain, message_handler());
  rewrite_driver()->AddFilters();
  GoogleString initial_url = StrCat(kTestDomain, kPuzzleJpgFile);
  GoogleString page_url = StrCat(kTestDomain, "test.html");
  AddFileToMockFetcher(initial_url, kPuzzleJpgFile, kContentTypeJpeg, 100);
  const char html_boilerplate[] = "<img src='%s'>";
  GoogleString html_input = StringPrintf(html_boilerplate, initial_url.c_str());
  ParseUrl(page_url, html_input);
  EXPECT_THAT(
      output_buffer_,
      testing::HasSubstr(
          "<img src='http://external.example.com/Puzzle.jpg'>"
          "<!--Image http://external.example.com/Puzzle.jpg does "
          "not appear to need resizing.-->"));
}

TEST_F(ImageRewriteTest, DebugWithMapRewriteDomainOptOnly) {
  // w/o rewrite_domains we don't touch the URLs in comments, as they can be
  // left as such in the page source proper anyway.
  options()->EnableFilter(RewriteOptions::kDebug);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->WriteableDomainLawyer()->AddRewriteDomainMapping(
      "external.example.com", kTestDomain, message_handler());
  rewrite_driver()->AddFilters();
  GoogleString initial_url = StrCat(kTestDomain, kPuzzleJpgFile);
  GoogleString page_url = StrCat(kTestDomain, "test.html");
  AddFileToMockFetcher(initial_url, kPuzzleJpgFile, kContentTypeJpeg, 100);
  const char html_boilerplate[] = "<img src='%s'>";
  GoogleString html_input = StringPrintf(html_boilerplate, initial_url.c_str());
  ParseUrl(page_url, html_input);
  EXPECT_THAT(
      output_buffer_,
      testing::HasSubstr(
          "<img src='http://test.com/Puzzle.jpg'>"
          "<!--Image http://test.com/Puzzle.jpg does "
          "not appear to need resizing.-->"));
}

TEST_F(ImageRewriteTest, TestLoggingWithoutOptimize) {
  // Make sure we don't resize, if we don't optimize.
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  rewrite_driver()->AddFilters();
  SetMockLogRecord();
  MockLogRecord* log = mock_log_record();
  EXPECT_CALL(*log,
              MockLogImageRewriteActivity(LogImageRewriteActivityMatcher(
                  StrEq("ic"),
                  StrEq("http://test.com/IronChef2.gif"),
                  RewriterApplication::NOT_APPLIED,
                  false /* is_image_inlined */,
                  true /* is_critical_image */,
                  false /* is_url_rewritten */,
                  24941 /* original size */,
                  false /* try_low_res_src_insertion */,
                  false /* low_res_src_inserted */,
                  IMAGE_UNKNOWN /* low res image type */,
                  _ /* low_res_data_size */)));
  // Without resize, it's not optimizable.
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    "", kChefDims, false, false);
}

TEST_F(ImageRewriteTest, TestLoggingWithOptimize) {
  options()->set_image_inline_max_bytes(10000);
  options()->set_log_url_indices(true);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  options()->set_log_background_rewrites(true);
  rewrite_driver()->AddFilters();
  const char kResizedDims[] = " width=48 height=64";
  SetMockLogRecord();
  MockLogRecord* log = mock_log_record();
  EXPECT_CALL(*log,
              MockLogImageRewriteActivity(LogImageRewriteActivityMatcher(
                  StrEq("ic"),
                  StrEq("http://test.com/IronChef2.gif"),
                  RewriterApplication::APPLIED_OK,
                  true /* is_image_inlined */,
                  true /* is_critical_image */,
                  true /* is_url_rewritten */,
                  5735 /* rewritten size */,
                  false /* try_low_res_src_insertion */,
                  false /* low_res_src_inserted */,
                  IMAGE_UNKNOWN /* low res image type */,
                  _ /* low_res_data_size */)));
  // Without resize, it's not optimizable.
  // With resize, the image shrinks quite a bit, and we can inline it
  // given the 10K threshold explicitly set above.  This also strips the
  // size information, which is now embedded in the image itself anyway.
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypePng,
                    kResizedDims, "", true, true);
}

TEST_F(ImageRewriteTest, InlineTestWithoutOptimize) {
  // Make sure we don't resize, if we don't optimize.
  options()->set_allow_logging_urls_in_log_record(true);
  options()->set_image_inline_max_bytes(10000);
  options()->set_log_background_rewrites(true);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  rewrite_driver()->AddFilters();
  // Without resize, it's not optimizable.
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    "", kChefDims, false, false);

  // No optimization has been applied. Image type and size are not changed,
  // so the optimzied image does not have these values logged.
  rewrite_driver()->Clear();
  TestBackgroundRewritingLog(
      1, /* rewrite_info_size */
      0, /* rewrite_info_index */
      RewriterApplication::NOT_APPLIED, /* status */
      "ic", /* ID */
      "http://test.com/IronChef2.gif", /* URL */
      IMAGE_GIF, /* original_type */
      IMAGE_UNKNOWN, /* optimized_type */
      24941, /* original_size */
      0, /* optimized_size */
      false, /* is_recompressed */
      false, /* is_resized */
      192, /* original width */
      256, /* original height */
      false, /* is_resized_using_rendered_dimensions */
      -1, /* resized_width */
      -1 /* resized_height */);
}

TEST_F(ImageRewriteTest, InlineTestWithResizeWithOptimize) {
  options()->set_image_inline_max_bytes(10000);
  options()->set_log_url_indices(true);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  options()->set_log_background_rewrites(true);
  rewrite_driver()->AddFilters();
  const char kResizedDims[] = " width=48 height=64";
  // Without resize, it's not optimizable.
  // With resize, the image shrinks quite a bit, and we can inline it
  // given the 10K threshold explicitly set above.  This also strips the
  // size information, which is now embedded in the image itself anyway.
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypePng,
                    kResizedDims, "", true, true);

  // After optimization, the GIF image is converted to a PNG image.
  rewrite_driver()->Clear();
  TestBackgroundRewritingLog(
      1, /* rewrite_info_size */
      0, /* rewrite_info_index */
      RewriterApplication::APPLIED_OK, /* status */
      "ic", /* ID */
      "", /* URL */
      IMAGE_GIF, /* original_type */
      IMAGE_PNG, /* optimized_type */
      24941, /* original_size */
      5735, /* optimized_size */
      true, /* is_recompressed */
      true, /* is_resized */
      192, /* original width */
      256, /* original height */
      false, /* is_resized_using_rendered_dimensions */
      48, /* resized_width */
      64 /* resized_height */);
}

TEST_F(ImageRewriteTest, InlineTestWithResizeKeepDims) {
  // their dimensions when we inline.
  options()->set_image_inline_max_bytes(10000);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  options()->EnableFilter(RewriteOptions::kDebug);
  rewrite_driver()->AddFilters();

  GoogleString initial_url = StrCat(kTestDomain, kChefGifFile);
  GoogleString page_url = StrCat(kTestDomain, "test.html");
  AddFileToMockFetcher(initial_url, kChefGifFile, kContentTypeGif, 100);
  const char kResizedDims[] = " width=48 height=64";
  const char html_boilerplate[] = "<td background='%s'%s></td>";
  GoogleString html_input =
      StringPrintf(html_boilerplate, initial_url.c_str(), kResizedDims);
  ParseUrl(page_url, html_input);
  // Image should have been resized
  EXPECT_THAT(
      output_buffer_,
      testing::HasSubstr(
          "<!--Resized image http://test.com/IronChef2.gif "
          "from 192x256 to 48x64-->"));
  // And inlined
  EXPECT_THAT(
      output_buffer_,
      testing::HasSubstr("<td background='data:"));
  // But dimensions should still be there.
  EXPECT_THAT(
      output_buffer_,
      testing::HasSubstr(kResizedDims));
}

TEST_F(ImageRewriteTest, InlineTestWithResizeWithOptimizeAndUrlLogging) {
  options()->set_image_inline_max_bytes(10000);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  options()->set_allow_logging_urls_in_log_record(true);
  rewrite_driver()->AddFilters();

  const char kResizedDims[] = " width=48 height=64";
  // Without resize, it's not optimizable.
  // With resize, the image shrinks quite a bit, and we can inline it
  // given the 10K threshold explicitly set above.  This also strips the
  // size information, which is now embedded in the image itself anyway.
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypePng,
                    kResizedDims, "", true, true);
  TestSingleRewriteWithoutAbs(kChefGifFile, kChefGifFile, kContentTypeGif,
                              kContentTypePng, kResizedDims, "", true, true);
}

TEST_F(ImageRewriteTest, DimensionStripAfterInline) {
  options()->set_image_inline_max_bytes(100000);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  rewrite_driver()->AddFilters();
  const char kChefWidth[] = " width=192";
  const char kChefHeight[] = " height=256";
  // With all specified dimensions matching, dims are stripped after inlining.
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    kChefDims, "", false, true);
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    kChefWidth, "", false, true);
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    kChefHeight, "", false, true);
  // If we stretch the image in either dimension, we keep the dimensions.
  const char kChefWider[] = " width=384 height=256";
  const char kChefTaller[] = " width=192 height=512";
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    kChefWider, kChefWider, false, true);
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    kChefTaller, kChefTaller, false, true);

  const char kChefWidthWithPercentage[] = " width=100% height=1";
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    kChefWidthWithPercentage, kChefWidthWithPercentage,
                    false, true);
  const char kChefHeightWithPercentage[] = " width=1 height=%";
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    kChefHeightWithPercentage, kChefHeightWithPercentage,
                    false, true);
}

TEST_F(ImageRewriteTest, InlineCriticalOnly) {
  MockCriticalImagesFinder* finder = new MockCriticalImagesFinder(statistics());
  server_context()->set_critical_images_finder(finder);
  options()->set_image_inline_max_bytes(30000);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  rewrite_driver()->AddFilters();
  // With no critical images registered, no images are candidates for inlining.
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    "", "", false, false);
  // Here and below, -1 results mean "no critical image data reported".
  EXPECT_EQ(-1, logging_info()->num_html_critical_images());
  EXPECT_EQ(-1, logging_info()->num_css_critical_images());

  // Image not present in critical set should not be inlined.
  StringSet* critical_images = new StringSet;
  critical_images->insert(StrCat(kTestDomain, "other_image.png"));
  finder->set_critical_images(critical_images);

  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    "", "", false, false);
  EXPECT_EQ(-1, logging_info()->num_html_critical_images());
  EXPECT_EQ(-1, logging_info()->num_css_critical_images());

  // Image present in critical set should be inlined.
  critical_images->insert(StrCat(kTestDomain, kChefGifFile));
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    "", "", false, true);
  EXPECT_EQ(-1, logging_info()->num_html_critical_images());
  EXPECT_EQ(-1, logging_info()->num_css_critical_images());
}

TEST_F(ImageRewriteTest, InlineNoRewrite) {
  // Make sure we inline an image that isn't otherwise altered in any way.
  options()->set_image_inline_max_bytes(30000);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  rewrite_driver()->AddFilters();
  // This image is just small enough to inline, which also erases
  // dimension information.
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    kChefDims, "", false, true);
  // This image is too big to inline, and we don't insert missing
  // dimension information because that is not explicitly enabled.
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                    "", "", false, false);
}

TEST_F(ImageRewriteTest, InlineNoResize) {
  // Make sure we inline an image if it meets the inlining threshold but can't
  // be resized.  Make sure we retain sizing information when this happens.
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kRecompressWebp);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  const char kOrigDims[] = " width=24 height=24";
  const char kResizedDims[] = " width=20 height=12";
  // At natural size, we should inline and erase dimensions.
  TestSingleRewrite(kChromium24, kContentTypeWebp, kContentTypeWebp,
                    kOrigDims, "", false, true);
  // Image is inlined but not resized, so preserve dimensions.
  TestSingleRewrite(kChromium24, kContentTypeWebp, kContentTypeWebp,
                    kResizedDims, kResizedDims, false, true);
}

TEST_F(ImageRewriteTest, InlineLargerResize) {
  // Make sure we inline an image if it meets the inlining threshold before
  // resize, resizing succeeds, but the resulting image is larger than the
  // original.  Make sure we retain sizing information when this happens.
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  const char kOrigDims[] = " width=65 height=70";
  const char kResizedDims[] = " width=64 height=69";
  // At natural size, we should inline and erase dimensions.
  TestSingleRewrite(kCuppaOPngFile, kContentTypePng, kContentTypePng,
                    kOrigDims, "", false, true);
  // Image is inlined but not resized, so preserve dimensions.
  TestSingleRewrite(kCuppaOPngFile, kContentTypePng, kContentTypePng,
                    kResizedDims, kResizedDims, false, true);
}

TEST_F(ImageRewriteTest, ResizeTransparentImage) {
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  const char kResizedDims[] = " width=26 height=28";
  // Image is resized and inlined.
  TestSingleRewrite(kCuppaTPngFile, kContentTypePng, kContentTypePng,
                    kResizedDims, "", true, true);
}

TEST_F(ImageRewriteTest, InlineEnlargedImage) {
  // Make sure we inline an image that meets the inlining threshold,
  // but retain its sizing information if the image has been enlarged.
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  rewrite_driver()->AddFilters();
  const char kDoubledDims[] = " width=130 height=140";
  TestSingleRewrite(kCuppaOPngFile, kContentTypePng, kContentTypePng,
                    kDoubledDims, kDoubledDims, false, true);
}

TEST_F(ImageRewriteTest, RespectsBaseUrl) {
  // Put original files into our fetcher.
  static const char kHtmlUrl[] = "http://image.test/base_url.html";
  static const char kPngUrl[]  = "http://other_domain.test/foo/bar/a.png";
  static const char kJpegUrl[] = "http://other_domain.test/baz/b.jpeg";
  static const char kGifUrl[]  = "http://other_domain.test/foo/c.gif";

  AddFileToMockFetcher(kPngUrl, kBikePngFile, kContentTypePng, 100);
  AddFileToMockFetcher(kJpegUrl, kPuzzleJpgFile, kContentTypeJpeg, 100);
  AddFileToMockFetcher(kGifUrl, kChefGifFile, kContentTypeGif, 100);

  // First two images are on base domain.  Last is on origin domain.
  const char html_format[] =
      "<head>\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "</head>\n"
      "<body>\n"
      "  <img src='%s'>\n"
      "  <img src='%s'>\n"
      "  <img src='%s'>\n"
      "</body>";

  GoogleString html_input =
      StringPrintf(html_format, "bar/a.png", "/baz/b.jpeg", "c.gif");

  // Rewrite
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  rewrite_driver()->AddFilters();
  ParseUrl(kHtmlUrl, html_input);

  // Check for image files in the rewritten page.
  StringVector image_urls;
  CollectImgSrcs("base_url-links", output_buffer_, &image_urls);
  EXPECT_EQ(3UL, image_urls.size());
  const GoogleString& new_png_url = image_urls[0];
  const GoogleString& new_jpeg_url = image_urls[1];
  const GoogleString& new_gif_url = image_urls[2];

  // Sanity check that we changed the URL.
  EXPECT_NE("bar/a.png", new_png_url);
  EXPECT_NE("/baz/b.jpeg", new_jpeg_url);
  EXPECT_NE("c.gif", new_gif_url);

  GoogleString expected_output =
      StringPrintf(html_format, new_png_url.c_str(),
                   new_jpeg_url.c_str(), new_gif_url.c_str());

  EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);

  GoogleUrl base_gurl("http://other_domain.test/foo/");
  GoogleUrl new_png_gurl(base_gurl, new_png_url);
  EXPECT_TRUE(new_png_gurl.IsWebValid());
  GoogleUrl encoded_png_gurl(EncodeWithBase("http://other_domain.test/",
                                            "http://other_domain.test/foo/bar/",
                                            "x", "0", "a.png", "x"));
  EXPECT_EQ(encoded_png_gurl.AllExceptLeaf(), new_png_gurl.AllExceptLeaf());

  GoogleUrl new_jpeg_gurl(base_gurl, new_jpeg_url);
  EXPECT_TRUE(new_jpeg_gurl.IsWebValid());
  GoogleUrl encoded_jpeg_gurl(EncodeWithBase("http://other_domain.test/",
                                             "http://other_domain.test/baz/",
                                             "x", "0", "b.jpeg", "x"));
  EXPECT_EQ(encoded_jpeg_gurl.AllExceptLeaf(), new_jpeg_gurl.AllExceptLeaf());

  GoogleUrl new_gif_gurl(base_gurl, new_gif_url);
  EXPECT_TRUE(new_gif_gurl.IsWebValid());
  GoogleUrl encoded_gif_gurl(EncodeWithBase("http://other_domain.test/",
                                            "http://other_domain.test/foo/",
                                            "x", "0", "c.gif", "x"));
  EXPECT_EQ(encoded_gif_gurl.AllExceptLeaf(), new_gif_gurl.AllExceptLeaf());
}

TEST_F(ImageRewriteTest, FetchInvalid) {
  // Make sure that fetching invalid URLs cleanly reports a problem by
  // calling Done(false).
  AddFilter(RewriteOptions::kRecompressJpeg);
  GoogleString out;

  // We are trying to test with an invalid encoding. By construction,
  // Encode cannot make an invalid encoding.  However we can make one
  // using a PlaceHolder string and then mutating it.
  const char kPlaceholder[] = "PlaceHolder";
  GoogleString encoded_url = Encode("http://www.example.com/", "ic",
                                    "ABCDEFGHIJ", kPlaceholder, "jpg");
  GlobalReplaceSubstring(kPlaceholder, "70x53x,", &encoded_url);
  EXPECT_FALSE(FetchResourceUrl(encoded_url, &out));
}

TEST_F(ImageRewriteTest, Rewrite404) {
  // Make sure we don't fail when rewriting with invalid input.
  SetFetchResponse404("404.jpg");
  AddFilter(RewriteOptions::kRecompressJpeg);
  DebugWithMessage("<!--4xx status code, preventing rewriting of %url%-->");
  for (int i = 0; i < 2; ++i) {
    // Try twice to exercise the cached case.
    ValidateExpected(
        "404",
        "<img src='404.jpg'>",
        StrCat("<img src='404.jpg'>", DebugMessage("404.jpg")));
  }
}

TEST_F(ImageRewriteTest, CanonicalOnTimeout) {
  options()->ClearSignatureForTesting();
  options()->set_test_instant_fetch_rewrite_deadline(true);
  server_context()->ComputeSignature(options());

  AddFileToMockFetcher(StrCat(kTestDomain, "a.jpg"), kPuzzleJpgFile,
                        kContentTypeJpeg, 100);

  GoogleString out_url(Encode(kTestDomain, "ic", "0", "a.jpg", "jpg"));
  GoogleString content;
  ResponseHeaders headers;

  EXPECT_EQ(
      0,
      statistics()->GetVariable(RewriteContext::kNumDeadlineAlarmInvocations)
          ->Get());
  EXPECT_TRUE(RewriteTestBase::FetchResourceUrl(out_url, &content, &headers));
  EXPECT_EQ(
      1,
      statistics()->GetVariable(RewriteContext::kNumDeadlineAlarmInvocations)
          ->Get());

  EXPECT_STREQ(ResponseHeaders::RelCanonicalHeaderValue(
                   StrCat(kTestDomain, "a.jpg")),
               headers.Lookup1(HttpAttributes::kLink));

  // Now try with an existing canonical header. That should be preserved
  lru_cache()->Clear();
  AddToResponse(StrCat(kTestDomain, "a.jpg"),
                HttpAttributes::kLink,
                ResponseHeaders::RelCanonicalHeaderValue(
                    StrCat(kTestDomain, "nota.jpg")));
  EXPECT_TRUE(RewriteTestBase::FetchResourceUrl(out_url, &content, &headers));
  EXPECT_STREQ(ResponseHeaders::RelCanonicalHeaderValue(
                   StrCat(kTestDomain, "nota.jpg")),
               headers.Lookup1(HttpAttributes::kLink));
}

TEST_F(ImageRewriteTest, HonorNoTransform) {
  // If cache-control: no-transform then we should serve the original URL
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  rewrite_driver()->AddFilters();
  DebugWithMessage(
      "<!--Cache-control: no-transform, preventing rewriting of %url%-->");

  GoogleString url = StrCat(kTestDomain, "notransform.png");
  AddFileToMockFetcher(url, kBikePngFile, kContentTypePng, 100);
  AddToResponse(url, HttpAttributes::kCacheControl, "no-transform");

  for (int i = 0; i < 2; ++i) {
    // Validate twice in case changes in cache from the first request alter the
    // second.
    ValidateExpected(
        "NoTransform",
        StrCat("<img src=", url, ">"),
        StrCat("<img src=", url, ">", DebugMessage(url)));
  }
}

TEST_F(ImageRewriteTest, YesTransform) {
  // Replicates above test but without no-transform to show that it works.  We
  // also verify that the data-pagespeed-no-defer attribute doesn't get removed
  // when we rewrite images.
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  rewrite_driver()->AddFilters();

  GoogleString url = StrCat(kTestDomain, "notransform.png");
  AddFileToMockFetcher(url, kBikePngFile,
                       kContentTypePng, 100);
  ValidateExpected("YesTransform",
                   StrCat("<img src=", url, " data-pagespeed-no-defer>"),
                   StrCat("<img src=",
                          Encode("http://test.com/", "ic", "0",
                                 "notransform.png", "png"),
                          " data-pagespeed-no-defer>"));
  // Validate twice in case changes in cache from the first request alter the
  // second.
  ValidateExpected("YesTransform", StrCat("<img src=", url, ">"),
                   StrCat("<img src=",
                          Encode("http://test.com/", "ic", "0",
                                 "notransform.png", "png"),
                          ">"));
}

TEST_F(ImageRewriteTest, YesTransformWithOptionFalse) {
  // Verify rewrite happens even when no-transform is set, if the option is
  // set to false.
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->set_disable_rewrite_on_no_transform(false);
  rewrite_driver()->AddFilters();

  GoogleString url = StrCat(kTestDomain, "notransform.png");
  AddFileToMockFetcher(url, kBikePngFile, kContentTypePng, 100);
  AddToResponse(url, HttpAttributes::kCacheControl, "no-transform");
  ValidateExpected("YesTransform", StrCat("<img src=", url, ">"),
                   StrCat("<img src=",
                          Encode("http://test.com/", "ic", "0",
                                 "notransform.png", "png"),
                          ">"));
  // Validate twice in case changes in cache from the first request alter the
  // second.
  ValidateExpected("YesTransform", StrCat("<img src=", url, ">"),
                   StrCat("<img src=",
                          Encode("http://test.com/", "ic", "0",
                                 "notransform.png", "png"),
                          ">"));
}

TEST_F(ImageRewriteTest, NoExtensionCorruption) {
  TestCorruptUrl("%22", true /* append %22 */);
}

TEST_F(ImageRewriteTest, NoQueryCorruption) {
  TestCorruptUrl("?query",  true /* append ?query*/);
}

TEST_F(ImageRewriteTest, NoWrongExtCorruption) {
  TestCorruptUrl(".html", false /* replace ext with .html */);
}

TEST_F(ImageRewriteTest, NoCrashOnInvalidDim) {
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  rewrite_driver()->AddFilters();
  AddFileToMockFetcher(StrCat(kTestDomain, "a.png"), kBikePngFile,
                       kContentTypePng, 100);

  ParseUrl(kTestDomain, "<img width=0 height=0 src=\"a.png\">");
  ParseUrl(kTestDomain, "<img width=0 height=42 src=\"a.png\">");
  ParseUrl(kTestDomain, "<img width=42 height=0 src=\"a.png\">");
  ParseUrl(kTestDomain, "<img width=\"-5\" height=\"5\" src=\"a.png\">");
  ParseUrl(kTestDomain, "<img width=\"-5\" height=\"0\" src=\"a.png\">");
  ParseUrl(kTestDomain, "<img width=\"-5\" height=\"-5\" src=\"a.png\">");
  ParseUrl(kTestDomain, "<img width=\"5\" height=\"-5\" src=\"a.png\">");
}

TEST_F(ImageRewriteTest, RewriteCacheExtendInteraction) {
  // There was a bug in async mode where rewriting failing would prevent
  // cache extension from working as well.
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kExtendCacheImages);
  rewrite_driver()->AddFilters();

  // Provide a non-image file, so image rewrite fails (but cache extension
  // works)
  SetResponseWithDefaultHeaders("a.png", kContentTypePng, "Not a PNG", 600);

  ValidateExpected("cache_extend_fallback", "<img src=a.png>",
                   StrCat("<img src=",
                          Encode("", "ce", "0", "a.png", "png"),
                          ">"));
}

// http://github.com/pagespeed/mod_pagespeed/issues/324
TEST_F(ImageRewriteTest, RetainExtraHeaders) {
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  rewrite_driver()->AddFilters();

  // Store image contents into fetcher.
  AddFileToMockFetcher(StrCat(kTestDomain, kPuzzleJpgFile), kPuzzleJpgFile,
                       kContentTypeJpeg, 100);
  TestRetainExtraHeaders(kPuzzleJpgFile, "ic", "jpg");
}

TEST_F(ImageRewriteTest, NestedConcurrentRewritesLimit) {
  // Make sure we're limiting # of concurrent rewrites properly even when we're
  // nested inside another filter, and that we do not cache that outcome
  // improperly.
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->set_image_max_rewrites_at_once(1);
  options()->set_always_rewrite_css(true);
  rewrite_driver()->AddFilters();

  const char kPngFile[] = "a.png";
  const char kCssFile[] = "a.css";
  const char kCssTemplate[] = "div{background-image:url(%s)}";
  AddFileToMockFetcher(StrCat(kTestDomain, kPngFile), kBikePngFile,
                       kContentTypePng, 100);
  GoogleString in_css = StringPrintf(kCssTemplate, kPngFile);
  SetResponseWithDefaultHeaders(kCssFile,  kContentTypeCss, in_css, 100);

  GoogleString out_css_url = Encode("", "cf", "0", kCssFile, "css");
  GoogleString out_png_url = Encode("", "ic", "0", kPngFile, "png");

  MarkTooBusyToWork();

  // If the nested context is too busy, we don't want the parent to partially
  // optimize.
  ValidateNoChanges("img_in_css", CssLinkHref(kCssFile));

  GoogleString out_css;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, out_css_url), &out_css));
  // Nothing changes in the HTML and a dropped image rewrite should be recorded.
  EXPECT_EQ(in_css, out_css);
  TimedVariable* drops = statistics()->GetTimedVariable(
      ImageRewriteFilter::kImageRewritesDroppedDueToLoad);
  EXPECT_EQ(1, drops->Get(TimedVariable::START));

  // Now rewrite it again w/o any load. We should get the image link
  // changed.
  UnMarkTooBusyToWork();
  ValidateExpected("img_in_css", CssLinkHref(kCssFile),
                   CssLinkHref(out_css_url));
  GoogleString expected_out_css =
      StringPrintf(kCssTemplate, out_png_url.c_str());
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, out_css_url), &out_css));
  // This time, however, CSS should be altered (and the drop count still be 1).
  EXPECT_EQ(expected_out_css, out_css);
  EXPECT_EQ(1, drops->Get(TimedVariable::START));
}

TEST_F(ImageRewriteTest, GifToPngTestWithResizeWithOptimize) {
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  rewrite_driver()->AddFilters();
  const char kResizedDims[] = " width=48 height=64";
  // With resize and optimization. Translating gif to png.
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypePng,
                    kResizedDims, kResizedDims, true, false);
}

TEST_F(ImageRewriteTest, GifToPngTestResizeEnableGifToPngDisabled) {
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    "", "", false, false);
  const char kResizedDims[] = " width=48 height=64";
  // Not traslating gifs to pngs.
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    kResizedDims, kResizedDims, false, false);
}

TEST_F(ImageRewriteTest, GifToPngTestWithoutResizeWithOptimize) {
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  rewrite_driver()->AddFilters();
  // Without resize and with optimization
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypePng,
                    "", "", true, false);
}

// TODO(poojatandon): Add a test where .gif file size increases on optimization.

TEST_F(ImageRewriteTest, GifToPngTestWithoutResizeWithoutOptimize) {
  // Without resize and without optimization
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    "", "", false, false);
}

TEST_F(ImageRewriteTest, GifToJpegTestWithoutResizeWithOptimize) {
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->set_image_recompress_quality(85);
  rewrite_driver()->AddFilters();
  // Without resize and with optimization
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeJpeg,
                    "", "", true, false);
}

TEST_F(ImageRewriteTest, GifToWebpTestWithResizeWithOptimize) {
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->set_image_recompress_quality(85);
  rewrite_driver()->AddFilters();
  SetupForWebpLossless();
  const char kResizedDims[] = " width=48 height=64";
  // With resize and optimization
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeWebp,
                    kResizedDims, kResizedDims, true, false);
  TestConversionVariables(0, 1, 0,   // gif
                          0, 0, 0,   // png
                          0, 0, 0,   // jpg
                          0, 0, 0,   // gif animated
                          true);
}

TEST_F(ImageRewriteTest, GifToWebpTestWithoutResizeWithOptimize) {
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->set_image_recompress_quality(85);
  rewrite_driver()->AddFilters();
  SetupForWebpLossless();
  // Without resize and with optimization
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeWebp,
                    "", "", true, false);
  TestConversionVariables(0, 1, 0,   // gif
                          0, 0, 0,   // png
                          0, 0, 0,   // jpg
                          0, 0, 0,   // gif animated
                          true);
}

TEST_F(ImageRewriteTest, InlinableImagesInsertedIntoPropertyCache) {
  // If image_inlining_identify_and_cache_without_rewriting() is set in
  // RewriteOptions, images that would have been inlined are instead inserted
  // into the property cache.
  options()->set_image_inline_max_bytes(30000);
  options()->set_cache_small_images_unrewritten(true);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  rewrite_driver()->AddFilters();
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    kChefDims, kChefDims, false, false);
  EXPECT_STREQ("\"http://test.com/IronChef2.gif\"",
               FetchInlinablePropertyCacheValue()->value());
}

TEST_F(ImageRewriteTest, InlinableCssImagesInsertedIntoPropertyCache) {
  // If image_inlining_identify_and_cache_without_rewriting() is set in
  // RewriteOptions, CSS images that would have been inlined are instead
  // inserted into the property cache.
  options()->set_image_inline_max_bytes(30000);
  options()->set_cache_small_images_unrewritten(true);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  rewrite_driver()->AddFilters();
  const char kPngFile1[] = "a.png";
  const char kPngFile2[] = "b.png";
  AddFileToMockFetcher(StrCat(kTestDomain, kPngFile1), kBikePngFile,
                       kContentTypePng, 100);
  AddFileToMockFetcher(StrCat(kTestDomain, kPngFile2), kCuppaTPngFile,
                       kContentTypePng, 100);
  const char kCssFile[] = "a.css";
  // We include a duplicate image here to verify that duplicate suppression
  // is working.
  GoogleString css_contents = StringPrintf(
      "div{background-image:url(%s)}"
      "h1{background-image:url(%s)}"
      "p{background-image:url(%s)}", kPngFile1, kPngFile1, kPngFile2);
  SetResponseWithDefaultHeaders(kCssFile, kContentTypeCss, css_contents, 100);
  // Parse the CSS and ensure contents are unchanged.
  GoogleString out_css_url = Encode("", "cf", "0", kCssFile, "css");
  GoogleString out_css;
  StringAsyncFetch async_fetch(RequestContext::NewTestRequestContext(
      server_context()->thread_system()), &out_css);
  ResponseHeaders response;
  async_fetch.set_response_headers(&response);
  EXPECT_TRUE(rewrite_driver_->FetchResource(StrCat(kTestDomain, out_css_url),
                                             &async_fetch));
  rewrite_driver_->WaitForShutDown();
  EXPECT_TRUE(async_fetch.success());

  // The CSS is unmodified and the image URL is stored in the property cache.
  EXPECT_STREQ(css_contents, out_css);
  // The expected URLs are present.
  StringPieceVector urls;
  StringSet expected_urls;
  expected_urls.insert("\"http://test.com/a.png\"");
  expected_urls.insert("\"http://test.com/b.png\"");
  SplitStringPieceToVector(FetchInlinablePropertyCacheValue()->value(), ",",
                           &urls, false);
  EXPECT_EQ(expected_urls.size(), urls.size());
  for (int i = 0; i < urls.size(); ++i) {
    EXPECT_EQ(1, expected_urls.count(urls[i].as_string()));
  }
}

TEST_F(ImageRewriteTest, RewritesDroppedDueToNoSavingNoResizeTest) {
  Histogram* rewrite_latency_ok = statistics()->GetHistogram(
      ImageRewriteFilter::kImageRewriteLatencyOkMs);
  Histogram* rewrite_latency_failed = statistics()->GetHistogram(
      ImageRewriteFilter::kImageRewriteLatencyFailedMs);
  rewrite_latency_ok->Clear();
  rewrite_latency_failed->Clear();

  options()->EnableFilter(RewriteOptions::kRecompressPng);
  rewrite_driver()->AddFilters();
  const char kOriginalDims[] = " width=65 height=70";
  TestSingleRewrite(kCuppaOPngFile, kContentTypePng, kContentTypePng,
                    kOriginalDims, kOriginalDims, false, false);
  Variable* rewrites_drops = statistics()->GetVariable(
      ImageRewriteFilter::kImageRewritesDroppedNoSavingNoResize);
  EXPECT_EQ(1, rewrites_drops->Get());
  EXPECT_EQ(0, rewrite_latency_ok->Count());
  EXPECT_EQ(1, rewrite_latency_failed->Count());
}

TEST_F(ImageRewriteTest, RewritesDroppedDueToMIMETypeUnknownTest) {
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  rewrite_driver()->AddFilters();
  const char kOriginalDims[] = " width=10 height=10";
  TestSingleRewrite(kSmallDataFile, kContentTypePng, kContentTypePng,
                    kOriginalDims, kOriginalDims, false, false);
  Variable* rewrites_drops = statistics()->GetVariable(
      ImageRewriteFilter::kImageRewritesDroppedMIMETypeUnknown);
  EXPECT_EQ(1, rewrites_drops->Get());
}

TEST_F(ImageRewriteTest, JpegQualityForSmallScreens) {
  ResetUserAgent("Mozilla/5.0 (Linux; U; Android 4.0.1; en-us; "
      "Galaxy Nexus Build/ICL27) AppleWebKit/534.30 (KHTML, like Gecko) "
      "Version/4.0 Mobile Safari/534.30");
  ImageRewriteFilter image_rewrite_filter(rewrite_driver());
  ResourceContext ctx;
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  const ResourcePtr res_ptr(
      rewrite_driver()->CreateInputResourceAbsoluteUncheckedForTestsOnly(
          "data:image/png;base64,test"));
  scoped_ptr<Image::CompressionOptions> img_options(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr));

  // Neither option is set explicitly, default is 70.
  EXPECT_EQ(70, img_options->jpeg_quality);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());

  // Base image quality is set, but for_small_screens is not, return base.
  options()->ClearSignatureForTesting();
  options()->set_image_jpeg_recompress_quality_for_small_screens(-1);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr));
  EXPECT_EQ(85, img_options->jpeg_quality);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());

  // Base jpeg quality not set, but for_small_screens is, return small_screen.
  options()->ClearSignatureForTesting();
  options()->set_image_recompress_quality(-1);
  options()->set_image_jpeg_recompress_quality_for_small_screens(20);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr));
  EXPECT_EQ(20, img_options->jpeg_quality);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());

  // Neither jpeg quality is set, return -1.
  options()->ClearSignatureForTesting();
  options()->set_image_recompress_quality(-1);
  options()->set_image_jpeg_recompress_quality_for_small_screens(-1);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr));
  EXPECT_EQ(-1, img_options->jpeg_quality);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());

  // Base and for_small_screen options are set; mobile
  options()->ClearSignatureForTesting();
  options()->set_image_jpeg_recompress_quality(85);
  options()->set_image_jpeg_recompress_quality_for_small_screens(20);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr));
  EXPECT_EQ(20, img_options->jpeg_quality);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());

  // Non-mobile UA.
  ResetUserAgent("Mozilla/5.0 (Windows; U; Windows NT 5.1; "
      "en-US) AppleWebKit/525.13 (KHTML, like Gecko) Chrome/0.A.B.C "
      "Safari/525.13");
  ctx.Clear();
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr));
  EXPECT_EQ(85, img_options->jpeg_quality);
  EXPECT_FALSE(ctx.may_use_small_screen_quality());

  // Mobile UA
  ResetUserAgent("iPhone OS Safari");
  options()->ClearSignatureForTesting();
  options()->set_image_jpeg_recompress_quality_for_small_screens(70);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr));
  EXPECT_EQ(70, img_options->jpeg_quality);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());

  // Although the regular (desktop) quality is smaller, it won't affect the
  // quality used for mobile.
  ResetUserAgent("iPhone OS Safari");
  options()->ClearSignatureForTesting();
  options()->set_image_jpeg_recompress_quality_for_small_screens(70);
  options()->set_image_jpeg_recompress_quality(60);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr));
  EXPECT_EQ(70, img_options->jpeg_quality);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());
}

TEST_F(ImageRewriteTest, WebPQualityForSmallScreens) {
  ResetUserAgent("Mozilla/5.0 (Linux; U; Android 4.0.1; en-us; "
      "Galaxy Nexus Build/ICL27) AppleWebKit/534.30 (KHTML, like Gecko) "
      "Version/4.0 Mobile Safari/534.30");
  ImageRewriteFilter image_rewrite_filter(rewrite_driver());
  ResourceContext ctx;
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  const ResourcePtr res_ptr(
      rewrite_driver()->CreateInputResourceAbsoluteUncheckedForTestsOnly(
          "data:image/png;base64,test"));
  scoped_ptr<Image::CompressionOptions> img_options(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr));

  // Neither option is set, default is 70.
  EXPECT_EQ(70, img_options->webp_quality);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());

  // Base webp quality set, but for_small_screens is not, return base quality.
  ctx.Clear();
  options()->ClearSignatureForTesting();
  options()->set_image_webp_recompress_quality(85);
  options()->set_image_webp_recompress_quality_for_small_screens(-1);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr));
  EXPECT_EQ(85, img_options->webp_quality);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());

  // Base webp quality not set, but for_small_screens is, return small_screen.
  options()->ClearSignatureForTesting();
  options()->set_image_recompress_quality(-1);
  options()->set_image_webp_recompress_quality(-1);
  options()->set_image_webp_recompress_quality_for_small_screens(20);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr));
  EXPECT_EQ(20, img_options->webp_quality);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());

  // Base and for_small_screen options are set; mobile
  options()->ClearSignatureForTesting();
  options()->set_image_webp_recompress_quality(85);
  options()->set_image_webp_recompress_quality_for_small_screens(20);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr));
  EXPECT_EQ(20, img_options->webp_quality);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());

  // Non-mobile UA.
  ResetUserAgent("Mozilla/5.0 (Windows; U; Windows NT 5.1; "
      "en-US) AppleWebKit/525.13 (KHTML, like Gecko) Chrome/0.A.B.C "
      "Safari/525.13");
  ctx.Clear();
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr));
  EXPECT_EQ(85, img_options->webp_quality);
  EXPECT_FALSE(ctx.may_use_small_screen_quality());

  // Mobile UA
  ResetUserAgent("iPhone OS Safari");
  ctx.Clear();
  options()->ClearSignatureForTesting();
  options()->set_image_webp_recompress_quality_for_small_screens(70);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr));
  EXPECT_EQ(70, img_options->webp_quality);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());

  // Although the regular (desktop) quality is smaller, it won't affect the
  // quality used for mobile.
  ResetUserAgent("iPhone OS Safari");
  ctx.Clear();
  options()->ClearSignatureForTesting();
  options()->set_image_webp_recompress_quality_for_small_screens(70);
  options()->set_image_webp_recompress_quality(55);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr));
  EXPECT_EQ(70, img_options->webp_quality);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());
}

void SetNumberOfScans(int num_scans, int num_scans_small_screen,
                      const ResourcePtr res_ptr,
                      RewriteOptions* options,
                      RewriteDriver* rewrite_driver,
                      ImageRewriteFilter* image_rewrite_filter,
                      ResourceContext* ctx,
                      scoped_ptr<Image::CompressionOptions>* img_options) {
  static const int DO_NOT_SET = -10;
  ctx->Clear();
  if ((num_scans != DO_NOT_SET)  ||
      (num_scans_small_screen != DO_NOT_SET)) {
    options->ClearSignatureForTesting();
    if (num_scans != DO_NOT_SET) {
      options->set_image_jpeg_num_progressive_scans(num_scans);
    }
    if (num_scans_small_screen != DO_NOT_SET) {
      options->set_image_jpeg_num_progressive_scans_for_small_screens(
          num_scans_small_screen);
    }
  }
  image_rewrite_filter->EncodeUserAgentIntoResourceContext(ctx);
  img_options->reset(
      image_rewrite_filter->ImageOptionsForLoadedResource(
          *ctx, res_ptr));
}

TEST_F(ImageRewriteTest, JpegProgressiveScansForSmallScreens) {
  static const int DO_NOT_SET = -10;
  ResetUserAgent("Mozilla/5.0 (Linux; U; Android 4.0.1; en-us; "
      "Galaxy Nexus Build/ICL27) AppleWebKit/534.30 (KHTML, like Gecko) "
      "Version/4.0 Mobile Safari/534.30");
  ImageRewriteFilter image_rewrite_filter(rewrite_driver());
  ResourceContext ctx;
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  const ResourcePtr res_ptr(
      rewrite_driver()->CreateInputResourceAbsoluteUncheckedForTestsOnly(
          "data:image/png;base64,test"));
  scoped_ptr<Image::CompressionOptions> img_options(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr));

  // Neither option is set, default is -1.
  EXPECT_EQ(-1, img_options->jpeg_num_progressive_scans);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());

  // Base jpeg num scans set, but for_small_screens is not, return
  // base num scans.
  SetNumberOfScans(8, -1, res_ptr, options(), rewrite_driver(),
                   &image_rewrite_filter, &ctx, &img_options);
  EXPECT_EQ(8, img_options->jpeg_num_progressive_scans);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());

  // Base jpeg quality not set, but for_small_screens is, return small_screen.
  SetNumberOfScans(DO_NOT_SET, 2, res_ptr, options(), rewrite_driver(),
                   &image_rewrite_filter, &ctx, &img_options);
  EXPECT_EQ(2, img_options->jpeg_num_progressive_scans);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());

  // Base and for_small_screen options are set; mobile.
  SetNumberOfScans(8, 2, res_ptr, options(), rewrite_driver(),
                   &image_rewrite_filter, &ctx, &img_options);
  EXPECT_EQ(2, img_options->jpeg_num_progressive_scans);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());

  // Non-mobile UA.
  ResetUserAgent("Mozilla/5.0 (Windows; U; Windows NT 5.1; "
      "en-US) AppleWebKit/525.13 (KHTML, like Gecko) Chrome/0.A.B.C "
      "Safari/525.13");
  SetNumberOfScans(DO_NOT_SET, DO_NOT_SET, res_ptr, options(), rewrite_driver(),
                   &image_rewrite_filter, &ctx, &img_options);
  EXPECT_EQ(8, img_options->jpeg_num_progressive_scans);
  EXPECT_FALSE(ctx.may_use_small_screen_quality());

  // Mobile UA
  ResetUserAgent("iPhone OS Safari");
  SetNumberOfScans(DO_NOT_SET, 2, res_ptr, options(), rewrite_driver(),
                   &image_rewrite_filter, &ctx, &img_options);
  EXPECT_EQ(2, img_options->jpeg_num_progressive_scans);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());

  // Although the regular (desktop) number of scans is smaller, it won't affect
  // that used for mobile.
  ResetUserAgent("iPhone OS Safari");
  SetNumberOfScans(2, 8, res_ptr, options(), rewrite_driver(),
                   &image_rewrite_filter, &ctx, &img_options);
  EXPECT_EQ(8, img_options->jpeg_num_progressive_scans);
  EXPECT_TRUE(ctx.may_use_small_screen_quality());
}

TEST_F(ImageRewriteTest, ProgressiveJpegThresholds) {
  GoogleString image_data;
  ASSERT_TRUE(LoadFile(kPuzzleJpgFile, &image_data));
  Image::CompressionOptions* options = new Image::CompressionOptions;
  options->recompress_jpeg = true;
  scoped_ptr<Image> image(NewImage(image_data, kPuzzleJpgFile, "",
                                   options, timer(), message_handler()));

  // Since we haven't established a size, resizing won't happen.
  ImageDim dims;
  EXPECT_TRUE(ImageTestingPeer::ShouldConvertToProgressive(-1, image.get()));

  // Now provide a context, resizing the image to 10x10.  Of course
  // we should not convert that to progressive, because post-resizing
  // the image will be tiny.
  dims.set_width(10);
  dims.set_height(10);
  ImageTestingPeer::SetResizedDimensions(dims, image.get());
  EXPECT_FALSE(ImageTestingPeer::ShouldConvertToProgressive(-1, image.get()));

  // At 256x192, we are close to the tipping point, and whether we should
  // convert to progressive or not is dependent on the compression
  // level.
  dims.set_width(256);
  dims.set_height(192);
  ImageTestingPeer::SetResizedDimensions(dims, image.get());
  EXPECT_TRUE(ImageTestingPeer::ShouldConvertToProgressive(-1, image.get()));

  // Setting compression to 90.  The quality level is high, and our model
  // says we'll wind up with an image >10204 bytes, which is still
  // large enough to convert to progressive.
  EXPECT_TRUE(ImageTestingPeer::ShouldConvertToProgressive(90, image.get()));

  // Now set the compression to 75, which shrinks our image to <10k so
  // we should stop converting to progressive.
  EXPECT_FALSE(ImageTestingPeer::ShouldConvertToProgressive(75, image.get()));
}

TEST_F(ImageRewriteTest, CacheControlHeaderCheckForNonWebpUA) {
  if (RunningOnValgrind()) {  // Too slow under vg.
    return;
  }

  GoogleString initial_image_url = StrCat(kTestDomain, kPuzzleJpgFile);
  const GoogleString kHtmlInput =
      StrCat("<img src='", initial_image_url, "'>");
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  AddRecompressImageFilters();
  rewrite_driver()->AddFilters();
  ResetForWebp();

  GoogleString page_url = StrCat(kTestDomain, "test.html");
  // Store image contents into fetcher.
  AddFileToMockFetcher(initial_image_url, kPuzzleJpgFile,
                       kContentTypeJpeg, 100);
  int64 start_time_ms = timer()->NowMs();
  ParseUrl(page_url, kHtmlInput);

  StringVector image_urls;
  CollectImgSrcs(initial_image_url, output_buffer_, &image_urls);
  EXPECT_EQ(1, image_urls.size());
  const GoogleUrl image_gurl(image_urls[0]);
  EXPECT_TRUE(image_gurl.LeafSansQuery().ends_with("webp"));
  const GoogleString& src_string = image_urls[0];

  ExpectStringAsyncFetch expect_callback(true, CreateRequestContext());
  EXPECT_TRUE(rewrite_driver()->FetchResource(src_string, &expect_callback));
  rewrite_driver()->WaitForCompletion();

  ResponseHeaders* response_headers = expect_callback.response_headers();
  EXPECT_TRUE(response_headers->IsProxyCacheable());
  EXPECT_EQ(Timer::kYearMs,
            response_headers->CacheExpirationTimeMs() - start_time_ms);
  // Set a non-webp UA.
  ResetUserAgent("");

  GoogleString new_image_url =  StrCat(kTestDomain, kPuzzleJpgFile);
  page_url = StrCat(kTestDomain, "test.html");
  ParseUrl(page_url, kHtmlInput);

  CollectImgSrcs(new_image_url, output_buffer_, &image_urls);
  EXPECT_EQ(2, image_urls.size());
  const GoogleString& rewritten_url = image_urls[1];
  const GoogleUrl rewritten_gurl(rewritten_url);
  EXPECT_TRUE(rewritten_gurl.LeafSansQuery().ends_with("jpg"));

  GoogleString content;
  ResponseHeaders response;
  MD5Hasher hasher;
  GoogleString new_hash = hasher.Hash(output_buffer_);
  // Fetch a new rewritten url with a new hash so as to get a short cache
  // time.
  const GoogleString rewritten_url_new =
      StrCat("http://test.com/x", kPuzzleJpgFile, ".pagespeed.ic.",
             new_hash, ".jpg");
  ASSERT_TRUE(FetchResourceUrl(rewritten_url_new, &content, &response));
  EXPECT_FALSE(response.IsProxyCacheable());
  // TTL will be 100s since resource creation, because that is the input
  // resource TTL and is lower than the 300s implicit cache TTL for hash
  // mismatch.
  EXPECT_EQ(100 * Timer::kSecondMs,
            response.CacheExpirationTimeMs() - start_time_ms);
}

TEST_F(ImageRewriteTest, RewriteImagesAddingOptionsToUrl) {
  AddRecompressImageFilters();
  options()->set_add_options_to_urls(true);
  options()->set_image_jpeg_recompress_quality(73);
  AddFileToMockFetcher(kPuzzleUrl, kPuzzleJpgFile, kContentTypeJpeg, 100);
  GoogleString img_src;
  RewriteImageFromHtml("img", kContentTypeJpeg, &img_src);
  GoogleUrl img_gurl(html_gurl(), img_src);
  EXPECT_STREQ("", img_gurl.Query());
  ResourceNamer namer;
  EXPECT_TRUE(rewrite_driver()->Decode(img_gurl.LeafSansQuery(), &namer));
  EXPECT_STREQ("gp+jw+pj+rj+rp+rw+iq=73", namer.options());

  // Serve this from rewrite_driver(), which has the same cache & the
  // same options set so will have the canonical results.
  GoogleString golden_content, remote_content;
  ResponseHeaders golden_response, remote_response;
  EXPECT_TRUE(FetchResourceUrl(img_gurl.Spec(), &golden_content,
                               &golden_response));
  // EXPECT_EQ(84204, golden_content.size());

  // TODO(jmarantz): We cannot test fetches using a flow that
  // resembles that of the server currently; we need a non-trivial
  // refactor to put the query-param processing into BlockingFetch.
  //
  // In the meantime we rely on system-tests to make sure we can fetch
  // what we rewrite.
  /*
  RewriteOptions* other_opts = other_server_context()->global_options();
  other_opts->ClearSignatureForTesting();
  other_opts->set_add_options_to_urls(true);
  other_server_context()->ComputeSignature(other_opts);
  ASSERT_TRUE(BlockingFetch(img_src, &remote_content,
                            other_server_context(), nullptr));
  ASSERT_EQ(golden_content.size(), remote_content.size());
  EXPECT_EQ(golden_content, remote_content);  // don't bother if sizes differ...
  */
}

TEST_F(ImageRewriteTest, ServeWebpFromColdCache) {
  const StringPiece kJpegMimeType = kContentTypeJpeg.mime_type();
  const StringPiece kWebpMimeType = kContentTypeWebp.mime_type();

  // First rewrite an HTML file with an image for a webp-compatible browser,
  // and collect the image URL.
  UseMd5Hasher();
  AddRecompressImageFilters();
  options()->set_serve_rewritten_webp_urls_to_any_agent(true);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  GoogleString img_src;
  ResetForWebp();
  Variable* webp_rewrite_count = statistics()->GetVariable(
      ImageRewriteFilter::kImageWebpRewrites);
  AddFileToMockFetcher(kPuzzleUrl, kPuzzleJpgFile, kContentTypeJpeg, 100);
  RewriteImageFromHtml("img", kContentTypeWebp, &img_src);
  EXPECT_EQ(1, webp_rewrite_count->Get());
  GoogleUrl webp_gurl(html_gurl(), img_src);

  // Serve this image from cache. No further rewrites should be needed, since
  // the image was optimized when serving HTML.
  GoogleString golden_content, content;
  ResponseHeaders response;
  EXPECT_TRUE(FetchWebp(webp_gurl.Spec(), "webp", &golden_content, &response));
  EXPECT_STREQ("image/webp", response.Lookup1(HttpAttributes::kContentType));
  EXPECT_TRUE(response.IsProxyCacheable());
  EXPECT_EQ(0, webp_rewrite_count->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());

  // Now clear the cache and fetch the resource again.  We will need to
  // reconstruct the image but we'll get the same result.
  lru_cache()->Clear();
  EXPECT_TRUE(FetchWebp(webp_gurl.Spec(), "webp", &content, &response));
  EXPECT_STREQ(kWebpMimeType, response.Lookup1(HttpAttributes::kContentType));
  EXPECT_TRUE(response.IsProxyCacheable());
  EXPECT_EQ(1, webp_rewrite_count->Get());  // We had to reconstruct.
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_TRUE(content == golden_content);

  // Do the same test again, but don't clear the cache.
  EXPECT_TRUE(FetchWebp(webp_gurl.Spec(), "webp", &content, &response));
  EXPECT_STREQ(kWebpMimeType, response.Lookup1(HttpAttributes::kContentType));
  EXPECT_TRUE(response.IsProxyCacheable());
  EXPECT_EQ(0, webp_rewrite_count->Get());  // No need to reconstruct...
  EXPECT_EQ(1, lru_cache()->num_hits());    // ...picked it up from cache.
  EXPECT_TRUE(content == golden_content);

  // Now set the user-agent to something that does not support webp,
  // and we should still reconstruct the webp when asked for it, since
  // we have called options()->set_serve_rewritten_webp_urls_to_any_agent(true)
  // above.
  lru_cache()->Clear();
  EXPECT_TRUE(FetchWebp(webp_gurl.Spec(), "null", &content, &response));
  EXPECT_STREQ(kWebpMimeType, response.Lookup1(HttpAttributes::kContentType));
  EXPECT_TRUE(response.IsProxyCacheable());
  EXPECT_EQ(1, webp_rewrite_count->Get());  // We had to reconstruct.
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_TRUE(content == golden_content);

  // Now turn off 'serve_rewritten_webp_urls_to_any_agent', and
  // we will serve the original jpeg instead, privately cached.
  options()->ClearSignatureForTesting();
  options()->set_serve_rewritten_webp_urls_to_any_agent(false);
  server_context()->ComputeSignature(options());

  // Don't clear the cache here, proving Issue 846 is fixed.
  ClearStats();
  EXPECT_TRUE(FetchWebp(webp_gurl.Spec(), "null", &content, &response));
  EXPECT_STREQ(kJpegMimeType, response.Lookup1(
      HttpAttributes::kContentType));
  EXPECT_FALSE(response.IsProxyCacheable());
  EXPECT_TRUE(response.IsBrowserCacheable());
  EXPECT_EQ(0, webp_rewrite_count->Get());  // Reconstruction not attempted.
  EXPECT_EQ(2, lru_cache()->num_hits());    // Hits, but result is invalid.
  EXPECT_FALSE(content == golden_content);
  EXPECT_GT(content.size(), golden_content.size());

  // All works fine anyway we if we clear the cache first.
  lru_cache()->Clear();
  EXPECT_TRUE(FetchWebp(webp_gurl.Spec(), "null", &content, &response));
  EXPECT_STREQ(kJpegMimeType, response.Lookup1(HttpAttributes::kContentType));
  EXPECT_FALSE(response.IsProxyCacheable());
  EXPECT_TRUE(response.IsBrowserCacheable());
  EXPECT_EQ(0, webp_rewrite_count->Get());  // Reconstruction not attempted.
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_FALSE(content == golden_content);
  EXPECT_GT(content.size(), golden_content.size());

  // But if any webp-enabled client asks for the resource, we will serve
  // the webp to them.
  EXPECT_TRUE(FetchWebp(webp_gurl.Spec(), "webp", &content, &response));
  EXPECT_STREQ(kWebpMimeType, response.Lookup1(HttpAttributes::kContentType));

  // And we will continue to serve jpeg to other browsers.
  EXPECT_TRUE(FetchWebp(webp_gurl.Spec(), "none", &content, &response));
  EXPECT_STREQ(kJpegMimeType, response.Lookup1(HttpAttributes::kContentType));
}

// If we drop a rewrite because of load, make sure it returns the original URL.
// This verifies that Issue 707 is fixed.
TEST_F(ImageRewriteTest, TooBusyReturnsOriginalResource) {
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->set_image_max_rewrites_at_once(1);
  rewrite_driver()->AddFilters();

  MarkTooBusyToWork();
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng, "", "",
                    false, false);

  UnMarkTooBusyToWork();
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng, "", "",
                    true, false);
}

TEST_F(ImageRewriteTest, ResizeUsingRenderedDimensions) {
  MockCriticalImagesFinder* finder = new MockCriticalImagesFinder(statistics());
  server_context()->set_critical_images_finder(finder);
  options()->EnableFilter(RewriteOptions::kResizeToRenderedImageDimensions);
  options()->set_log_background_rewrites(true);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  rewrite_driver()->AddFilters();

  GoogleString expected_rewritten_url =
      StrCat(kTestDomain, UintToString(100) , "x", UintToString(70), "x",
      kChefGifFile, ".pagespeed.ic.0.png");
  TestForRenderedDimensions(finder, 100, 70, 100, 70, "",
                            expected_rewritten_url, 1);
  TestBackgroundRewritingLog(
      1, /* rewrite_info_size */
      0, /* rewrite_info_index */
      RewriterApplication::APPLIED_OK, /* status */
      "ic", /* ID */
      "", /* URL */
      IMAGE_GIF, /* original_type */
      IMAGE_PNG, /* optimized_type */
      24941, /* original_size */
      11489, /* optimized_size */
      true, /* is_recompressed */
      true, /* is_resized */
      192, /* original width */
      256, /* original height */
      true, /* is_resized_using_rendered_dimensions */
      100, /* resized_width */
      70 /* resized_height */);

  expected_rewritten_url =
      StrCat(kTestDomain, "x", kChefGifFile, ".pagespeed.ic.0.png");
  TestForRenderedDimensions(finder, 100, 0, 192, 256, "",
                            expected_rewritten_url, 0);
  TestForRenderedDimensions(finder, 0, 70, 192, 256, "",
                            expected_rewritten_url, 0);
  TestForRenderedDimensions(finder, 0, 0, 192, 256, "",
                            expected_rewritten_url, 0);

  // Test if rendered dimensions is more than the width and height attribute,
  // not to resize the image using rendered dimensions.
  expected_rewritten_url =
      StrCat(kTestDomain, UintToString(100), "x", UintToString(100), "x",
             kChefGifFile, ".pagespeed.ic.0.png");
  TestForRenderedDimensions(finder, 400, 400, 100, 100,
                            " width=\"100\" height=\"100\"",
                            expected_rewritten_url, 0);
}

TEST_F(ImageRewriteTest, ResizeEmptyImageUsingRenderedDimensions) {
  MockCriticalImagesFinder* finder = new MockCriticalImagesFinder(statistics());
  server_context()->set_critical_images_finder(finder);
  options()->EnableFilter(RewriteOptions::kResizeToRenderedImageDimensions);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  rewrite_driver()->AddFilters();

  RenderedImages* rendered_images = new RenderedImages;
  RenderedImages_Image* images = rendered_images->add_image();
  images->set_src(StrCat(kTestDomain, kEmptyScreenGifFile));
  images->set_rendered_width(1);  // Only set width, but not height.

  finder->set_rendered_images(rendered_images);

  TestSingleRewrite(kEmptyScreenGifFile, kContentTypeGif, kContentTypeGif,
                    "", "", false, false);
}

TEST_F(ImageRewriteTest, PreserveUrlRelativity) {
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  rewrite_driver()->AddFilters();
  AddFileToMockFetcher("a.jpg", kPuzzleJpgFile, kContentTypeJpeg, 100);
  AddFileToMockFetcher("b.jpg", kPuzzleJpgFile, kContentTypeJpeg, 100);
  ValidateExpected(
      "single_attribute",
      "<img src=a.jpg>"
      "<img src=http://test.com/b.jpg>",
      StrCat("<img src=", Encode("", "ic", "0", "a.jpg", "jpg"), ">"
             "<img src=", Encode("http://test.com/", "ic", "0", "b.jpg", "jpg"),
             ">"));
}

TEST_F(ImageRewriteTest, RewriteMultipleAttributes) {
  // Test a complex setup with both regular and custom image urls, including an
  // invalid image which should only get cache-extended.
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kExtendCacheImages);

  rewrite_driver()->AddFilters();

  options()->ClearSignatureForTesting();
  options()->AddUrlValuedAttribute("img", "data-src", semantic_type::kImage);
  server_context()->ComputeSignature(options());

  // A, B, and D are real image files, so they should be properly rewritten.
  AddFileToMockFetcher("a.jpg", kPuzzleJpgFile, kContentTypeJpeg, 100);
  AddFileToMockFetcher("b.jpg", kPuzzleJpgFile, kContentTypeJpeg, 100);
  AddFileToMockFetcher("d.jpg", kPuzzleJpgFile, kContentTypeJpeg, 100);

  // C is not an image file, so image rewrite fails (but cache extension works).
  SetResponseWithDefaultHeaders("c.jpg", kContentTypeJpeg, "Not a JPG", 600);

  ValidateExpected(
      "multiple_attributes",
      "<img src=a.jpg data-src=b.jpg data-src=c.jpg data-src=d.jpg>",
      StrCat(
          "<img src=", Encode("", "ic", "0", "a.jpg", "jpg"),
          " data-src=", Encode("", "ic", "0", "b.jpg", "jpg"),
          " data-src=", Encode("", "ce", "0", "c.jpg", "jpg"),
          " data-src=", Encode("", "ic", "0", "d.jpg", "jpg"),
          ">"));
}

TEST_F(ImageRewriteTest, IproCorrectVaryHeaders) {
  // See https://github.com/pagespeed/mod_pagespeed/issues/817
  // Here we're particularly looking for some issues that the ipro-specific
  // testing doesn't catch because it uses a fake version of the image rewrite
  // filter.
  SetupIproTests("Accept");
  rewrite_driver()->AddFilters();
  GoogleString puzzleUrl = StrCat(kTestDomain, kPuzzleJpgFile);
  GoogleString bikeUrl = StrCat(kTestDomain, kBikePngFile);
  GoogleString cuppaUrl = StrCat(kTestDomain, kCuppaPngFile);
  ResponseHeaders response_headers;

  // We test 3 kinds of image (photo, photographic png, non-photographic png)
  // with two pairs of browsers: simple and maximally webp-capable (including
  // Accept: image/webp).

  // puzzle is unconditionally webp-convertible and thus gets a vary: header.
  IProFetchAndValidate(puzzleUrl, "webp-la", "image/webp", &response_headers);
  EXPECT_EQ(&kContentTypeWebp, response_headers.DetermineContentType()) <<
      response_headers.DetermineContentType()->mime_type();
  EXPECT_STREQ(HttpAttributes::kAccept,
               response_headers.Lookup1(HttpAttributes::kVary));
  IProFetchAndValidate(puzzleUrl, "", "", &response_headers);
  EXPECT_EQ(&kContentTypeJpeg, response_headers.DetermineContentType()) <<
      response_headers.DetermineContentType()->mime_type();
  EXPECT_STREQ(HttpAttributes::kAccept,
               response_headers.Lookup1(HttpAttributes::kVary));

  // Similarly, bike is photographic and will be jpeg or webp-converted and have
  // a Vary: header.
  IProFetchAndValidate(bikeUrl, "webp-la", "image/webp", &response_headers);
  EXPECT_EQ(&kContentTypeWebp, response_headers.DetermineContentType()) <<
      response_headers.DetermineContentType()->mime_type();
  EXPECT_STREQ(HttpAttributes::kAccept,
               response_headers.Lookup1(HttpAttributes::kVary));
  IProFetchAndValidate(bikeUrl, "", "", &response_headers);
  EXPECT_EQ(&kContentTypeJpeg, response_headers.DetermineContentType()) <<
      response_headers.DetermineContentType()->mime_type();
  EXPECT_STREQ(HttpAttributes::kAccept,
               response_headers.Lookup1(HttpAttributes::kVary));

  // Finally, cuppa has an alpha channel and is non-photographic, so it
  // shouldn't be converted to webp and should remain a png.  Thus it should
  // lack a Vary: header.
  IProFetchAndValidate(cuppaUrl, "webp-la", "image/webp", &response_headers);
  EXPECT_EQ(&kContentTypePng, response_headers.DetermineContentType()) <<
      response_headers.DetermineContentType()->mime_type();
  EXPECT_FALSE(response_headers.Has(HttpAttributes::kVary)) <<
      response_headers.Lookup1(HttpAttributes::kVary);
  IProFetchAndValidate(cuppaUrl, "", "", &response_headers);
  EXPECT_EQ(&kContentTypePng, response_headers.DetermineContentType()) <<
      response_headers.DetermineContentType()->mime_type();
  EXPECT_FALSE(response_headers.Has(HttpAttributes::kVary)) <<
      response_headers.Lookup1(HttpAttributes::kVary);
}

TEST_F(ImageRewriteTest, NoTransformOptimized) {
  options()->set_no_transform_optimized_images(true);
  AddRecompressImageFilters();
  rewrite_driver()->AddFilters();
  GoogleString initial_url = StrCat(kTestDomain, kBikePngFile);
  AddFileToMockFetcher(initial_url, kBikePngFile, kContentTypePng, 100);
  GoogleString out_jpg_url(Encode(kTestDomain, "ic", "0", kBikePngFile, "jpg"));
  GoogleString out_jpg;
  ResponseHeaders response_headers;
  EXPECT_TRUE(FetchResourceUrl(out_jpg_url, &out_jpg, &response_headers));
  ConstStringStarVector values;
  ASSERT_TRUE(response_headers.Lookup(HttpAttributes::kCacheControl, &values));
  bool found = false;
  for (int i = 0, n = values.size(); i < n; ++i) {
    found |= *(values[i]) == "no-transform";
  }
  EXPECT_TRUE(found);
}

TEST_F(ImageRewriteTest, ReportDimensionsToJs) {
  options()->EnableFilter(RewriteOptions::kExperimentCollectMobImageInfo);
  AddRecompressImageFilters();
  rewrite_driver()->AddFilters();
  AddFileToMockFetcher(StrCat(kTestDomain, "a.png"), kBikePngFile,
                       kContentTypePng, 100);
  AddFileToMockFetcher(StrCat(kTestDomain, "b.jpeg"), kPuzzleJpgFile,
                       kContentTypeJpeg, 100);
  const GoogleString kTest1Gif = StrCat(kTestDomain, k1x1GifFile);
  AddFileToMockFetcher(kTest1Gif, k1x1GifFile, kContentTypeGif, 100);

  SetupWriter();
  rewrite_driver()->StartParse(StrCat(kTestDomain, "dims.html"));
  rewrite_driver()->ParseText(StrCat("<img src=\"", kTestDomain, "a.png\">"));
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText(StrCat("<img src=\"", kTestDomain, "b.jpeg\">"));
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText(StrCat("<img src=\"", kTest1Gif, "\">"));
  rewrite_driver()->FinishParse();

  GoogleString out_png_url(Encode(kTestDomain, "ic", "0", "a.png", "jpg"));
  GoogleString out_jpeg_url(Encode(kTestDomain, "ic", "0", "b.jpeg", "jpg"));
  GoogleString js = StrCat(
      "psMobStaticImageInfo = {"
      "\"", kTest1Gif, "\":{w:1,h:1},"  // not optimized.
      "\"", out_png_url, "\":{w:100,h:100},"
      "\"", out_jpeg_url, "\":{w:1023,h:766},"
      "}");
  EXPECT_EQ(StrCat("<img src=\"", out_png_url, "\">"
                   "<img src=\"", out_jpeg_url, "\">"
                   "<img src=\"", kTest1Gif, "\">"
                   "<script>", js, "</script>"),
            output_buffer_);
}

TEST_F(ImageRewriteTest, ReportDimensionsToJsPartial) {
  // Test where one image isn't loaded in time. We report partial info.
  SetupWaitFetcher();
  options()->EnableFilter(RewriteOptions::kExperimentCollectMobImageInfo);
  AddRecompressImageFilters();
  rewrite_driver()->AddFilters();
  AddFileToMockFetcher(StrCat(kTestDomain, "a.png"), kBikePngFile,
                       kContentTypePng, 100);
  AddFileToMockFetcher(StrCat(kTestDomain, "b.jpeg"), kPuzzleJpgFile,
                       kContentTypeJpeg, 100);
  factory()->wait_url_async_fetcher()->DoNotDelay(StrCat(kTestDomain, "a.png"));

  SetupWriter();
  rewrite_driver()->StartParse(StrCat(kTestDomain, "dims.html"));
  rewrite_driver()->ParseText("<img src=\"a.png\"><img src=\"b.jpeg\">");
  rewrite_driver()->FinishParse();

  GoogleString out_png_url(Encode("", "ic", "0", "a.png", "jpg"));
  GoogleString out_jpeg_url(Encode("", "ic", "0", "b.jpeg", "jpg"));
  GoogleString js1 = StrCat(
      "psMobStaticImageInfo = {"
      "\"", kTestDomain, out_png_url, "\":{w:100,h:100},"
      "}");
  GoogleString js2 = StrCat(
      "psMobStaticImageInfo = {"
      "\"", kTestDomain, out_png_url, "\":{w:100,h:100},"
      "\"", kTestDomain, out_jpeg_url, "\":{w:1023,h:766},"
      "}");
  EXPECT_EQ(StrCat("<img src=\"", out_png_url, "\">",
                   "<img src=\"b.jpeg\">",
                   "<script>", js1, "</script>"),
            output_buffer_);

  CallFetcherCallbacks();

  // Next time all is available.
  output_buffer_.clear();
  SetupWriter();
  rewrite_driver()->StartParse(StrCat(kTestDomain, "dims2.html"));
  rewrite_driver()->ParseText("<img src=\"a.png\"><img src=\"b.jpeg\">");
  rewrite_driver()->FinishParse();
  EXPECT_EQ(StrCat("<img src=\"", out_png_url, "\">",
                   "<img src=\"", out_jpeg_url, "\">",
                   "<script>", js2, "</script>"),
            output_buffer_);
}

TEST_F(ImageRewriteTest, DebugMessageImageInfo) {
  options()->EnableFilter(RewriteOptions::kDebug);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  options()->EnableFilter(RewriteOptions::kConvertToWebpAnimated);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  rewrite_driver()->AddFilters();
  AddFileToMockFetcher("photo_opaque.gif", kChefGifFile, kContentTypeGif,
                       100);
  AddFileToMockFetcher("graphic_transparent.png", kCuppaTPngFile,
                       kContentTypePng, 100);
  AddFileToMockFetcher("animated.gif", kCradleAnimation, kContentTypeGif, 100);

  Parse("single_attribute", "<img src=photo_opaque.gif>"
        "<img src=graphic_transparent.png><img src=animated.gif>");

  const GoogleString expected = StrCat(
      "<img src=", Encode("", "ic", "0", "photo_opaque.gif", "png"), ">"
      "<!--Image http://test.com/photo_opaque.gif "
      "does not appear to need resizing.-->"
      "<!--Image http://test.com/photo_opaque.gif "
      "has no transparent pixels, is not sensitive to compression "
      "noise, and has no animation.-->"
      "<img src=graphic_transparent.png>"
      "<!--Image http://test.com/graphic_transparent.png "
      "does not appear to need resizing.-->"
      "<!--Image http://test.com/graphic_transparent.png "
      "has transparent pixels, is sensitive to compression noise, "
      "and has no animation.-->"
      "<img src=animated.gif>"
      "<!--Image http://test.com/animated.gif "
      "does not appear to need resizing.-->"
      "<!--Image http://test.com/animated.gif "
      "has no transparent pixels, is sensitive to compression noise, "
      "and has animation.-->");

  EXPECT_THAT(output_buffer_, HasSubstr(expected));
}

TEST_F(ImageRewriteTest, DebugMessageInline) {
  options()->set_image_inline_max_bytes(100);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  options()->EnableFilter(RewriteOptions::kDebug);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  GoogleString initial_url = StrCat(kTestDomain, kChefGifFile);
  GoogleString page_url = StrCat(kTestDomain, "test.html");
  AddFileToMockFetcher(initial_url, kChefGifFile, kContentTypeGif, 100);
  const char html_boilerplate[] = "<img src='%s' width='10' height='12'>";
  GoogleString html_input = StringPrintf(html_boilerplate, initial_url.c_str());

  ParseUrl(page_url, html_input);

  const char kInlineMessage[] =
      "The image was not inlined because it has too many bytes.";
  EXPECT_THAT(output_buffer_, HasSubstr(kInlineMessage));
}

TEST_F(ImageRewriteTest, DebugMessageUnauthorized) {
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kDebug);
  rewrite_driver()->AddFilters();
  const char kAuthorizedPath[] = "http://test.com/photo_opaque.gif";
  const char kUnauthorizedPath[] = "http://unauth.com/photo_opaque.gif";
  AddFileToMockFetcher(kAuthorizedPath, kChefGifFile, kContentTypeGif, 100);
  AddFileToMockFetcher(kUnauthorizedPath, kChefGifFile, kContentTypeGif, 100);

  Parse("unauthorized_domain", StrCat("<img src=", kAuthorizedPath, ">"
                                      "<img src=", kUnauthorizedPath, ">"));

  GoogleUrl unauth_gurl(kUnauthorizedPath);
  const GoogleString expected = StrCat(
      "<img src=", Encode(kTestDomain, "ic", "0", "photo_opaque.gif", "png"),
      ">"
      "<!--Image http://test.com/photo_opaque.gif "
      "does not appear to need resizing.-->"
      "<!--Image http://test.com/photo_opaque.gif "
      "has no transparent pixels, is not sensitive to compression "
      "noise, and has no animation.-->"
      "<img src=", kUnauthorizedPath, ">",
      "<!--",
      RewriteDriver::GenerateUnauthorizedDomainDebugComment(unauth_gurl),
      "-->");

  EXPECT_THAT(output_buffer_, HasSubstr(expected));
}

// Chrome on iPhone rewrites a photo-like GIF to lossy WebP but cannot inline
// it.
TEST_F(ImageRewriteTest, ChromeIphoneOutlinesWebP) {
  TestInlining(true, UserAgentMatcherTestBase::kIPhoneChrome36UserAgent,
               kChefGifFile, kContentTypeGif, kContentTypeWebp, false);
}

// Chrome on iPad rewrites a graphics-like PNG to lossless WebP but cannot
// inline it.
TEST_F(ImageRewriteTest, ChromeIpadInlinesPng) {
  TestInlining(true, UserAgentMatcherTestBase::kIPadChrome36UserAgent,
               kCuppaTPngFile, kContentTypePng, kContentTypeWebp, false);
}

// Chrome on iPad rewrites a JPEG to lossy WebP but cannot inline it.
TEST_F(ImageRewriteTest, ChromeIpadOutlinesWebp) {
  TestInlining(true, UserAgentMatcherTestBase::kIPadChrome36UserAgent,
               kPuzzleJpgFile, kContentTypeJpeg, kContentTypeWebp, false);
}

// Chrome on iPhone rewrites a graphics-like PNG to another PNG and inlines it.
TEST_F(ImageRewriteTest, ChromeIphoneInlinesPng) {
  TestInlining(false, UserAgentMatcherTestBase::kIPhoneChrome36UserAgent,
               kCuppaPngFile, kContentTypePng, kContentTypePng, true);
}

// Chrome on iPad rewrites a JPEG to another JPEG and inlines it.
TEST_F(ImageRewriteTest, ChromeIpadInlinesJpeg) {
  TestInlining(false, UserAgentMatcherTestBase::kIPadChrome36UserAgent,
               kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg, true);
}

// Safari on iPhone rewrites a photo-like GIF to JPEG and inlines it.
TEST_F(ImageRewriteTest, SafariIphoneInlinesJpeg) {
  TestInlining(false, UserAgentMatcherTestBase::kIPhone4Safari,
               kChefGifFile, kContentTypeGif, kContentTypeJpeg, true);
}

// Chrome on Android rewrites a photo-like PNG to lossy WebP and inlines it.
TEST_F(ImageRewriteTest, ChromeAndroidInlinesWebP) {
  TestInlining(true, UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
               kChefGifFile, kContentTypeGif, kContentTypeWebp, true);
}

// Chrome on desktop rewrites a JPEG to lossy WebP and inlines it.
TEST_F(ImageRewriteTest, ChromeDesktopInlinesWebp) {
  TestInlining(true, UserAgentMatcherTestBase::kChrome18UserAgent,
               kPuzzleJpgFile, kContentTypeJpeg, kContentTypeWebp, true);
}

// Chrome on Android rewrites a graphics-like PNG to lossless WebP and
// inlines it.
TEST_F(ImageRewriteTest, ChromeAndroidInlinesLosslessWebp) {
  TestInlining(true, UserAgentMatcherTestBase::kNexus10ChromeUserAgent,
               kCuppaTPngFile, kContentTypePng, kContentTypeWebp, true);
}

TEST_F(ImageRewriteTest, PngExceedResolutionLimit) {
  TestResolutionLimit(kResolutionLimitBytes - 1, kResolutionLimitPngFile,
                      kContentTypePng, false /*try_webp*/,
                      false /*try_resize*/, false /*expect_rewritten*/);
}

TEST_F(ImageRewriteTest, JpegExceedResolutionLimit) {
  TestResolutionLimit(kResolutionLimitBytes - 1, kResolutionLimitJpegFile,
                      kContentTypeJpeg, false /*try_webp*/,
                      false /*try_resize*/, false /*expect_rewritten*/);
}

TEST_F(ImageRewriteTest, PngInResolutionLimit) {
  if (RunningOnValgrind()) {
    return;
  }

  TestResolutionLimit(kResolutionLimitBytes, kResolutionLimitPngFile,
                      kContentTypePng, true /*try_webp*/, true /*try_resize*/,
                      true /*expect_rewritten*/);
}

TEST_F(ImageRewriteTest, PngInResolutionLimitNoResizing) {
  if (RunningOnValgrind()) {
    return;
  }

  TestResolutionLimit(kResolutionLimitBytes, kResolutionLimitPngFile,
                      kContentTypePng, true /*try_webp*/,
                      false /*try_resize*/, true /*expect_rewritten*/);
}

TEST_F(ImageRewriteTest, JpegInResolutionLimit) {
  if (RunningOnValgrind()) {
    return;
  }

  TestResolutionLimit(kResolutionLimitBytes, kResolutionLimitJpegFile,
                      kContentTypeJpeg, true /*try_webp*/,
                      true /*try_resize*/, true /*expect_rewritten*/);
}

TEST_F(ImageRewriteTest, JpegInResolutionLimitNoResizing) {
  if (RunningOnValgrind()) {
    return;
  }

  TestResolutionLimit(kResolutionLimitBytes, kResolutionLimitJpegFile,
                      kContentTypeJpeg, true /*try_webp*/,
                      false /*try_resize*/, true /*expect_rewritten*/);
}

TEST_F(ImageRewriteTest, AnimatedGifToWebpWithWebpAnimatedUa) {
  if (RunningOnValgrind()) {
    return;
  }

  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->EnableFilter(RewriteOptions::kConvertToWebpAnimated);
  options()->set_image_recompress_quality(85);
  rewrite_driver()->AddFilters();
  SetupForWebpAnimated();
  TestSingleRewrite(kCradleAnimation, kContentTypeGif, kContentTypeWebp,
                    "", " width=\"200\" height=\"150\"", true, false);

  TestConversionVariables(0, 0, 0,   // gif
                          0, 0, 0,   // png
                          0, 0, 0,   // jpg
                          0, 1, 0,   // gif animated
                          true);
}

TEST_F(ImageRewriteTest, AnimatedGifToWebpWithWebpLaUa) {
  if (RunningOnValgrind()) {
    return;
  }

  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->EnableFilter(RewriteOptions::kConvertToWebpAnimated);
  options()->set_image_recompress_quality(85);
  rewrite_driver()->AddFilters();
  SetupForWebpLossless();
  TestSingleRewrite(kCradleAnimation, kContentTypeGif, kContentTypeGif,
                    "", " width=\"200\" height=\"150\"", false, false);
  TestConversionVariables(0, 0, 0,   // gif
                          0, 0, 0,   // png
                          0, 0, 0,   // jpg
                          0, 0, 0,   // gif animated
                          false);
}

TEST_F(ImageRewriteTest, AnimatedGifToWebpNotEnabled) {
  if (RunningOnValgrind()) {
    return;
  }

  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->EnableFilter(RewriteOptions::kConvertToWebpLossless);
  options()->set_image_recompress_quality(85);
  rewrite_driver()->AddFilters();
  SetupForWebpAnimated();
  TestSingleRewrite(kCradleAnimation, kContentTypeGif, kContentTypeGif,
                    "", " width=\"200\" height=\"150\"", false, false);
  TestConversionVariables(0, 0, 0,   // gif
                          0, 0, 0,   // png
                          0, 0, 0,   // jpg
                          0, 0, 0,   // gif animated
                          false);
}

TEST_F(ImageRewriteTest, GifToWebpLosslessWithWebpAnimatedUa) {
  if (RunningOnValgrind()) {
    return;
  }

  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kConvertToWebpAnimated);
  options()->set_image_recompress_quality(85);
  rewrite_driver()->AddFilters();
  SetupForWebpAnimated();
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeWebp,
                    "", " width=\"192\" height=\"256\"", true, false);
  TestConversionVariables(0, 1, 0,   // gif
                          0, 0, 0,   // png
                          0, 0, 0,   // jpg
                          0, 0, 0,   // gif animated
                          true);
}

TEST_F(ImageRewriteTest, AnimatedNoCacheReuse) {
  // Make sure we don't reuse results for animated webp-capable UAs for
  // non-webp targets.
  AddFileToMockFetcher(StrCat(kTestDomain, "a.jpeg"), kPuzzleJpgFile,
                       kContentTypeJpeg, 100);

  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kConvertToWebpAnimated);
  options()->set_image_recompress_quality(85);
  rewrite_driver()->AddFilters();

  // WebP capable browser --- made a WebP image.
  SetupForWebpAnimated();
  ValidateExpected("webp broswer", "<img src=a.jpeg>",
                   "<img src=xa.jpeg.pagespeed.ic.0.webp>");
  ClearRewriteDriver();

  // Not a WebP browser -- don't!
  SetCurrentUserAgent("curl");
  ValidateNoChanges("non-webp broswer", "<img src=a.jpeg>");
}

// Make sure that we optimize images to the correct format and correct quality,
// and add the correct "Vary" response header.
//
// Test 4 images:
//   - JPEG (optimized to lossy format)
//   - PNG image with photographic content (optimized to lossy format)
//   - PNG image with non-photographic content (optimized to lossless format)
//   - Animated GIF (optimized to animated WebP)
//
// Use 3 user-agents:
//   - Chrome on Android (mobile and supports all formats, including WebP)
//   - Safari on iOS (mobile but doesn't support WebP)
//   - Firefox (neither mobile nor supports WebP)
//
// Check 2 headers:
//   - Save-Data header
//   - Via header
//
// To make sure that we don't have cache collision, each image is fetched twice,
// with other image fetching in between.
TEST_F(ImageRewriteTest, IproAllowAuto) {
  if (RunningOnValgrind()) {
    return;
  }

  SetupIproTests("Auto");
  rewrite_driver()->AddFilters();

  // Fetch each image twice, to make sure no cache collision.
  for (int i = 0; i < 2; ++i) {
    // Test the combination of 4 images and 3 user-agents.
    for (int j = 0; j < 12; ++j) {
      const char* image_name = kOptimizedImageInfoList[j].image_name;
      const char* user_agent = kOptimizedImageInfoList[j].user_agent;
      const OptimizedImageInfoList& optimized_info =
          *kOptimizedImageInfoList[j].optimized_info;
      // Test the combination of 2 headers (each header can be on or off).
      IproFetchAndValidateWithHeaders(image_name, user_agent, optimized_info);
    }
  }
}

// Test when we can vary on "Accept,Save-Data".
TEST_F(ImageRewriteTest, IproAllowSaveDataAccept) {
  if (RunningOnValgrind()) {
    return;
  }

  SetupIproTests("Accept,Save-Data");
  rewrite_driver()->AddFilters();
  IproFetchAndValidateWithHeaders(
      kPuzzleJpgFile, UserAgentMatcherTestBase::kNexus6Chrome44UserAgent,
      kPuzzleOptimizedForWebpUaAllowSaveDataAccept);
}

// Test when we can vary on "User-Agent".
TEST_F(ImageRewriteTest, IproAllowUserAgent) {
  if (RunningOnValgrind()) {
    return;
  }

  SetupIproTests("User-Agent");
  rewrite_driver()->AddFilters();
  IproFetchAndValidateWithHeaders(
      kPuzzleJpgFile, UserAgentMatcherTestBase::kNexus6Chrome44UserAgent,
      kPuzzleOptimizedForWebpUaAllowUserAgent);
}

// Test when we can vary on "Accept".
TEST_F(ImageRewriteTest, IproAllowAccept) {
  if (RunningOnValgrind()) {
    return;
  }

  SetupIproTests("Accept");
  rewrite_driver()->AddFilters();
  IproFetchAndValidateWithHeaders(
      kPuzzleJpgFile, UserAgentMatcherTestBase::kNexus6Chrome44UserAgent,
      kPuzzleOptimizedForWebpUaAllowAccept);
}

// Test when we can vary on "Save-Data".
TEST_F(ImageRewriteTest, IproAllowSaveData) {
  if (RunningOnValgrind()) {
    return;
  }

  SetupIproTests("Save-Data");
  rewrite_driver()->AddFilters();
  IproFetchAndValidateWithHeaders(
      kPuzzleJpgFile, UserAgentMatcherTestBase::kNexus6Chrome44UserAgent,
      kPuzzleOptimizedForWebpUaAllowSaveData);
}

// Test when we cannot vary on anything.
TEST_F(ImageRewriteTest, IproAllowNone) {
  if (RunningOnValgrind()) {
    return;
  }

  SetupIproTests("None");
  rewrite_driver()->AddFilters();
  IproFetchAndValidateWithHeaders(
      kPuzzleJpgFile, UserAgentMatcherTestBase::kNexus6Chrome44UserAgent,
      kPuzzleOptimizedForWebpUaAllowNone);
}

// Test when the qualities for Save-Data are undefined.
TEST_F(ImageRewriteTest, IproAllowAutoNoSaveDataQualities) {
  if (RunningOnValgrind()) {
    return;
  }

  SetupIproTests("Auto");
  options()->set_image_jpeg_quality_for_save_data(-1);
  options()->set_image_webp_quality_for_save_data(-1);
  rewrite_driver()->AddFilters();
  IproFetchAndValidateWithHeaders(
      kPuzzleJpgFile, UserAgentMatcherTestBase::kNexus6Chrome44UserAgent,
      kPuzzleOptimizedForWebpUaNoSaveDataQualities);
}

// Test when the qualities for Save-Data are the same as the regular ones.
TEST_F(ImageRewriteTest, IproAllowAutoUnusedSaveDataQualities) {
  if (RunningOnValgrind()) {
    return;
  }

  SetupIproTests("Auto");
  options()->set_image_jpeg_quality_for_save_data(
    options()->ImageJpegQuality());
  options()->set_image_webp_quality_for_save_data(
    options()->ImageWebpQuality());
  rewrite_driver()->AddFilters();
  IproFetchAndValidateWithHeaders(
      kPuzzleJpgFile, UserAgentMatcherTestBase::kNexus6Chrome44UserAgent,
      kPuzzleOptimizedForWebpUaNoSaveDataQualities);
}

// Test when the qualities for small screen are undefined.
TEST_F(ImageRewriteTest, IproAllowAutoNoSmallScreenQualities) {
  if (RunningOnValgrind()) {
    return;
  }

  SetupIproTests("Auto");
  options()->set_image_jpeg_recompress_quality_for_small_screens(-1);
  options()->set_image_webp_recompress_quality_for_small_screens(-1);
  rewrite_driver()->AddFilters();
  IproFetchAndValidateWithHeaders(
      kPuzzleJpgFile, UserAgentMatcherTestBase::kNexus6Chrome44UserAgent,
      kPuzzleOptimizedForWebpUaNoSmallScreenQualities);
}

// Test when neither the qualities for Save-Data nor those for small screens
// are undefined.
TEST_F(ImageRewriteTest, IproAllowAutoNoSmallScreenSaveDataQualities) {
  if (RunningOnValgrind()) {
    return;
  }

  SetupIproTests("Auto");
  options()->set_image_jpeg_quality_for_save_data(-1);
  options()->set_image_webp_quality_for_save_data(-1);
  options()->set_image_jpeg_recompress_quality_for_small_screens(-1);
  options()->set_image_webp_recompress_quality_for_small_screens(-1);
  rewrite_driver()->AddFilters();
  IproFetchAndValidateWithHeaders(
      kPuzzleJpgFile, UserAgentMatcherTestBase::kNexus6Chrome44UserAgent,
      kPuzzleOptimizedForWebpUaNoSpecialQualities);
}

TEST_F(ImageRewriteTest, ContentTypeValidation) {
  ValidateFallbackHeaderSanitization("ic");
}

}  // namespace net_instaweb
