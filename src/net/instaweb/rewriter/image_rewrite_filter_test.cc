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

#include "base/scoped_ptr.h"  // for scoped_ptr
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/image_rewrite_filter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/dynamic_annotations.h"  // RunningOnValgrind
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class AbstractMutex;

namespace {

// Filenames of resource files.
const char kBikePngFile[] = "BikeCrashIcn.png";
const char kPuzzleJpgFile[] = "Puzzle.jpg";
const char kChefGifFile[] = "IronChef2.gif";
const char kCuppaTPngFile[] = "CuppaT.png";
const char kCuppaOPngFile[] = "CuppaO.png";
const char kLargePngFile[] = "Large.png";

// A callback for HTTP cache that stores body and string representation
// of headers into given strings.
class HTTPCacheStringCallback : public OptionsAwareHTTPCacheCallback {
 public:
  HTTPCacheStringCallback(const RewriteOptions* options,
                          GoogleString* body_out, GoogleString* headers_out)
      : OptionsAwareHTTPCacheCallback(options), body_out_(body_out),
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

// By default, CriticalImagesFinder does not return meaningful results. However,
// this test manually manages the critical image set, so CriticalImagesFinder
// can return useful information for testing this filter.
class MeaningfulCriticalImagesFinder : public CriticalImagesFinder {
 public:
  MeaningfulCriticalImagesFinder()
      : compute_calls_(0) {}
  virtual ~MeaningfulCriticalImagesFinder() {}
  virtual bool IsMeaningful() const {
    return true;
  }
  virtual void ComputeCriticalImages(StringPiece url,
                                     RewriteDriver* driver,
                                     bool must_compute) {
    ++compute_calls_;
  }
  int num_compute_calls() { return compute_calls_; }
  virtual const char* GetCriticalImagesCohort() const {
    return kCriticalImagesCohort;
  }

 private:
  static const char kCriticalImagesCohort[];
  int compute_calls_;
  DISALLOW_COPY_AND_ASSIGN(MeaningfulCriticalImagesFinder);
};

const char MeaningfulCriticalImagesFinder::kCriticalImagesCohort[] =
    "critical_images";

class MockPage : public PropertyPage {
 public:
  MockPage(AbstractMutex* mutex, const StringPiece& key)
      : PropertyPage(mutex, key) {}
  virtual ~MockPage() {}
  virtual void Done(bool valid) {}
 private:
  DISALLOW_COPY_AND_ASSIGN(MockPage);
};

class ImageRewriteTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    PropertyCache* pcache = page_property_cache();
    resource_manager_->set_enable_property_cache(true);
    pcache->AddCohort(RewriteDriver::kDomCohort);
    RewriteTestBase::SetUp();
    MockPage* page = new MockPage(factory_->thread_system()->NewMutex(),
                                  kTestDomain);
    pcache->set_enabled(true);
    rewrite_driver()->set_property_page(page);
    pcache->Read(page);
  }

  // Simple image rewrite test to check resource fetching functionality.
  void RewriteImage(const GoogleString& tag_string,
                    const ContentType& content_type) {
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

    // Rewrite the HTML page.
    ParseUrl(html_url, image_html);
    StringVector img_srcs;
    CollectImgSrcs("RewriteImage/collect_sources", output_buffer_, &img_srcs);
    // output_buffer_ should have exactly one image file (Puzzle.jpg).
    EXPECT_EQ(1UL, img_srcs.size());
    // Make sure the next two checks won't abort().
    EXPECT_LT(domain.AllExceptLeaf().size() + 4, img_srcs[0].size());
    const GoogleUrl img_gurl(img_srcs[0]);
    EXPECT_TRUE(img_gurl.is_valid());
    EXPECT_EQ(domain.AllExceptLeaf(), img_gurl.AllExceptLeaf());
    EXPECT_TRUE(img_gurl.LeafSansQuery().ends_with(
        content_type.file_extension()));

    const GoogleString& src_string = img_srcs[0];
    const GoogleString expected_output =
        StrCat("<head/><body><", tag_string, " src=\"", src_string,
               "\" width=\"1023\" height=\"766\"/></body>");
    EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);

    // Fetch the version we just put into the cache, so we can
    // make sure we produce it consistently.
    GoogleString rewritten_image;
    GoogleString rewritten_headers;
    HTTPCacheStringCallback cache_callback(
        options(), &rewritten_image, &rewritten_headers);
    http_cache()->Find(src_string, message_handler(), &cache_callback);
    cache_callback.ExpectFound();

    // Make sure the headers produced make sense.
    GoogleString expect_headers;
    AppendDefaultHeaders(content_type, &expect_headers);
    EXPECT_STREQ(expect_headers, rewritten_headers);

    // Also fetch the resource to ensure it can be created dynamically
    ExpectStringAsyncFetch expect_callback(true);
    lru_cache()->Clear();

    EXPECT_TRUE(rewrite_driver()->FetchResource(src_string, &expect_callback));
    rewrite_driver()->WaitForCompletion();
    EXPECT_EQ(HttpStatus::kOK,
              expect_callback.response_headers()->status_code()) <<
        "Looking for " << src_string;
    EXPECT_STREQ(rewritten_image, expect_callback.buffer());
    EXPECT_STREQ(rewritten_headers,
                 expect_callback.response_headers()->ToString());

