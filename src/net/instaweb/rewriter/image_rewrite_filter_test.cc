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
#include "net/instaweb/util/public/stdio_file_system.h"

namespace net_instaweb {

namespace {

class ImageRewriteTest : public ResourceManagerTestBase {
 protected:
  // Simple image rewrite test to check resource fetching functionality.
  void RewriteImage() {
    RewriteOptions options;
    options.EnableFilter(RewriteOptions::kRewriteImages);
    options.EnableFilter(RewriteOptions::kInsertImgDimensions);
    options.set_img_inline_max_bytes(2000);
    rewrite_driver_.AddFilters(options);

    other_rewrite_driver_.AddFilter(RewriteOptions::kRewriteImages);

    // URLs and content for HTML document and resources.
    const char domain[] = "http://rewrite_image.test/";
    const char html_url[] = "http://rewrite_image.test/RewriteImage.html";
    const char image_url[] = "http://rewrite_image.test/Puzzle.jpg";

    const char image_html[] = "<head/><body><img src=\"Puzzle.jpg\"/></body>";

    // Store image contents in a string.
    // TODO(sligocki): There's probably a lot of wasteful copying here.
    std::string image_filename =
        StrCat(GTestSrcDir(), kTestData, "Puzzle.jpg");
    std::string image_body;
    // We need to load a file from the testdata directory. Don't use this
    // physical filesystem for anything else, use file_system_ which can be
    // abstracted as a MemFileSystem instead.
    StdioFileSystem stdio_file_system;
    ASSERT_TRUE(stdio_file_system.ReadFile(image_filename.c_str(), &image_body,
                                           &message_handler_));

    // Put original image into our fetcher.
    SimpleMetaData default_jpeg_header;
    resource_manager_->SetDefaultHeaders(&kContentTypeJpeg,
                                         &default_jpeg_header);
    mock_url_fetcher_.SetResponse(image_url, default_jpeg_header, image_body);

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

    std::string expected_output =
        StrCat("<head/><body><img src=\"", src_string,
               "\" width=1023 height=766 /></body>");
    EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);

    std::string rewritten_filename;
    filename_encoder_.Encode(file_prefix_, src_string, &rewritten_filename);

    std::string rewritten_image_data;
    ASSERT_TRUE(file_system_.ReadFile(rewritten_filename.c_str(),
                                      &rewritten_image_data,
                                      &message_handler_));

    // Also fetch the resource to ensure it can be created dynamically
    SimpleMetaData request_headers, response_headers;
    std::string fetched_resource_content;
    StringWriter writer(&fetched_resource_content);
    DummyCallback dummy_callback(true);

    std::string headers;
    AppendDefaultHeaders(kContentTypeJpeg, resource_manager_, &headers);

    writer.Write(headers, &message_handler_);
    rewrite_driver_.FetchResource(src_string, request_headers,
                                  &response_headers, &writer,
                                  &message_handler_, &dummy_callback);
    EXPECT_EQ(HttpStatus::kOK, response_headers.status_code()) <<
        "Looking for " << src_string;
    // For readability, only do EXPECT_EQ on initial portions of data
    // as most of it isn't human-readable.  This will show us the headers
    // and the start of the image data.  So far every failure fails this
    // first, and we caught doubled headers this way.
    EXPECT_EQ(rewritten_image_data.substr(0, 100),
              fetched_resource_content.substr(0, 100)) <<
        "In " << src_string;
    EXPECT_TRUE(rewritten_image_data == fetched_resource_content) <<
        "In " << src_string;

    // Now we fetch from the "other" server. To simulate first fetch, we
    // need to:
    // 1) Clear the cache, so we don't just find the result there.
    // TODO(sligocki): We should just use a separate cache.
    lru_cache_->Clear();
    // 2) Disable the file system, so we don't find the resource there.
    // TODO(sligocki): We should just use a separate mem_file_system.
    file_system_.Disable();
    // 3) Disable the fetcher, so that the fetch doesn't finish imidiately.
    // TODO(sligocki): We could use the CallCallbacks() trick to get a more
    // correct response, rather than effectively shutting off our network.
    mock_url_fetcher_.Disable();

    fetched_resource_content.clear();
    dummy_callback.Reset();
    SimpleMetaData redirect_headers;
    other_rewrite_driver_.FetchResource(src_string, request_headers,
                                        &redirect_headers, &writer,
                                        &message_handler_, &dummy_callback);
    std::string expected_redirect =
        StrCat("<img src=\"", image_url, "\" alt=\"Temporarily Moved\"/>");

