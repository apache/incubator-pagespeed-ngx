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

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/image_rewrite_filter.h"
#include "net/instaweb/rewriter/public/image_tag_scanner.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/dynamic_annotations.h"  // RunningOnValgrind
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

namespace {

// Filenames of resource files.
const char kBikePngFile[] = "BikeCrashIcn.png";
const char kPuzzleJpgFile[] = "Puzzle.jpg";
const char kChefGifFile[] = "IronChef2.gif";
const char kCuppaTPngFile[] = "CuppaT.png";

class ImageRewriteTest : public ResourceManagerTestBase,
                         public ::testing::WithParamInterface<bool> {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    SetAsynchronousRewrites(GetParam());
  }

  // Simple image rewrite test to check resource fetching functionality.
  void RewriteImage(const GoogleString& tag_string,
                    const ContentType& content_type) {
    options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
    options()->EnableFilter(RewriteOptions::kRecompressImages);
    options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
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

    GoogleString rewritten_filename;
    EncodeFilename(src_string, &rewritten_filename);

    GoogleString rewritten_image_data;
    ASSERT_TRUE(ReadFile(rewritten_filename.c_str(), &rewritten_image_data));

    // Also fetch the resource to ensure it can be created dynamically
    RequestHeaders request_headers;
    ResponseHeaders response_headers;
    GoogleString fetched_resource_content;
    StringWriter writer(&fetched_resource_content);
    ExpectCallback dummy_callback(true);

    GoogleString headers;
    AppendDefaultHeaders(content_type, &headers);

    writer.Write(headers, &message_handler_);
    writer.Flush(&message_handler_);
    int header_size = fetched_resource_content.length();
    EXPECT_TRUE(
        rewrite_driver()->FetchResource(src_string, request_headers,
                                        &response_headers, &writer,
                                        &dummy_callback));
    rewrite_driver()->WaitForCompletion();
    EXPECT_EQ(HttpStatus::kOK, response_headers.status_code()) <<
        "Looking for " << src_string;
    // For readability, only do EXPECT_EQ on initial portions of data
    // as most of it isn't human-readable.  This will show us the headers
    // and the start of the image data.  So far every failure fails this
    // first, and we caught doubled headers this way.
    EXPECT_EQ(rewritten_image_data.substr(0, 250),
              fetched_resource_content.substr(0, 250)) <<
        "In " << src_string <<
        " response headers " << response_headers.ToString();
    EXPECT_TRUE(rewritten_image_data == fetched_resource_content) <<
        "In " << src_string;

    // Try to fetch from an independent server.
    ServeResourceFromManyContexts(src_string,
                                  rewritten_image_data.substr(header_size));
  }

  // Helper class to collect image srcs.
  class ImageCollector : public EmptyHtmlFilter {
   public:
    ImageCollector(HtmlParse* html_parse, StringVector* img_srcs)
        : img_srcs_(img_srcs),
          image_filter_(html_parse) {
    }

    virtual void StartElement(HtmlElement* element) {
      HtmlElement::Attribute* src = image_filter_.ParseImageElement(element);
      if (src != NULL) {
        img_srcs_->push_back(src->value());
      }
    }

    virtual const char* Name() const { return "ImageCollector"; }

   private:
    StringVector* img_srcs_;
    ImageTagScanner image_filter_;

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
    EXPECT_TRUE(rewrite_driver()->ReadIfCached(cuppa_resource));
    GoogleString cuppa_contents;
    cuppa_resource->contents().CopyToString(&cuppa_contents);
    // Now make sure axing the original cuppa_string doesn't affect the
    // internals of the cuppa_resource.
    ResourcePtr other_resource(
        rewrite_driver()->CreateInputResourceAbsoluteUnchecked(cuppa_string));
    ASSERT_TRUE(other_resource.get() != NULL);
    cuppa_string.clear();
    EXPECT_TRUE(rewrite_driver()->ReadIfCached(other_resource));
    GoogleString other_contents;
    cuppa_resource->contents().CopyToString(&other_contents);
    ASSERT_EQ(cuppa_contents, other_contents);
  }

  // Helper to test for how we handle trailing junk in URLs
  void TestCorruptUrl(const char* junk, bool should_fetch_ok) {
    const char kHtml[] =
        "<img src=\"a.jpg\"><img src=\"b.png\"><img src=\"c.gif\">";
    AddFileToMockFetcher(StrCat(kTestDomain, "a.jpg"), kPuzzleJpgFile,
                         kContentTypeJpeg, 100);

    AddFileToMockFetcher(StrCat(kTestDomain, "b.png"), kBikePngFile,
                         kContentTypePng, 100);

    AddFileToMockFetcher(StrCat(kTestDomain, "c.gif"), kChefGifFile,
                         kContentTypeGif, 100);

    AddFilter(RewriteOptions::kRecompressImages);

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
    EXPECT_EQ(should_fetch_ok, ServeResourceUrl(StrCat(url1, junk), &out));
    EXPECT_EQ(should_fetch_ok, ServeResourceUrl(StrCat(url2, junk), &out));
    EXPECT_EQ(should_fetch_ok, ServeResourceUrl(StrCat(url3, junk), &out));

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
                         const ContentType& content_type,
                         const char* initial_dims, const char* final_dims,
                         bool expect_rewritten, bool expect_inline) {
    GoogleString initial_url = StrCat(kTestDomain, name);
    GoogleString page_url = StrCat(kTestDomain, "test.html");
    AddFileToMockFetcher(initial_url, name, content_type, 100);

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
    } else if (expect_rewritten) {
      EXPECT_NE(initial_url, rewritten_url);
    } else {
      EXPECT_EQ(initial_url, rewritten_url);
    }

