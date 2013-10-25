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

#include <memory>

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/log_record_test_helper.h"
#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/image_testing_peer.h"
#include "net/instaweb/rewriter/public/dom_stats_filter.h"
#include "net/instaweb/rewriter/public/image.h"
#include "net/instaweb/rewriter/public/mock_critical_images_finder.h"
#include "net/instaweb/rewriter/public/request_properties.h"
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
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/dynamic_annotations.h"  // RunningOnValgrind
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/md5_hasher.h"  // for MD5Hasher
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/null_thread_system.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"  // for Timer, etc
#include "pagespeed/kernel/image/test_utils.h"

namespace net_instaweb {

class AbstractMutex;

using pagespeed::image_compression::kMessagePatternPixelFormat;
using pagespeed::image_compression::kMessagePatternStats;
using pagespeed::image_compression::kMessagePatternWritingToWebp;

namespace {

// Filenames of resource files.
const char kBikePngFile[] = "BikeCrashIcn.png";
const char kChefGifFile[] = "IronChef2.gif";
const char kCuppaOPngFile[] = "CuppaO.png";
const char kCuppaTPngFile[] = "CuppaT.png";
const char kLargePngFile[] = "Large.png";
const char kPuzzleJpgFile[] = "Puzzle.jpg";
const char kSmallDataFile[] = "small-data.png";

const char kChefDims[] = " width=\"192\" height=\"256\"";

// Size of a 1x1 image.
const char kPixelDims[] = " width='1' height='1'";

// If the expected value of a size is set to -1, this size will be ignored in
// the test.
const int kIgnoreSize = -1;

const char kCriticalImagesCohort[] = "critical_images";

// Message to ignore.
const char kMessagePatternFailedToEncodeWebp[] = "Could not encode webp data*";
const char kMessagePatternWebpTimeOut[] = "WebP conversion timed out!";

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
    if ((find_result == HTTPCache::kFound) &&
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
      : RequestContext(mutex, NULL),
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
  ImageRewriteTest()
    : test_request_context_(TestRequestContextPtr(
          new TestRequestContext(&logging_info_,
                                 factory()->thread_system()->NewMutex()))) {
  }

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
    const char html_url[] = "http://rewrite_image.test/RewriteImage.html";
    const char image_url[] = "http://rewrite_image.test/Puzzle.jpg";

    const GoogleString image_html =
        StrCat("<head/><body><", tag_string, " src=\"Puzzle.jpg\"/></body>");

    // Store image contents into fetcher.
    AddFileToMockFetcher(image_url, kPuzzleJpgFile, kContentTypeJpeg, 100);
    ParseUrl(html_url, image_html);
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
    // Capture normal headers for comparison. We need to do it now
    // since the clock -after- rewrite is non-deterministic, but it must be
    // at the initial value at the time of the rewrite.
    GoogleString expect_headers;
    AppendDefaultHeaders(content_type, &expect_headers);

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
    http_cache()->Find(img_gurl.Spec().as_string(), message_handler(),
                       &cache_callback);
    cache_callback.ExpectFound();

    // Make sure the headers produced make sense.
    EXPECT_STREQ(expect_headers, rewritten_headers);

    // Also fetch the resource to ensure it can be created dynamically
    ExpectStringAsyncFetch expect_callback(true, CreateRequestContext());
    lru_cache()->Clear();

    // New time --- new timestamp.
    expect_headers.clear();
    AppendDefaultHeaders(content_type, &expect_headers);

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
        rewrite_driver()->CreateInputResourceAbsoluteUnchecked(cuppa_string));
    ASSERT_TRUE(cuppa_resource.get() != NULL);
    EXPECT_TRUE(ReadIfCached(cuppa_resource));
    GoogleString cuppa_contents;
    cuppa_resource->contents().CopyToString(&cuppa_contents);
    // Now make sure axing the original cuppa_string doesn't affect the
    // internals of the cuppa_resource.
    ResourcePtr other_resource(
        rewrite_driver()->CreateInputResourceAbsoluteUnchecked(cuppa_string));
    ASSERT_TRUE(other_resource.get() != NULL);
    cuppa_string.clear();
    EXPECT_TRUE(ReadIfCached(other_resource));
    GoogleString other_contents;
    cuppa_resource->contents().CopyToString(&other_contents);
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
                         const char* initial_dims, const char* final_dims,
                         bool expect_rewritten, bool expect_inline) {
    GoogleString initial_url = StrCat(kTestDomain, name);
    TestSingleRewriteWithoutAbs(initial_url, name, input_type, output_type,
        initial_dims, final_dims, expect_rewritten, expect_inline);
  }

  void TestSingleRewriteWithoutAbs(const GoogleString& initial_url,
                                   const StringPiece& name,
                                   const ContentType& input_type,
                                   const ContentType& output_type,
                                   const char* initial_dims,
                                   const char* final_dims,
                                   bool expect_rewritten,
                                   bool expect_inline) {
    GoogleString page_url = StrCat(kTestDomain, "test.html");
    AddFileToMockFetcher(initial_url, name, input_type, 100);

    const char html_boilerplate[] = "<img src='%s'%s>";
    GoogleString html_input =
        StringPrintf(html_boilerplate, initial_url.c_str(), initial_dims);

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
        StringPrintf(html_boilerplate, rewritten_url.c_str(), final_dims);
    EXPECT_EQ(AddHtmlBody(html_expected_output), output_buffer_);
  }