    EXPECT_EQ(HttpStatus::kTemporaryRedirect, redirect_headers.status_code());
    EXPECT_EQ(expected_redirect, fetched_resource_content);

    // Now we switch the file system and fetcher back on, corresponding to
    // the case where a prior async fetch completed and we're ready to rewrite.
    // This should yield the same results as the original html- driven rewrite.
    file_system_.Enable();
    mock_url_fetcher_.Enable();

    // TODO(jmarantz): Refactor this code which tries the serving-fetch twice,
    // the second after the cache stops 'remembering' that the origin fetch
    // failed.
    message_handler_.Message(
        kInfo, "Now with serving, but with a recent fetch failure.");
    SimpleMetaData other_headers;
    //size_t header_size = fetched_resource_content.size();
    dummy_callback.Reset();

    // Trying this twice.  The first time, we will again fail to rewrite
    // the resource because the cache will 'remembered' that the image origin
    // fetch failed and so the resource manager will not try again for 5
    // minutes.
    fetched_resource_content.clear();
    other_rewrite_driver_.FetchResource(src_string, request_headers,
                                        &other_headers, &writer,
                                        &message_handler_, &dummy_callback);
    EXPECT_EQ(HttpStatus::kTemporaryRedirect, redirect_headers.status_code());
    EXPECT_EQ(expected_redirect, fetched_resource_content);

    // Now let some time go by and try again.
    message_handler_.Message(
        kInfo, "Now with serving, but 5 minutes expired since fetch failure.");
    mock_timer_.advance_ms(5 * Timer::kMinuteMs + 1 * Timer::kSecondMs);
    fetched_resource_content.clear();
    writer.Write(headers, &message_handler_);
    EXPECT_EQ(headers, fetched_resource_content);
    dummy_callback.Reset();
    other_rewrite_driver_.FetchResource(src_string, request_headers,
                                        &other_headers, &writer,
                                        &message_handler_, &dummy_callback);
    EXPECT_EQ(HttpStatus::kOK, other_headers.status_code());
    EXPECT_EQ(rewritten_image_data.substr(0, 100),
              fetched_resource_content.substr(0, 100));
    EXPECT_TRUE(rewritten_image_data == fetched_resource_content);

    std::string secondary_image_data;
    ASSERT_TRUE(file_system_.ReadFile(rewritten_filename.c_str(),
                                      &secondary_image_data,
                                      &message_handler_));
    EXPECT_EQ(rewritten_image_data.substr(0, 100),
              secondary_image_data.substr(0, 100));
    EXPECT_EQ(rewritten_image_data, secondary_image_data);

    // Try to fetch from an independent server.
    /* TODO(sligocki): Get this working. Right now it returns a redirect.
    ServeResourceFromManyContexts(src_string, RewriteOptions::kRewriteImages,
                                  &mock_hasher_,
                                  fetched_resource_content.substr(header_size));
    */
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
        resource_manager_->CreateInputResourceAbsolute(cuppa_string,
                                                       &message_handler_));
    ASSERT_TRUE(cuppa_resource != NULL);
    EXPECT_TRUE(resource_manager_->ReadIfCached(cuppa_resource.get(),
                                                &message_handler_));
    std::string cuppa_contents;
    cuppa_resource->contents().CopyToString(&cuppa_contents);
    // Now make sure axing the original cuppa_string doesn't affect the
    // internals of the cuppa_resource.
    scoped_ptr<Resource> other_resource(
        resource_manager_->CreateInputResourceAbsolute(cuppa_string,
                                                       &message_handler_));
    ASSERT_TRUE(other_resource != NULL);
    cuppa_string.clear();
    EXPECT_TRUE(resource_manager_->ReadIfCached(other_resource.get(),
                                                &message_handler_));
    std::string other_contents;
    cuppa_resource->contents().CopyToString(&other_contents);
    ASSERT_EQ(cuppa_contents, other_contents);
  }
};

TEST_F(ImageRewriteTest, ImageRewriteTest) {
  RewriteImage();
}

TEST_F(ImageRewriteTest, DataUrlTest) {
  DataUrlResource();
}

}  // namespace

}  // namespace net_instaweb