    GoogleString html_expected_output =
        StringPrintf(html_boilerplate, rewritten_url.c_str(), final_dims);
    EXPECT_EQ(AddHtmlBody(html_expected_output), output_buffer_);
  }
};

TEST_P(ImageRewriteTest, ImgTag) {
  RewriteImage("img", kContentTypeJpeg);
}

TEST_P(ImageRewriteTest, ImgTagWebp) {
  if (RunningOnValgrind()) {
    return;
  }
  // We use the webp testing user agent; real webp-capable user agents are
  // tested as part of user_agent_matcher_test and are likely to remain in flux
  // over time.
  rewrite_driver()->set_user_agent("webp");
  RewriteImage("img", kContentTypeWebp);
}

TEST_P(ImageRewriteTest, InputTag) {
  RewriteImage("input type=\"image\"", kContentTypeJpeg);
}

TEST_P(ImageRewriteTest, InputTagWebp) {
  if (RunningOnValgrind()) {
    return;
  }
  // We use the webp testing user agent; real webp-capable user agents are
  // tested as part of user_agent_matcher_test and are likely to remain in flux
  // over time.
  rewrite_driver()->set_user_agent("webp");
  RewriteImage("input type=\"image\"", kContentTypeWebp);
}

TEST_P(ImageRewriteTest, DataUrlTest) {
  DataUrlResource();
}

TEST_P(ImageRewriteTest, AddDimTest) {
  // Make sure optimizable image isn't optimized, but
  // dimensions are inserted.
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  rewrite_driver()->AddFilters();
  TestSingleRewrite(kBikePngFile, kContentTypePng,
                    "", " width=\"100\" height=\"100\"", false, false);
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Force any image read to be a fetch.
  lru_cache()->Delete(StrCat(kTestDomain, kBikePngFile));

  // .. Now make sure we cached dimension insertion properly, and can do it
  // without re-fetching the image.
  TestSingleRewrite(kBikePngFile, kContentTypePng,
                    "", " width=\"100\" height=\"100\"", false, false);
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

TEST_P(ImageRewriteTest, ResizeTest) {
  // Make sure we resize images, but don't optimize them in place.
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  const char kResizedDims[] = " width=\"256\" height=\"192\"";
  // Without explicit resizing, we leave the image alone.
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg,
                    "", "", false, false);
  // With resizing, we optimize.
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg,
                    kResizedDims, kResizedDims, true, false);
}

TEST_P(ImageRewriteTest, ResizeStyleTest) {
  // Make sure we resize images, but don't optimize them in place.
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  const char kResizedDims[] = " style=\"width:256px;height:192px;\"";
  // Without explicit resizing, we leave the image alone.
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg,
                    "", "", false, false);
  // With resizing, we optimize.
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg,
                    kResizedDims, kResizedDims, true, false);

  const char kMixedDims[] = " width=\"256\" style=\"height:192px;\"";
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg,
                    kMixedDims, kMixedDims, true, false);

  const char kMoreMixedDims[] =
      " height=\"197\" style=\"width:256px;broken:true;\"";
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg,
                    kMoreMixedDims, kMoreMixedDims, true, false);

  const char kUnparsableDims[] =
      " style=\"width:256cm;height:192cm;\"";
    TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg,
                      kUnparsableDims, kUnparsableDims, false, false);
}

TEST_P(ImageRewriteTest, InlineTest) {
  // Make sure we resize and inline images, but don't optimize them in place.
  options()->set_image_inline_max_bytes(10000);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  rewrite_driver()->AddFilters();
  const char kChefDims[] = " width=\"192\" height=\"256\"";
  const char kResizedDims[] = " width=48 height=64";
  // Without resize, it's not optimizable.
  TestSingleRewrite(kChefGifFile, kContentTypeGif,
                    "", kChefDims, false, false);
  // With resize, the image shrinks quite a bit, and we can inline it
  // given the 10K threshold explicitly set above.  This also strips the
  // size information, which is now embedded in the image itself anyway.
  TestSingleRewrite(kChefGifFile, kContentTypeGif,
                    kResizedDims, "", true, true);
}

TEST_P(ImageRewriteTest, InlineNoRewrite) {
  // Make sure we inline an image that isn't otherwise altered in any way.
  options()->set_image_inline_max_bytes(30000);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  rewrite_driver()->AddFilters();
  const char kChefDims[] = " width=192 height=256";
  // This image is just small enough to inline, which also erases
  // dimension information.
  TestSingleRewrite(kChefGifFile, kContentTypeGif,
                    kChefDims, "", false, true);
  // This image is too big to inline, and we don't insert missing
  // dimension information because that is not explicitly enabled.
  TestSingleRewrite(kPuzzleJpgFile, kContentTypeJpeg,
                    "", "", false, false);
}