  // Returns the property cache value for kInlinableImageUrlsPropertyName,
  // or NULL if it is not present.
  const PropertyValue* FetchInlinablePropertyCacheValue() {
    PropertyCache* pcache = page_property_cache();
    if (pcache == NULL) {
      return NULL;
    }
    const PropertyCache::Cohort* cohort = pcache->GetCohort(
        RewriteDriver::kDomCohort);
    if (cohort == NULL) {
      return NULL;
    }
    PropertyPage* property_page = rewrite_driver()->property_page();
    if (property_page == NULL) {
      return NULL;
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

  void TestSquashImagesForMobileScreen(
      RewriteDriver* driver, int screen_width, int screen_height) {
    EXPECT_LT(1, screen_width);
    EXPECT_LT(1, screen_height);
    rewrite_driver()->request_properties()->SetScreenResolution(
        screen_width, screen_height);
    TimedVariable* rewrites_squashing = statistics()->GetTimedVariable(
        ImageRewriteFilter::kImageRewritesSquashingForMobileScreen);
    rewrites_squashing->Clear();

    ImageDim desired_dim;
    ImageDim image_dim;

    // Both image dims are less than screen.
    image_dim.set_width(screen_width - 1);
    image_dim.set_height(screen_height - 1);

    ResourceContext context;
    ImageRewriteFilter image_rewrite_filter(rewrite_driver());
    image_rewrite_filter.EncodeUserAgentIntoResourceContext(&context);

    EXPECT_FALSE(image_rewrite_filter.UpdateDesiredImageDimsIfNecessary(
        image_dim, context, &desired_dim));
    EXPECT_EQ(0, rewrites_squashing->Get(TimedVariable::START));

    // Image height is larger than screen height but image width is less than
    // screen width.
    image_dim.set_width(screen_width - 1);
    image_dim.set_height(screen_height * 2);
    EXPECT_TRUE(image_rewrite_filter.UpdateDesiredImageDimsIfNecessary(
        image_dim, context, &desired_dim));
    EXPECT_FALSE(desired_dim.has_width());
    EXPECT_EQ(screen_height, desired_dim.height());
    desired_dim.clear_height();
    desired_dim.clear_width();
    EXPECT_EQ(1, rewrites_squashing->Get(TimedVariable::START));
    rewrites_squashing->Clear();

    // Image height is less than screen height but image width is larger than
    // screen width.
    image_dim.set_width(screen_width * 2);
    image_dim.set_height(screen_height - 1);
    EXPECT_TRUE(image_rewrite_filter.UpdateDesiredImageDimsIfNecessary(
        image_dim, context, &desired_dim));
    EXPECT_EQ(screen_width, desired_dim.width());
    EXPECT_FALSE(desired_dim.has_height());
    desired_dim.clear_height();
    desired_dim.clear_width();
    EXPECT_EQ(1, rewrites_squashing->Get(TimedVariable::START));
    rewrites_squashing->Clear();

    // Both image dims are larger than screen and screen/image width ratio is
    // is larger than height ratio.
    image_dim.set_width(screen_width * 2);
    image_dim.set_height(screen_height * 3);
    EXPECT_TRUE(image_rewrite_filter.UpdateDesiredImageDimsIfNecessary(
        image_dim, context, &desired_dim));
    EXPECT_FALSE(desired_dim.has_width());
    EXPECT_EQ(screen_height, desired_dim.height());
    desired_dim.clear_height();
    desired_dim.clear_width();
    EXPECT_EQ(1, rewrites_squashing->Get(TimedVariable::START));
    rewrites_squashing->Clear();

    // Both image dims are larger than screen and screen/image height ratio is
    // is larger than width ratio.
    image_dim.set_width(screen_width * 3);
    image_dim.set_height(screen_height * 2);
    EXPECT_TRUE(image_rewrite_filter.UpdateDesiredImageDimsIfNecessary(
        image_dim, context, &desired_dim));
    EXPECT_EQ(screen_width, desired_dim.width());
    EXPECT_FALSE(desired_dim.has_height());
    EXPECT_EQ(1, rewrites_squashing->Get(TimedVariable::START));
    rewrites_squashing->Clear();

    // Keep image dims unchanged and larger than screen from now on and
    // update desired_dim.
    image_dim.set_width(screen_width * 3);
    image_dim.set_height(screen_height * 2);

    // If a desired dim is present, no squashing.
    desired_dim.set_width(screen_width);
    desired_dim.clear_height();
    EXPECT_FALSE(image_rewrite_filter.UpdateDesiredImageDimsIfNecessary(
        image_dim, context, &desired_dim));
    EXPECT_EQ(0, rewrites_squashing->Get(TimedVariable::START));

    desired_dim.clear_width();
    desired_dim.set_height(screen_height);
    EXPECT_FALSE(image_rewrite_filter.UpdateDesiredImageDimsIfNecessary(
        image_dim, context, &desired_dim));
    EXPECT_EQ(0, rewrites_squashing->Get(TimedVariable::START));

    desired_dim.set_width(screen_width);
    desired_dim.set_height(screen_height);
    EXPECT_FALSE(image_rewrite_filter.UpdateDesiredImageDimsIfNecessary(
        image_dim, context, &desired_dim));
    EXPECT_EQ(0, rewrites_squashing->Get(TimedVariable::START));
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

                               bool is_opaque) {
    EXPECT_EQ(
        gif_webp_timeout,
        statistics()->GetVariable(
            net_instaweb::ImageRewriteFilter::kImageWebpFromGifTimeouts)->
        Get());
    EXPECT_EQ(
        gif_webp_success,
        statistics()->GetHistogram(
            net_instaweb::ImageRewriteFilter::kImageWebpFromGifSuccessMs)->
        Count());
    EXPECT_EQ(
        gif_webp_failure,
        statistics()->GetHistogram(
            net_instaweb::ImageRewriteFilter::kImageWebpFromGifFailureMs)->
        Count());

    EXPECT_EQ(
        png_webp_timeout,
        statistics()->GetVariable(
            net_instaweb::ImageRewriteFilter::kImageWebpFromPngTimeouts)->
        Get());
    EXPECT_EQ(
        png_webp_success,
        statistics()->GetHistogram(
            net_instaweb::ImageRewriteFilter::kImageWebpFromPngSuccessMs)->
        Count());
    EXPECT_EQ(
        png_webp_failure,
        statistics()->GetHistogram(
            net_instaweb::ImageRewriteFilter::kImageWebpFromPngFailureMs)->
        Count());

    EXPECT_EQ(
        jpeg_webp_timeout,
        statistics()->GetVariable(
            net_instaweb::ImageRewriteFilter::kImageWebpFromJpegTimeouts)->
        Get());
    EXPECT_EQ(
        jpeg_webp_success,
        statistics()->GetHistogram(
            net_instaweb::ImageRewriteFilter::kImageWebpFromJpegSuccessMs)->
        Count());
    EXPECT_EQ(
        jpeg_webp_failure,
        statistics()->GetHistogram(
            net_instaweb::ImageRewriteFilter::kImageWebpFromJpegFailureMs)->
        Count());

    int total_timeout =
        gif_webp_timeout +
        png_webp_timeout +
        jpeg_webp_timeout;
    int total_success =
        gif_webp_success +
        png_webp_success +
        jpeg_webp_success;
    int total_failure =
        gif_webp_failure +
        png_webp_failure +
        jpeg_webp_failure;

    EXPECT_EQ(
        total_timeout,
        statistics()->GetVariable(
            is_opaque ?
            net_instaweb::ImageRewriteFilter::kImageWebpOpaqueTimeouts :
            net_instaweb::ImageRewriteFilter::kImageWebpWithAlphaTimeouts)->
        Get());
    EXPECT_EQ(
        total_success,
        statistics()->GetHistogram(
            is_opaque ?
            net_instaweb::ImageRewriteFilter::kImageWebpOpaqueSuccessMs :
            net_instaweb::ImageRewriteFilter::kImageWebpWithAlphaSuccessMs)->
        Count());
    EXPECT_EQ(
        total_failure,
        statistics()->GetHistogram(
            is_opaque ?
            net_instaweb::ImageRewriteFilter::kImageWebpOpaqueFailureMs :
            net_instaweb::ImageRewriteFilter::kImageWebpWithAlphaFailureMs)->
        Count());
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

  virtual RequestContextPtr CreateRequestContext() {
    return RequestContextPtr(test_request_context_);
  }

 private:
  LoggingInfo logging_info_;
  TestRequestContextPtr test_request_context_;
};

TEST_F(ImageRewriteTest, ImgTag) {
  RewriteImage("img", kContentTypeJpeg);
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
  rewrite_driver()->SetUserAgent("webp");
  RewriteImage("img", kContentTypeWebp);
}

TEST_F(ImageRewriteTest, ImgTagWebpLa) {
  if (RunningOnValgrind()) {
    return;
  }
  // We use the webp testing user agent; real webp-capable user agents are
  // tested as part of user_agent_matcher_test and are likely to remain in flux
  // over time.
  rewrite_driver()->SetUserAgent("webp-la");
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
  rewrite_driver()->SetUserAgent("webp");
  RewriteImage("input type=\"image\"", kContentTypeWebp);
}

TEST_F(ImageRewriteTest, InputTagWebpLa) {
  if (RunningOnValgrind()) {
    return;
  }
  // We use the webp-la testing user agent; real webp-capable user agents are
  // tested as part of user_agent_matcher_test and are likely to remain in flux
  // over time.
  rewrite_driver()->SetUserAgent("webp-la");

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
  lru_cache()->Delete(StrCat(kTestDomain, kBikePngFile));

  // .. Now make sure we cached dimension insertion properly, and can do it
  // without re-fetching the image.
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng,
                    "", " width=\"100\" height=\"100\"", false, false);
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

TEST_F(ImageRewriteTest, NoDimsInNonImg) {
  // As above, only with an icon.  See:
  // https://code.google.com/p/modpagespeed/issues/detail?id=629
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
  rewrite_driver()->SetUserAgent("webp");
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypeWebp,
                    "", " width=\"100\" height=\"100\"", true, false);
  TestConversionVariables(0, 0, 0,   // gif
                          0, 1, 0,   // png
                          0, 0, 0,   // jpg
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
  rewrite_driver()->SetUserAgent("webp-la");
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypeWebp,
                    "", " width=\"100\" height=\"100\"", true, false);
  TestConversionVariables(0, 0, 0,   // gif
                          0, 1, 0,   // png
                          0, 0, 0,   // jpg
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
  options()->set_allow_logging_urls_in_log_record(true);
  options()->set_image_recompress_quality(85);
  options()->set_log_background_rewrites(true);
  rewrite_driver()->AddFilters();
  rewrite_driver()->SetUserAgent("webp-la");

  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypeWebp,
                    "", " width=\"100\" height=\"100\"", true, false);
  TestConversionVariables(0, 0, 0,   // gif
                          0, 1, 0,   // png
                          0, 0, 0,   // jpg
                          true);

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
      26548, /* original_size */
      kIgnoreSize, /* optimized_size */
      true, /* is_recompressed */
      false, /* is_resized */
      100, /* original width */
      100, /* original height */
      false, /* is_resized_using_rendered_dimensions */
      -1, /* resized_width */
      -1 /* resized_height */);
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
  rewrite_driver()->SetUserAgent("webp-la");
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypeJpeg,
                    "", " width=\"100\" height=\"100\"", true, false);
  TestConversionVariables(0, 0, 0,   // gif
                          1, 0, 0,   // png
                          0, 0, 0,   // jpg
                          true);
}

