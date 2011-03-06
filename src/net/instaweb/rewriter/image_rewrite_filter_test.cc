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

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/rewriter/public/img_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/mock_timer.h"

namespace net_instaweb {

namespace {

// Filenames of resource files.
const char kBikePngFile[] = "BikeCrashIcn.png";
const char kPuzzleJpgFile[] = "Puzzle.jpg";

class ImageRewriteTest : public ResourceManagerTestBase {
 protected:
  // Simple image rewrite test to check resource fetching functionality.
  void RewriteImage(const std::string& tag_string) {
    options_.EnableFilter(RewriteOptions::kRewriteImages);
    options_.EnableFilter(RewriteOptions::kInsertImgDimensions);
    options_.set_img_inline_max_bytes(2000);
    rewrite_driver_.AddFilters();

    AddOtherFilter(RewriteOptions::kRewriteImages);

    // URLs and content for HTML document and resources.
    const char domain[] = "http://rewrite_image.test/";
    const char html_url[] = "http://rewrite_image.test/RewriteImage.html";
    const char image_url[] = "http://rewrite_image.test/Puzzle.jpg";

    const std::string image_html =
        StrCat("<head/><body><", tag_string, " src=\"Puzzle.jpg\"/></body>");

    // Store image contents into fetcher.
    const std::string image_filename =
        StrCat(GTestSrcDir(), kTestData, "Puzzle.jpg");
    AddFileToMockFetcher(image_url, image_filename, kContentTypeJpeg);

    // Rewrite the HTML page.
    ParseUrl(html_url, image_html);
    StringVector img_srcs;
    CollectImgSrcs("RewriteImage/collect_sources", output_buffer_, &img_srcs);
    // output_buffer_ should have exactly one image file (Puzzle.jpg).
    EXPECT_EQ(1UL, img_srcs.size());
    const std::string& src_string = img_srcs[0];
    EXPECT_EQ(domain, src_string.substr(0, strlen(domain)));
    EXPECT_EQ(".jpg", src_string.substr(src_string.size() - 4, 4));

    std::string rewritten_data;

    const std::string expected_output =
        StrCat("<head/><body><", tag_string, " src=\"", src_string,
               "\" width=\"1023\" height=\"766\"/></body>");
    EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);

    std::string rewritten_filename;
    filename_encoder_.Encode(file_prefix_, src_string, &rewritten_filename);

    std::string rewritten_image_data;
    ASSERT_TRUE(file_system_.ReadFile(rewritten_filename.c_str(),
                                      &rewritten_image_data,
                                      &message_handler_));

    // Also fetch the resource to ensure it can be created dynamically
    RequestHeaders request_headers;
    ResponseHeaders response_headers;
    std::string fetched_resource_content;
    StringWriter writer(&fetched_resource_content);
    DummyCallback dummy_callback(true);

    std::string headers;
    AppendDefaultHeaders(kContentTypeJpeg, resource_manager_, &headers);

    writer.Write(headers, &message_handler_);
    writer.Flush(&message_handler_);
    int header_size = fetched_resource_content.length();
    EXPECT_TRUE(
        rewrite_driver_.FetchResource(src_string, request_headers,
                                      &response_headers, &writer,
                                      &dummy_callback));
    EXPECT_EQ(HttpStatus::kOK, response_headers.status_code()) <<
        "Looking for " << src_string;
    // For readability, only do EXPECT_EQ on initial portions of data
    // as most of it isn't human-readable.  This will show us the headers
    // and the start of the image data.  So far every failure fails this
    // first, and we caught doubled headers this way.
    EXPECT_EQ(rewritten_image_data.substr(0, 250),
              fetched_resource_content.substr(0, 250)) <<
        "In " << src_string;
    EXPECT_TRUE(rewritten_image_data == fetched_resource_content) <<
        "In " << src_string;

    // Try to fetch from an independent server.
    ServeResourceFromManyContexts(src_string, RewriteOptions::kRewriteImages,
                                  &mock_hasher_,
                                  rewritten_image_data.substr(header_size));
  }

  // Helper class to collect img srcs.
  class ImgCollector : public EmptyHtmlFilter {
   public:
    ImgCollector(HtmlParse* html_parse, StringVector* img_srcs)
        : img_srcs_(img_srcs),
          img_filter_(html_parse) {
    }