TEST_P(ImageRewriteTest, InlineNoResize) {
  // Make sure we inline an image if it meets the inlining threshold but can't
  // be resized.  Make sure we retain sizing information when this happens.
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  const char kOrigDims[] = " width=65 height=70";
  const char kResizedDims[] = " width=26 height=28";
  // At natural size, we should inline and erase dimensions.
  TestSingleRewrite(kCuppaTPngFile, kContentTypePng,
                    kOrigDims, "", false, true);
  // Image is inlined but not resized, so preserve dimensions.
  TestSingleRewrite(kCuppaTPngFile, kContentTypePng,
                    kResizedDims, kResizedDims, false, true);
}

TEST_P(ImageRewriteTest, RespectsBaseUrl) {
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
  AddFilter(RewriteOptions::kRecompressImages);
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

TEST_P(ImageRewriteTest, FetchInvalid) {
  // Make sure that fetching invalid URLs cleanly reports a problem by
  // calling Done(false).
  AddFilter(RewriteOptions::kRecompressImages);
  GoogleString out;

  // We are trying to test with an invalid encoding. By construction,
  // Encode cannot make an invalid encoding.  However we can make one
  // using a PlaceHolder string and then mutating it.
  const char kPlaceholder[] = "PlaceHolder";
  GoogleString encoded_url = Encode("http://www.example.com/", "ic",
                                    "ABCDEFGHIJ", kPlaceholder, "jpg");
  GlobalReplaceSubstring(kPlaceholder, "70x53x,", &encoded_url);
  EXPECT_FALSE(ServeResourceUrl(encoded_url, &out));
}

TEST_P(ImageRewriteTest, Rewrite404) {
  // Make sure we don't fail when rewriting with invalid input.
  SetFetchResponse404("404.jpg");
  AddFilter(RewriteOptions::kRecompressImages);
  ValidateNoChanges("404", "<img src='404.jpg'>");

  // Try again to exercise cached case.
  ValidateNoChanges("404", "<img src='404.jpg'>");
}

TEST_P(ImageRewriteTest, NoExtensionCorruption) {
  TestCorruptUrl("%22", false);
}

TEST_P(ImageRewriteTest, NoQueryCorruption) {
  TestCorruptUrl("?query", true);
}

TEST_P(ImageRewriteTest, NoCrashOnInvalidDim) {
  options()->EnableFilter(RewriteOptions::kRecompressImages);
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

TEST_P(ImageRewriteTest, RewriteCacheExtendInteraction) {
  // There was a bug in async mode where rewriting failing would prevent
  // cache extension from working as well.
  options()->EnableFilter(RewriteOptions::kRecompressImages);
  options()->EnableFilter(RewriteOptions::kExtendCache);
  rewrite_driver()->AddFilters();

  // Provide a non-image file, so image rewrite fails (but cache extension
  // works)
  InitResponseHeaders("a.png", kContentTypePng, "Not a PNG", 600);

  ValidateExpected("cache_extend_fallback", "<img src=a.png>",
                   StrCat("<img src=",
                          Encode("http://test.com/", "ce", "0", "a.png", "png"),
                          ">"));
}

// http://code.google.com/p/modpagespeed/issues/detail?id=324
TEST_P(ImageRewriteTest, RetainExtraHeaders) {
  options()->EnableFilter(RewriteOptions::kRecompressImages);
  rewrite_driver()->AddFilters();

  // Store image contents into fetcher.
  AddFileToMockFetcher(StrCat(kTestDomain, kPuzzleJpgFile), kPuzzleJpgFile,
                       kContentTypeJpeg, 100);
  TestRetainExtraHeaders(kPuzzleJpgFile, "ic", "jpg");
}

TEST_P(ImageRewriteTest, NestedConcurrentRewritesLimit) {
  // Buggy in sync.
  if (!rewrite_driver()->asynchronous_rewrites()) {
    return;
  }

  // Make sure we're limiting # of concurrent rewrites properly even when we're
  // nested inside another filter, and that we do not cache that outcome
  // improperly.
  options()->EnableFilter(RewriteOptions::kRecompressImages);
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
  InitResponseHeaders(kCssFile,  kContentTypeCss, in_css, 100);

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
  EXPECT_TRUE(ServeResourceUrl(out_css_url, &out_css));
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
  EXPECT_TRUE(ServeResourceUrl(out_css_url, &out_css));
  // This time, however, CSS should be altered (and the drop count still be 1).
  EXPECT_EQ(expected_out_css, out_css);
  EXPECT_EQ(1, drops->Get(TimedVariable::START));
}

// We test with asynchronous_rewrites() == GetParam() as both true and false.
INSTANTIATE_TEST_CASE_P(ImageRewriteTestInstance,
                        ImageRewriteTest,
                        ::testing::Bool());

}  // namespace

}  // namespace net_instaweb