TEST_F(ImageRewriteTest, DistributedImageRewrite) {
  // Distribute an image rewrite, make sure that the image is resized.
  SetupSharedCache();
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options_->DistributeFilter(RewriteOptions::kImageCompressionId);
  options_->set_distributed_rewrite_servers("example.com:80");
  options_->set_distributed_rewrite_key("1234123");
  other_options()->Merge(*options());
  rewrite_driver()->AddFilters();
  other_rewrite_driver()->AddFilters();
  RequestHeaders request_headers;
  // Set default request headers.
  rewrite_driver()->SetRequestHeaders(request_headers);
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng,
                    " width=10 height=10",  // initial_dims,
                    " width=10 height=10",  // final_dims,
                    true,                   // expect_rewritten
                    false);                 // expect_inline
  EXPECT_EQ(1, statistics()->GetVariable(
                   RewriteContext::kNumDistributedRewriteSuccesses)->Get());
}

TEST_F(ImageRewriteTest, DistributedImageInline) {
  // Distribute an image rewrite, make sure that the inlined image is used from
  // the returned metadata.
  SetupSharedCache();
  options()->set_image_inline_max_bytes(1000000);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options_->DistributeFilter(RewriteOptions::kImageCompressionId);
  options_->set_distributed_rewrite_servers("example.com:80");
  options_->set_distributed_rewrite_key("1234123");
  other_options()->Merge(*options());
  rewrite_driver()->AddFilters();
  other_rewrite_driver()->AddFilters();
  RequestHeaders request_headers;
  // Set default request headers.
  rewrite_driver()->SetRequestHeaders(request_headers);
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng, "", "",
                    true,   // expect_rewritten
                    true);  // expect_inline
  EXPECT_EQ(1, statistics()->GetVariable(
                   RewriteContext::kNumDistributedRewriteSuccesses)->Get());
  GoogleString distributed_output = output_buffer_;

  // Run it again but this time without distributed rewriting, the output should
  // be the same.
  lru_cache()->Clear();
  ClearStats();
  // Clearing the distributed_rewrite_servers disables distribution.
  options()->ClearSignatureForTesting();
  options_->set_distributed_rewrite_servers("");
  options()->ComputeSignature();
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng, "", "",
                    true,   // expect_rewritten
                    true);  // expect_inline
  EXPECT_EQ(0, statistics()->GetVariable(
                   RewriteContext::kNumDistributedRewriteSuccesses)->Get());
  // Make sure we did a rewrite.
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());

  // Is the output from distributed rewriting and local rewriting the same?
  EXPECT_STREQ(distributed_output, output_buffer_);
}