    virtual void StartElement(HtmlElement* element) {
      HtmlElement::Attribute* src = img_filter_.ParseImgElement(element);
      if (src != NULL) {
        img_srcs_->push_back(src->value());
      }
    }

    virtual const char* Name() const { return "ImgCollector"; }

   private:
    StringVector* img_srcs_;
    ImgTagScanner img_filter_;

    DISALLOW_COPY_AND_ASSIGN(ImgCollector);
  };

  // Fills `img_srcs` with the urls in img src attributes in `html`
  void CollectImgSrcs(const StringPiece& id, const StringPiece& html,
                       StringVector* img_srcs) {
    HtmlParse html_parse(&message_handler_);
    ImgCollector collector(&html_parse, img_srcs);
    html_parse.AddFilter(&collector);
    std::string dummy_url = StrCat("http://collect.css.links/", id, ".html");
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
    std::string cuppa_string(kCuppaData);
    scoped_ptr<Resource> cuppa_resource(
        rewrite_driver_.CreateInputResourceAbsoluteUnchecked(cuppa_string));
    ASSERT_TRUE(cuppa_resource != NULL);
    EXPECT_TRUE(rewrite_driver_.ReadIfCached(cuppa_resource.get()));
    std::string cuppa_contents;
    cuppa_resource->contents().CopyToString(&cuppa_contents);
    // Now make sure axing the original cuppa_string doesn't affect the
    // internals of the cuppa_resource.
    scoped_ptr<Resource> other_resource(
        rewrite_driver_.CreateInputResourceAbsoluteUnchecked(cuppa_string));
    ASSERT_TRUE(other_resource != NULL);
    cuppa_string.clear();
    EXPECT_TRUE(rewrite_driver_.ReadIfCached(other_resource.get()));
    std::string other_contents;
    cuppa_resource->contents().CopyToString(&other_contents);
    ASSERT_EQ(cuppa_contents, other_contents);
  }

  // Helper to test for how we handle trailing junk in URLs
  void TestCorruptUrl(const char* junk, bool should_fetch_ok) {
    const char kHtml[] = "<img src=\"a.jpg\"><img src=\"b.png\">";
    AddFileToMockFetcher(StrCat(kTestDomain, "a.jpg"),
                        StrCat(GTestSrcDir(), kTestData, kPuzzleJpgFile),
                        kContentTypeJpeg);

    AddFileToMockFetcher(StrCat(kTestDomain, "b.png"),
                        StrCat(GTestSrcDir(), kTestData, kBikePngFile),
                        kContentTypeJpeg);

    AddFilter(RewriteOptions::kRewriteImages);

    StringVector img_srcs;
    ImgCollector img_collect(&rewrite_driver_, &img_srcs);
    rewrite_driver_.AddFilter(&img_collect);

    ParseUrl(kTestDomain, kHtml);
    ASSERT_EQ(2, img_srcs.size());
    std::string normal_output = output_buffer_;
    std::string url1 = img_srcs[0];
    std::string url2 = img_srcs[1];

    // Fetch messed up versions. Currently image rewriter doesn't actually
    // fetch them.
    std::string out;
    EXPECT_EQ(should_fetch_ok, ServeResourceUrl(StrCat(url1, junk), &out));
    EXPECT_EQ(should_fetch_ok, ServeResourceUrl(StrCat(url2, junk), &out));

    // Now run through again to make sure we didn't cache the messed up URL
    img_srcs.clear();
    ParseUrl(kTestDomain, kHtml);
    EXPECT_EQ(normal_output, output_buffer_);
    ASSERT_EQ(2, img_srcs.size());
    EXPECT_EQ(url1, img_srcs[0]);
    EXPECT_EQ(url2, img_srcs[1]);
  }
};

TEST_F(ImageRewriteTest, ImgTag) {
  RewriteImage("img");
}

TEST_F(ImageRewriteTest, InputTag) {
  RewriteImage("input type=\"image\"");
}

TEST_F(ImageRewriteTest, DataUrlTest) {
  DataUrlResource();
}

TEST_F(ImageRewriteTest, RespectsBaseUrl) {
  // Put original files into our fetcher.
  const char html_url[] = "http://image.test/base_url.html";
  const char png_url[]  = "http://other_domain.test/foo/bar/a.png";
  const char jpeg_url[] = "http://other_domain.test/baz/b.jpeg";

  AddFileToMockFetcher(png_url,
                       StrCat(GTestSrcDir(), kTestData, kBikePngFile),
                       kContentTypePng);
  AddFileToMockFetcher(jpeg_url,
                       StrCat(GTestSrcDir(), kTestData, kPuzzleJpgFile),
                       kContentTypeJpeg);

  // Second stylesheet is on other domain.
  const char html_input[] =
      "<head>\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "</head>\n"
      "<body>\n"
      "  <img src='bar/a.png'>\n"
      "  <img src='/baz/b.jpeg'>\n"
      "</body>";

  // Rewrite
  AddFilter(RewriteOptions::kRewriteImages);
  ParseUrl(html_url, html_input);

  // Check for CSS files in the rewritten page.
  StringVector image_urls;
  CollectImgSrcs("base_url-links", output_buffer_, &image_urls);
  EXPECT_EQ(2UL, image_urls.size());
  const std::string& new_png_url = image_urls[0];
  const std::string& new_jpeg_url = image_urls[1];

  // Sanity check that we changed the URL.
  EXPECT_NE("a.png", new_png_url);
  EXPECT_NE("b.jpeg", new_jpeg_url);

  LOG(INFO) << "new_png_url: " << new_png_url;
  LOG(INFO) << "new_jpeg_url: " << new_jpeg_url;

  const char expected_output_format[] =
      "<head>\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "</head>\n"
      "<body>\n"
      "  <img src='%s'>\n"
      "  <img src='%s'>\n"
      "</body>";
  std::string expected_output = StringPrintf(expected_output_format,
                                              new_png_url.c_str(),
                                              new_jpeg_url.c_str());

  EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);