    // Try to fetch from an independent server.
    ServeResourceFromManyContexts(src_string, rewritten_image);
  }

  // Helper class to collect image srcs.
  class ImageCollector : public EmptyHtmlFilter {
   public:
    ImageCollector(HtmlParse* html_parse, StringVector* img_srcs)
        : img_srcs_(img_srcs) {
    }

    virtual void StartElement(HtmlElement* element) {
      semantic_type::Category category;
      HtmlElement::Attribute* src = resource_tag_scanner::ScanElement(
          element, NULL /* driver */, &category);
      if (src != NULL && category == semantic_type::kImage) {
        img_srcs_->push_back(src->DecodedValueOrNull());
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

    // Fetch messed up versions. Currently image rewriter doesn't actually
    // fetch them.
    GoogleString out;
    EXPECT_TRUE(
        FetchResourceUrl(ChangeSuffix(url1, append_junk, ".jpg", junk), &out));
    EXPECT_TRUE(
        FetchResourceUrl(ChangeSuffix(url2, append_junk, ".png", junk), &out));
    // This actually has .png in the output since we convert gif -> png.
    EXPECT_TRUE(
        FetchResourceUrl(ChangeSuffix(url3, append_junk, ".png", junk), &out));

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
    EXPECT_TRUE(rewritten_gurl.is_valid());

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
};

TEST_F(ImageRewriteTest, ImgTag) {
  RewriteImage("img", kContentTypeJpeg);
}

TEST_F(ImageRewriteTest, ImgTagWebp) {
  if (RunningOnValgrind()) {
    return;
  }
  // We use the webp testing user agent; real webp-capable user agents are
  // tested as part of user_agent_matcher_test and are likely to remain in flux
  // over time.
  rewrite_driver()->set_user_agent("webp");
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
  rewrite_driver()->set_user_agent("webp");
  RewriteImage("input type=\"image\"", kContentTypeWebp);
}

TEST_F(ImageRewriteTest, DataUrlTest) {
  DataUrlResource();
}

TEST_F(ImageRewriteTest, AddDimTest) {
  // Make sure optimizable image isn't optimized, but
  // dimensions are inserted.
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  rewrite_driver()->AddFilters();
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng,
                    "", " width=\"100\" height=\"100\"", false, false);
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Force any image read to be a fetch.
  lru_cache()->Delete(StrCat(kTestDomain, kBikePngFile));

  // .. Now make sure we cached dimension insertion properly, and can do it
  // without re-fetching the image.
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypePng,
                    "", " width=\"100\" height=\"100\"", false, false);
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

TEST_F(ImageRewriteTest, PngToJpeg) {
  // Make sure we convert png to jpeg if we requested that.
  // We lower compression quality to ensure the jpeg is smaller.
  options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->set_image_jpeg_recompress_quality(85);
  rewrite_driver()->AddFilters();
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypeJpeg,
                    "", " width=\"100\" height=\"100\"", true, false);
}

TEST_F(ImageRewriteTest, PngToWebp) {
  if (RunningOnValgrind()) {
    return;
  }
  // Make sure we convert png to webp if user agent permits.
  // We lower compression quality to ensure the webp is smaller.
  options()->EnableFilter(RewriteOptions::kConvertPngToJpeg);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->set_image_jpeg_recompress_quality(85);
  rewrite_driver()->AddFilters();
  rewrite_driver()->set_user_agent("webp");
  // TODO(jmaessen): Make png->webp conversion work in image.cc and
  // webp_optimizer.cc (the latter code is jpeg-specific right now).
  TestSingleRewrite(kBikePngFile, kContentTypePng, kContentTypeJpeg,
                    "", " width=\"100\" height=\"100\"", true, false);
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

  const char kUnparsableDims[] =
      " style=\"width:256cm;height:192cm;\"";
    TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg, kContentTypeJpeg,
                      kUnparsableDims, kUnparsableDims, false, false);
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

TEST_F(ImageRewriteTest, InlineTestWithoutOptimize) {
  // Make sure we don't resize, if we don't optimize.
  options()->set_image_inline_max_bytes(10000);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  rewrite_driver()->AddFilters();
  const char kChefDims[] = " width=\"192\" height=\"256\"";
  // Without resize, it's not optimizable.
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    "", kChefDims, false, false);
}

TEST_F(ImageRewriteTest, InlineTestWithResizeWithOptimize) {
  options()->set_image_inline_max_bytes(10000);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  rewrite_driver()->AddFilters();
  const char kResizedDims[] = " width=48 height=64";
  // Without resize, it's not optimizable.
  // With resize, the image shrinks quite a bit, and we can inline it
  // given the 10K threshold explicitly set above.  This also strips the
  // size information, which is now embedded in the image itself anyway.
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypePng,
                    kResizedDims, "", true, true);
}