TEST_F(ImageRewriteTest, ImageRewritePreserveURLsOn) {
  // Make sure that the image URL stays the same.
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kResizeImages);
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

TEST_F(ImageRewriteTest, ImageRewriteInlinePreserveURLs) {
  // Willing to inline large files.
  options()->set_image_inline_max_bytes(1000000);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  options()->set_image_preserve_urls(true);
  rewrite_driver()->AddFilters();
  const char kResizedDims[] = " width=48 height=64";
  // File would be inlined without preserve urls, make sure it's not!
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    kResizedDims, kResizedDims, false, false);
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

TEST_F(ImageRewriteTest, ImageRewriteNoTransformAttribute) {
  // Make sure that the image stays the same and that the attribute is stripped.
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  rewrite_driver()->AddFilters();
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng,
                    "pagespeed_no_transform",       // initial attributes
                    "",                             // final attributes
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
      net_instaweb::ImageRewriteFilter::kImageNoRewritesHighResolution);
  EXPECT_EQ(1, no_rewrites->Get());
}

TEST_F(ImageRewriteTest, DimensionParsingOK) {
  // First some tests that should succeed.
  int value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(
      "5", &value));
  EXPECT_EQ(value, 5);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(
      " 341  ", &value));
  EXPECT_EQ(value, 341);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(
      " 000743  ", &value));
  EXPECT_EQ(value, 743);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(
      "\n\r\t \f62", &value));
  EXPECT_EQ(value, 62);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(
      "+40", &value));
  EXPECT_EQ(value, 40);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(
      " +41", &value));
  EXPECT_EQ(value, 41);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(
      "54px", &value));
  EXPECT_EQ(value, 54);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(
      "  70.", &value));
  EXPECT_EQ(value, 70);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(
      "71.3", &value));
  EXPECT_EQ(value, 71);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(
      "71.523", &value));
  EXPECT_EQ(value, 72);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(
      "73.4999990982589729048572938579287459874", &value));
  EXPECT_EQ(value, 73);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(
      "75.px", &value));
  EXPECT_EQ(value, 75);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(
      "75.6 px", &value));
  EXPECT_EQ(value, 76);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(
      "77.34px", &value));
  EXPECT_EQ(value, 77);
  value = -34;
  EXPECT_TRUE(ImageRewriteFilter::ParseDimensionAttribute(
      "78px ", &value));
  EXPECT_EQ(value, 78);
}