  GoogleUrl new_png_gurl(new_png_url);
  EXPECT_TRUE(new_png_gurl.is_valid());
  EXPECT_EQ("other_domain.test", new_png_gurl.Host());

  GoogleUrl new_jpeg_gurl(new_jpeg_url);
  EXPECT_TRUE(new_jpeg_gurl.is_valid());
  EXPECT_EQ("other_domain.test", new_jpeg_gurl.Host());
}

TEST_F(ImageRewriteTest, FetchInvalid) {
  // Make sure that fetching invalid URLs cleanly reports a problem by
  // calling Done(false).
  AddFilter(RewriteOptions::kRewriteImages);
  std::string out;
  EXPECT_FALSE(
      ServeResourceUrl(
          "http://www.example.com/70x53x,.pagespeed.ic.ABCDEFGHIJ.jpg", &out));
}

TEST_F(ImageRewriteTest, NoExtensionCorruption) {
  TestCorruptUrl("%22", false);
}

TEST_F(ImageRewriteTest, NoQueryCorruption) {
  TestCorruptUrl("?query", true);
}

TEST_F(ImageRewriteTest, NoCrashOnInvalidDim) {
  options_.EnableFilter(RewriteOptions::kRewriteImages);
  options_.EnableFilter(RewriteOptions::kInsertImgDimensions);
  rewrite_driver_.AddFilters();
  AddFileToMockFetcher(StrCat(kTestDomain, "a.png"),
                       StrCat(GTestSrcDir(), kTestData, kBikePngFile),
                       kContentTypePng);

  ParseUrl(kTestDomain, "<img width=0 height=0 src=\"a.png\">");
  ParseUrl(kTestDomain, "<img width=0 height=42 src=\"a.png\">");
  ParseUrl(kTestDomain, "<img width=42 height=0 src=\"a.png\">");
  ParseUrl(kTestDomain, "<img width=\"-5\" height=\"5\" src=\"a.png\">");
  ParseUrl(kTestDomain, "<img width=\"-5\" height=\"0\" src=\"a.png\">");
  ParseUrl(kTestDomain, "<img width=\"-5\" height=\"-5\" src=\"a.png\">");
  ParseUrl(kTestDomain, "<img width=\"5\" height=\"-5\" src=\"a.png\">");
}

}  // namespace

}  // namespace net_instaweb