TEST_F(ImageRewriteTest, InlineCriticalOnly) {
  StringSet* critical_images = new StringSet;
  rewrite_driver()->set_critical_images(critical_images);
  MeaningfulCriticalImagesFinder* finder = new MeaningfulCriticalImagesFinder;
  resource_manager()->set_critical_images_finder(finder);
  options()->set_image_inline_max_bytes(30000);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  rewrite_driver()->AddFilters();
  // Image not present in critical set should not be inlined.
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    "", "", false, false);

  // Image present in critical set should be inlined.
  critical_images->insert(StrCat(kTestDomain, kChefGifFile));
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    "", "", false, true);
}

TEST_F(ImageRewriteTest, ComputeCriticalImages) {
  MeaningfulCriticalImagesFinder* finder = new MeaningfulCriticalImagesFinder;
  resource_manager()->set_critical_images_finder(finder);
  options()->set_image_inline_max_bytes(30000);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  rewrite_driver()->AddFilters();
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    "", "", false, false);
  // Empty user agent supports inlining, so we call ComputeCriticalImages.
  EXPECT_EQ(1, finder->num_compute_calls());

  // Change to a user agent that does not support inlining. We don't call
  // ComputeCriticalImages.
  rewrite_driver()->set_user_agent("Firefox/2.0");
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    "", "", false, false);
  EXPECT_EQ(1, finder->num_compute_calls());

  // Change back to a user agent that supports inlining. We call
  // ComputeCriticalImages again.
  rewrite_driver()->set_user_agent("Firefox/3.0");
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    "", "", false, false);
  EXPECT_EQ(2, finder->num_compute_calls());
}

TEST_F(ImageRewriteTest, InlineNoRewrite) {
  // Make sure we inline an image that isn't otherwise altered in any way.
  options()->set_image_inline_max_bytes(30000);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  rewrite_driver()->AddFilters();
  const char kChefDims[] = " width=192 height=256";
  // This image is just small enough to inline, which also erases
  // dimension information.
  // TODO(jmaessen): At present we're conservatively leaving it in
  // due to problems with resizing in the field.  The second set
  // of dimensions ought to be "".
  TestSingleRewrite(kChefGifFile, kContentTypeGif, kContentTypeGif,
                    kChefDims, kChefDims, false, true);
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

  GoogleUrl new_png_gurl(new_png_url);
  EXPECT_TRUE(new_png_gurl.is_valid());
  GoogleUrl encoded_png_gurl(EncodeWithBase("http://other_domain.test/",
                                            "http://other_domain.test/foo/bar/",
                                            "x", "0", "a.png", "x"));
  EXPECT_EQ(encoded_png_gurl.AllExceptLeaf(), new_png_gurl.AllExceptLeaf());

  GoogleUrl new_jpeg_gurl(new_jpeg_url);
  EXPECT_TRUE(new_jpeg_gurl.is_valid());
  GoogleUrl encoded_jpeg_gurl(EncodeWithBase("http://other_domain.test/",
                                             "http://other_domain.test/baz/",
                                             "x", "0", "b.jpeg", "x"));
  EXPECT_EQ(encoded_jpeg_gurl.AllExceptLeaf(), new_jpeg_gurl.AllExceptLeaf());

  GoogleUrl new_gif_gurl(new_gif_url);
  EXPECT_TRUE(new_gif_gurl.is_valid());
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
                          Encode("http://test.com/", "ce", "0", "a.png", "png"),
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

  GoogleString out_css_url = Encode(kTestDomain, "cf", "0", kCssFile, "css");
  GoogleString out_png_url = Encode(kTestDomain, "ic", "0", kPngFile, "png");

  // Set the current # of rewrites very high, so we stop doing more
  // due to "load".
  Variable* ongoing_rewrites =
      statistics()->GetVariable(ImageRewriteFilter::kImageOngoingRewrites);
  ongoing_rewrites->Set(100);

  ValidateExpected("img_in_css", CssLinkHref(kCssFile),
                   CssLinkHref(out_css_url));

  GoogleString out_css;
  EXPECT_TRUE(FetchResourceUrl(out_css_url, &out_css));
  // During this time, the out_css should only be absolutified, and a dropped
  // image rewrite be recorded.
  GoogleString abs_png_url = AbsolutifyUrl(kPngFile);
  GoogleString abs_css = StringPrintf(kCssTemplate, abs_png_url.c_str());
  EXPECT_EQ(abs_css, out_css);
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
  EXPECT_TRUE(FetchResourceUrl(out_css_url, &out_css));
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

TEST_F(ImageRewriteTest, InlinableImagesInsertedIntoPropertyCache) {
  // If image_inlining_identify_and_cache_without_rewriting() is set in
  // RewriteOptions, images that would have been inlined are instead inserted
  // into the property cache.
  const char kChefDims[] = " width=192 height=256";
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
  GoogleString out_css_url = Encode(kTestDomain, "cf", "0", kCssFile, "css");
  GoogleString out_css;
  StringAsyncFetch async_fetch(&out_css);
  ResponseHeaders response;
  async_fetch.set_response_headers(&response);
  EXPECT_TRUE(rewrite_driver_->FetchResource(out_css_url, &async_fetch));
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

}  // namespace

}  // namespace net_instaweb