TEST_F(ImageRewriteTest, DimensionParsingFail) {
  int value = -34;
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "0", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "+0", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "+0.9", &value));  // Bizarrely not allowed!
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "  0  ", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "junk5", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "  junk10", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "junk  50", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "-43", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "+ 43", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "21px%", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "21px junk", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "9123948572038209720561049018365037891046", &value));
  EXPECT_EQ(-34, value);
  // We don't handle percentages because we can't resize them.
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "73%", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "43.2 %", &value));
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
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "21%px", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "59 .", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "60 . 9", &value));  // 60 today
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "+61. 9", &value));  // 61 today
  EXPECT_EQ(-34, value);
  // Some other units that some old browsers treat as px, but we just ignore
  // to avoid confusion / inconsistency.
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "29in", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "30cm", &value));
  EXPECT_EQ(-34, value);
  EXPECT_FALSE(ImageRewriteFilter::ParseDimensionAttribute(
      "43pt", &value));
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
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  const char kOrigDims[] = " width=65 height=70";
  const char kResizedDims[] = " width=26 height=28";
  // At natural size, we should inline and erase dimensions.
  TestSingleRewrite(kCuppaTPngFile, kContentTypePng, kContentTypePng,
                    kOrigDims, "", false, true);
  // Image is inlined but not resized, so preserve dimensions.
  TestSingleRewrite(kCuppaTPngFile, kContentTypePng, kContentTypePng,
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
  const char html_url[] = "http://image.test/base_url.html";
  const char png_url[]  = "http://other_domain.test/foo/bar/a.png";
  const char jpeg_url[] = "http://other_domain.test/baz/b.jpeg";
  const char gif_url[]  = "http://other_domain.test/foo/c.gif";

  AddFileToMockFetcher(png_url, kBikePngFile, kContentTypePng, 100);
  AddFileToMockFetcher(jpeg_url, kPuzzleJpgFile, kContentTypeJpeg, 100);
  AddFileToMockFetcher(gif_url, kChefGifFile, kContentTypeGif, 100);

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
  ParseUrl(html_url, html_input);

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
  ValidateNoChanges("404", "<img src='404.jpg'>");

  // Try again to exercise cached case.
  ValidateNoChanges("404", "<img src='404.jpg'>");
}

TEST_F(ImageRewriteTest, HonorNoTransform) {
  // If cache-control: no-transform then we should serve the original URL
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  rewrite_driver()->AddFilters();

  GoogleString url = StrCat(kTestDomain, "notransform.png");
  AddFileToMockFetcher(url, kBikePngFile, kContentTypePng, 100);
  AddToResponse(url, HttpAttributes::kCacheControl, "no-transform");

  ValidateNoChanges("NoTransform1", StrCat("<img src=", url, ">"));
  // Validate twice in case changes in cache from the first request alter the
  // second.
  ValidateNoChanges("NoTransform2", StrCat("<img src=", url, ">"));
}

TEST_F(ImageRewriteTest, YesTransform) {
  // Replicates above test but without no-transform to show that it works.
  // We also verfiy that the pagespeed_no_defer attribute doesn't get removed
  // when we rewrite images.
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  rewrite_driver()->AddFilters();

  GoogleString url = StrCat(kTestDomain, "notransform.png");
  AddFileToMockFetcher(url, kBikePngFile,
                       kContentTypePng, 100);
  ValidateExpected("YesTransform",
                   StrCat("<img src=", url, " pagespeed_no_defer>"),
                   StrCat("<img src=",
                          Encode("http://test.com/", "ic", "0",
                                 "notransform.png", "png"),
                          " pagespeed_no_defer>"));
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

// http://code.google.com/p/modpagespeed/issues/detail?id=324
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

  // Set the current # of rewrites very high, so we stop doing more
  // due to "load".
  Variable* ongoing_rewrites =
      statistics()->GetVariable(ImageRewriteFilter::kImageOngoingRewrites);
  ongoing_rewrites->Set(100);

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
  ongoing_rewrites->Set(0);
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
  rewrite_driver()->SetUserAgent("webp-la");
  const char kResizedDims[] = " width=48 height=64";
  // With resize and optimization
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeWebp,
                    kResizedDims, kResizedDims, true, false);
  TestConversionVariables(0, 1, 0,   // gif
                          0, 0, 0,   // png
                          0, 0, 0,   // jpg
                          true);
}

TEST_F(ImageRewriteTest, GifToWebpTestWithoutResizeWithOptimize) {
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->set_image_recompress_quality(85);
  rewrite_driver()->AddFilters();
  rewrite_driver()->SetUserAgent("webp-la");
  // Without resize and with optimization
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeWebp,
                    "", "", true, false);
  TestConversionVariables(0, 1, 0,   // gif
                          0, 0, 0,   // png
                          0, 0, 0,   // jpg
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
      net_instaweb::ImageRewriteFilter::kImageRewritesDroppedNoSavingNoResize);
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
      net_instaweb::ImageRewriteFilter::kImageRewritesDroppedMIMETypeUnknown);
  EXPECT_EQ(1, rewrites_drops->Get());
}

TEST_F(ImageRewriteTest, SquashImagesForMobileScreen) {
  // Make sure squash_images_for_mobile_screen works for mobile user agent
  // (we test Android and iPhone specifically here) and no-op for desktop user
  // agent (using Safari as example).
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->EnableFilter(RewriteOptions::kSquashImagesForMobileScreen);
  rewrite_driver()->AddFilters();

  int screen_width;
  int screen_height;
  ImageUrlEncoder::GetNormalizedScreenResolution(
      100, 80, &screen_width, &screen_height);
  rewrite_driver()->SetUserAgent(
      UserAgentMatcherTestBase::kAndroidNexusSUserAgent);

  TestSquashImagesForMobileScreen(
      rewrite_driver(), screen_width, screen_height);

  rewrite_driver()->SetUserAgent(
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_6_8) AppleWebKit/537.13+ "
      "(KHTML, like Gecko) Version/5.1.7 Safari/534.57.2");
  rewrite_driver()->request_properties()->SetScreenResolution(
      screen_width, screen_height);

  ImageDim desired_dim;
  ImageDim image_dim;
  // Both image dims are larger than screen but no update since not mobile.
  image_dim.set_width(screen_width + 100);
  image_dim.set_height(screen_height + 100);
  ImageRewriteFilter image_rewrite_filter(rewrite_driver());
  ResourceContext context;
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&context);
  EXPECT_FALSE(image_rewrite_filter.UpdateDesiredImageDimsIfNecessary(
      image_dim, context, &desired_dim));

  rewrite_driver()->SetUserAgent("iPhone OS");
  TestSquashImagesForMobileScreen(
      rewrite_driver(), screen_width, screen_height);
}

TEST_F(ImageRewriteTest, JpegQualityForSmallScreens) {
  rewrite_driver()->SetUserAgent("Mozilla/5.0 (Linux; U; Android 4.0.1; en-us; "
      "Galaxy Nexus Build/ICL27) AppleWebKit/534.30 (KHTML, like Gecko) "
      "Version/4.0 Mobile Safari/534.30");
  ImageRewriteFilter image_rewrite_filter(rewrite_driver());
  ResourceContext ctx;
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  const ResourcePtr res_ptr(rewrite_driver()->
      CreateInputResourceAbsoluteUnchecked("data:image/png;base64,test"));
  scoped_ptr<Image::CompressionOptions> img_options(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));

  // Neither option is set explicitly, default is 70.
  EXPECT_EQ(70, img_options->jpeg_quality);
  EXPECT_TRUE(ctx.has_use_small_screen_quality());

  // Base image quality is set, but for_small_screens is not, return base.
  options()->ClearSignatureForTesting();
  options()->set_image_jpeg_recompress_quality_for_small_screens(-1);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(85, img_options->jpeg_quality);
  EXPECT_TRUE(ctx.has_use_small_screen_quality());

  // Base jpeg quality not set, but for_small_screens is, return small_screen.
  options()->ClearSignatureForTesting();
  options()->set_image_recompress_quality(-1);
  options()->set_image_jpeg_recompress_quality_for_small_screens(20);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(20, img_options->jpeg_quality);
  EXPECT_TRUE(ctx.has_use_small_screen_quality());

  // Neither jpeg quality is set, return -1.
  options()->ClearSignatureForTesting();
  options()->set_image_recompress_quality(-1);
  options()->set_image_jpeg_recompress_quality_for_small_screens(-1);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(-1, img_options->jpeg_quality);
  EXPECT_TRUE(ctx.has_use_small_screen_quality());

  // Base and for_small_screen options are set, and screen is small;
  options()->ClearSignatureForTesting();
  options()->set_image_jpeg_recompress_quality(85);
  options()->set_image_jpeg_recompress_quality_for_small_screens(20);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(20, img_options->jpeg_quality);
  EXPECT_TRUE(ctx.has_use_small_screen_quality());

  // Base and for_small_screen options are set, but screen is not small.
  rewrite_driver()->SetUserAgent("Mozilla/5.0 (Linux; U; Android 4.2; en-us; "
      "Nexus 10 Build/JOP12D) AppleWebKit/534.30 (KHTML, like Gecko) "
      "Version/4.0 Safari/534.30");
  ctx.Clear();
  options()->ClearSignatureForTesting();
  options()->set_image_jpeg_recompress_quality(85);
  options()->set_image_jpeg_recompress_quality_for_small_screens(20);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(85, img_options->jpeg_quality);
  EXPECT_FALSE(ctx.has_use_small_screen_quality());

  // Small screen following big screen.
  rewrite_driver()->SetUserAgent("Mozilla/5.0 (Linux; U; Android 4.0.1; en-us; "
      "Galaxy Nexus Build/ICL27) AppleWebKit/534.30 (KHTML, like Gecko) "
      "Version/4.0 Mobile Safari/534.30");
  ctx.Clear();
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(20, img_options->jpeg_quality);
  EXPECT_TRUE(ctx.has_use_small_screen_quality());

  // Big screen following small screen.
  rewrite_driver()->SetUserAgent("Mozilla/5.0 (Linux; U; Android 4.2; en-us; "
      "Nexus 10 Build/JOP12D) AppleWebKit/534.30 (KHTML, like Gecko) "
      "Version/4.0 Safari/534.30");
  ctx.Clear();
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(85, img_options->jpeg_quality);
  EXPECT_FALSE(ctx.has_use_small_screen_quality());

  // Non-mobile UA.
  rewrite_driver()->SetUserAgent("Mozilla/5.0 (Windows; U; Windows NT 5.1; "
      "en-US) AppleWebKit/525.13 (KHTML, like Gecko) Chrome/0.A.B.C "
      "Safari/525.13");
  ctx.Clear();
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(85, img_options->jpeg_quality);
  EXPECT_FALSE(ctx.use_small_screen_quality());

  // Mobile UA
  rewrite_driver()->SetUserAgent("iPhone OS Safari");
  options()->ClearSignatureForTesting();
  options()->set_image_jpeg_recompress_quality_for_small_screens(70);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(70, img_options->jpeg_quality);
  EXPECT_TRUE(ctx.use_small_screen_quality());

  // Min of small screen and desktop
  rewrite_driver()->SetUserAgent("iPhone OS Safari");
  options()->ClearSignatureForTesting();
  options()->set_image_jpeg_recompress_quality_for_small_screens(70);
  options()->set_image_jpeg_recompress_quality(60);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(60, img_options->jpeg_quality);
  EXPECT_TRUE(ctx.use_small_screen_quality());
}

TEST_F(ImageRewriteTest, WebPQualityForSmallScreens) {
  rewrite_driver()->SetUserAgent("Mozilla/5.0 (Linux; U; Android 4.0.1; en-us; "
      "Galaxy Nexus Build/ICL27) AppleWebKit/534.30 (KHTML, like Gecko) "
      "Version/4.0 Mobile Safari/534.30");
  ImageRewriteFilter image_rewrite_filter(rewrite_driver());
  ResourceContext ctx;
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  const ResourcePtr res_ptr(rewrite_driver()->
      CreateInputResourceAbsoluteUnchecked("data:image/png;base64,test"));
  scoped_ptr<Image::CompressionOptions> img_options(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));

  // Neither option is set, default is 70.
  EXPECT_EQ(70, img_options->webp_quality);
  EXPECT_TRUE(ctx.has_use_small_screen_quality());

  // Base webp quality set, but for_small_screens is not, return base quality.
  ctx.Clear();
  options()->ClearSignatureForTesting();
  options()->set_image_webp_recompress_quality(85);
  options()->set_image_webp_recompress_quality_for_small_screens(-1);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(85, img_options->webp_quality);
  EXPECT_TRUE(ctx.has_use_small_screen_quality());

  // Base webp quality not set, but for_small_screens is, return small_screen.
  options()->ClearSignatureForTesting();
  options()->set_image_recompress_quality(-1);
  options()->set_image_webp_recompress_quality(-1);
  options()->set_image_webp_recompress_quality_for_small_screens(20);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(20, img_options->webp_quality);
  EXPECT_TRUE(ctx.has_use_small_screen_quality());

  // Base and for_small_screen options are set, and screen is small;
  options()->ClearSignatureForTesting();
  options()->set_image_webp_recompress_quality(85);
  options()->set_image_webp_recompress_quality_for_small_screens(20);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(20, img_options->webp_quality);
  EXPECT_TRUE(ctx.has_use_small_screen_quality());

  // Base and for_small_screen options are set, but screen is not small.
  rewrite_driver()->SetUserAgent("Mozilla/5.0 (Linux; U; Android 4.2; en-us; "
      "Nexus 10 Build/JOP12D) AppleWebKit/534.30 (KHTML, like Gecko) "
      "Version/4.0 Safari/534.30");
  ctx.Clear();
  options()->ClearSignatureForTesting();
  options()->set_image_webp_recompress_quality(85);
  options()->set_image_webp_recompress_quality_for_small_screens(20);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(85, img_options->webp_quality);
  EXPECT_FALSE(ctx.has_use_small_screen_quality());

  // Small screen following big screen.
  rewrite_driver()->SetUserAgent("Mozilla/5.0 (Linux; U; Android 4.0.1; en-us; "
      "Galaxy Nexus Build/ICL27) AppleWebKit/534.30 (KHTML, like Gecko) "
      "Version/4.0 Mobile Safari/534.30");
  ctx.Clear();
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(20, img_options->webp_quality);
  EXPECT_TRUE(ctx.has_use_small_screen_quality());

  // Big screen following small screen.
  rewrite_driver()->SetUserAgent("Mozilla/5.0 (Linux; U; Android 4.2; en-us; "
      "Nexus 10 Build/JOP12D) AppleWebKit/534.30 (KHTML, like Gecko) "
      "Version/4.0 Safari/534.30");
  ctx.Clear();
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(85, img_options->webp_quality);
  EXPECT_FALSE(ctx.has_use_small_screen_quality());

  // Non-mobile UA.
  rewrite_driver()->SetUserAgent("Mozilla/5.0 (Windows; U; Windows NT 5.1; "
      "en-US) AppleWebKit/525.13 (KHTML, like Gecko) Chrome/0.A.B.C "
      "Safari/525.13");
  ctx.Clear();
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(85, img_options->webp_quality);
  EXPECT_FALSE(ctx.use_small_screen_quality());

  // Mobile UA
  rewrite_driver()->SetUserAgent("iPhone OS Safari");
  ctx.Clear();
  options()->ClearSignatureForTesting();
  options()->set_image_webp_recompress_quality_for_small_screens(70);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(70, img_options->webp_quality);
  EXPECT_TRUE(ctx.use_small_screen_quality());

  // Min of desktop and mobile quality
  rewrite_driver()->SetUserAgent("iPhone OS Safari");
  ctx.Clear();
  options()->ClearSignatureForTesting();
  options()->set_image_webp_recompress_quality_for_small_screens(70);
  options()->set_image_webp_recompress_quality(55);
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  img_options.reset(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));
  EXPECT_EQ(55, img_options->webp_quality);
  EXPECT_TRUE(ctx.use_small_screen_quality());
}

void SetNumberOfScans(int num_scans, int num_scans_small_screen,
                      const ResourcePtr res_ptr,
                      RewriteOptions* options,
                      RewriteDriver* rewrite_driver,
                      ImageRewriteFilter* image_rewrite_filter,
                      ResourceContext* ctx,
                      scoped_ptr<Image::CompressionOptions>* img_options) {
  static const int DO_NOT_SET=-10;
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
          *ctx, res_ptr, false));
}

TEST_F(ImageRewriteTest, JpegProgressiveScansForSmallScreens) {
  static const int DO_NOT_SET=-10;
  rewrite_driver()->SetUserAgent("Mozilla/5.0 (Linux; U; Android 4.0.1; en-us; "
      "Galaxy Nexus Build/ICL27) AppleWebKit/534.30 (KHTML, like Gecko) "
      "Version/4.0 Mobile Safari/534.30");
  ImageRewriteFilter image_rewrite_filter(rewrite_driver());
  ResourceContext ctx;
  image_rewrite_filter.EncodeUserAgentIntoResourceContext(&ctx);
  const ResourcePtr res_ptr(rewrite_driver()->
      CreateInputResourceAbsoluteUnchecked("data:image/png;base64,test"));
  scoped_ptr<Image::CompressionOptions> img_options(
      image_rewrite_filter.ImageOptionsForLoadedResource(ctx, res_ptr, false));

  // Neither option is set, default is -1.
  EXPECT_EQ(-1, img_options->jpeg_num_progressive_scans);
  EXPECT_TRUE(ctx.has_use_small_screen_quality());

  // Base jpeg num scans set, but for_small_screens is not, return
  // base num scans.
  SetNumberOfScans(8, -1, res_ptr, options(), rewrite_driver(),
                   &image_rewrite_filter, &ctx, &img_options);
  EXPECT_EQ(8, img_options->jpeg_num_progressive_scans);
  EXPECT_TRUE(ctx.has_use_small_screen_quality());

  // Base jpeg quality not set, but for_small_screens is, return small_screen.
  SetNumberOfScans(DO_NOT_SET, 2, res_ptr, options(), rewrite_driver(),
                   &image_rewrite_filter, &ctx, &img_options);
  EXPECT_EQ(2, img_options->jpeg_num_progressive_scans);
  EXPECT_TRUE(ctx.has_use_small_screen_quality());

  // Base and for_small_screen options are set, and screen is small;
  SetNumberOfScans(8, 2, res_ptr, options(), rewrite_driver(),
                   &image_rewrite_filter, &ctx, &img_options);
  EXPECT_EQ(2, img_options->jpeg_num_progressive_scans);
  EXPECT_TRUE(ctx.has_use_small_screen_quality());

  // Base and for_small_screen options are set, but screen is not small.
  rewrite_driver()->SetUserAgent("Mozilla/5.0 (Linux; U; Android 4.2; en-us; "
      "Nexus 10 Build/JOP12D) AppleWebKit/534.30 (KHTML, like Gecko) "
      "Version/4.0 Safari/534.30");
  SetNumberOfScans(8, 2, res_ptr, options(), rewrite_driver(),
                   &image_rewrite_filter, &ctx, &img_options);
  EXPECT_EQ(8, img_options->jpeg_num_progressive_scans);
  EXPECT_FALSE(ctx.has_use_small_screen_quality());

  // Small screen following big screen.
  rewrite_driver()->SetUserAgent("Mozilla/5.0 (Linux; U; Android 4.0.1; en-us; "
      "Galaxy Nexus Build/ICL27) AppleWebKit/534.30 (KHTML, like Gecko) "
      "Version/4.0 Mobile Safari/534.30");
  SetNumberOfScans(DO_NOT_SET, DO_NOT_SET, res_ptr, options(), rewrite_driver(),
                   &image_rewrite_filter, &ctx, &img_options);
  EXPECT_EQ(2, img_options->jpeg_num_progressive_scans);
  EXPECT_TRUE(ctx.has_use_small_screen_quality());

  // Big screen following small screen.
  rewrite_driver()->SetUserAgent("Mozilla/5.0 (Linux; U; Android 4.2; en-us; "
      "Nexus 10 Build/JOP12D) AppleWebKit/534.30 (KHTML, like Gecko) "
      "Version/4.0 Safari/534.30");
  SetNumberOfScans(DO_NOT_SET, DO_NOT_SET, res_ptr, options(), rewrite_driver(),
                   &image_rewrite_filter, &ctx, &img_options);
  EXPECT_EQ(8, img_options->jpeg_num_progressive_scans);
  EXPECT_FALSE(ctx.has_use_small_screen_quality());

  // Non-mobile UA.
  rewrite_driver()->SetUserAgent("Mozilla/5.0 (Windows; U; Windows NT 5.1; "
      "en-US) AppleWebKit/525.13 (KHTML, like Gecko) Chrome/0.A.B.C "
      "Safari/525.13");
  SetNumberOfScans(DO_NOT_SET, DO_NOT_SET, res_ptr, options(), rewrite_driver(),
                   &image_rewrite_filter, &ctx, &img_options);
  EXPECT_EQ(8, img_options->jpeg_num_progressive_scans);
  EXPECT_FALSE(ctx.use_small_screen_quality());

  // Mobile UA
  rewrite_driver()->SetUserAgent("iPhone OS Safari");
  SetNumberOfScans(DO_NOT_SET, 2, res_ptr, options(), rewrite_driver(),
                   &image_rewrite_filter, &ctx, &img_options);
  EXPECT_EQ(2, img_options->jpeg_num_progressive_scans);
  EXPECT_TRUE(ctx.use_small_screen_quality());

  // Mobile UA - use min of default and small screen values
  rewrite_driver()->SetUserAgent("iPhone OS Safari");
  SetNumberOfScans(2, 8, res_ptr, options(), rewrite_driver(),
                   &image_rewrite_filter, &ctx, &img_options);
  EXPECT_EQ(2, img_options->jpeg_num_progressive_scans);
  EXPECT_TRUE(ctx.has_use_small_screen_quality());
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
  rewrite_driver()->SetUserAgent("webp");

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
  rewrite_driver()->SetUserAgent("");

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
  GoogleString img_src;
  RewriteImageFromHtml("img", kContentTypeJpeg, &img_src);
  GoogleUrl img_gurl(html_gurl(), img_src);
  EXPECT_STREQ("", img_gurl.Query());
  ResourceNamer namer;
  EXPECT_TRUE(namer.Decode(img_gurl.LeafSansQuery()));
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
                            other_server_context(), NULL));
  ASSERT_EQ(golden_content.size(), remote_content.size());
  EXPECT_EQ(golden_content, remote_content);  // don't bother if sizes differ...
  */
}

// If we drop a rewrite because of load, make sure it returns the original URL.
// This verifies that Issue 707 is fixed.
TEST_F(ImageRewriteTest, TooBusyReturnsOriginalResource) {
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->set_image_max_rewrites_at_once(1);
  rewrite_driver()->AddFilters();

  // Set the current # of rewrites very high, so we stop doing more rewrites
  // due to "load".
  Variable* ongoing_rewrites =
      statistics()->GetVariable(ImageRewriteFilter::kImageOngoingRewrites);
  ongoing_rewrites->Set(100);

  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng, "", "",
                    false, false);
  ongoing_rewrites->Set(0);
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


}  // namespace net_instaweb
