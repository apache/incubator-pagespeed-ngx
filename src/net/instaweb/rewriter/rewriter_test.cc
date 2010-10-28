/**
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

// Author: jmarantz@google.com (Joshua Marantz)
//     and sligocki@google.com (Shawn Ligocki)

// Unit-test the html rewriter

#include "base/basictypes.h"
#include "net/instaweb/rewriter/public/css_move_to_head_filter.h"
#include "net/instaweb/rewriter/public/css_outline_filter.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/js_outline_filter.h"
#include "net/instaweb/rewriter/public/img_tag_scanner.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/mock_hasher.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/simple_stats.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

namespace net_instaweb {

const std::string kTestData = "/net/instaweb/rewriter/testdata/";

class RewriterTest : public ResourceManagerTestBase {
 protected:
  RewriterTest() : other_driver_(&message_handler_, &file_system_,
                                 &mock_url_async_fetcher_) {
  }

  void DeleteFileIfExists(const std::string& filename) {
    if (file_system_.Exists(filename.c_str(), &message_handler_).is_true()) {
      ASSERT_TRUE(file_system_.RemoveFile(filename.c_str(), &message_handler_));
    }
  }


  void AppendDefaultHeaders(const ContentType& content_type,
                            ResourceManager* resource_manager,
                            std::string* text) {
    SimpleMetaData header;
    resource_manager->SetDefaultHeaders(&content_type, &header);
    StringWriter writer(text);
    header.Write(&writer, &message_handler_);
  }

  // The async fetchers in these tests are really fake async fetchers, and
  // will call their callbacks directly.  Hence we don't really need
  // any functionality in the async callback.
  class DummyCallback : public UrlAsyncFetcher::Callback {
   public:
    DummyCallback() : done_(false) {}
    virtual ~DummyCallback() {
      EXPECT_TRUE(done_);
    }
    virtual void Done(bool success) {
      done_ = true;
      EXPECT_TRUE(success);
    }
    bool done_;

   private:
    DISALLOW_COPY_AND_ASSIGN(DummyCallback);
  };

  void ServeResourceFromNewContext(const std::string& resource,
                                   const std::string& output_filename,
                                   const StringPiece& expected_content,
                                   RewriteOptions::Filter filter,
                                   const char* leaf) {
    scoped_ptr<ResourceManager> resource_manager(
        NewResourceManager(&mock_hasher_));
    SimpleStats stats;
    RewriteDriver::Initialize(&stats);
    resource_manager->set_statistics(&stats);
    RewriteDriver driver(&message_handler_, &file_system_,
                         &mock_url_async_fetcher_);
    driver.SetResourceManager(resource_manager.get());
    driver.AddFilter(filter);
    Variable* resource_fetches =
        stats.GetVariable(RewriteDriver::kResourceFetches);
    SimpleMetaData request_headers, response_headers;
    std::string contents;
    StringWriter writer(&contents);
    DummyCallback callback;

    // Delete the output resource from the cache and the file system.
    EXPECT_EQ(CacheInterface::kAvailable, http_cache_.Query(resource)) << resource;
    lru_cache_->Clear();

    // Now delete it from the file system, so it must be recomputed.
    EXPECT_TRUE(file_system_.RemoveFile(output_filename.c_str(),
                                        &message_handler_));

    EXPECT_TRUE(driver.FetchResource(
        resource, request_headers, &response_headers, &writer,
        &message_handler_, &callback)) << resource;
    EXPECT_EQ(expected_content, contents);
    EXPECT_EQ(1, resource_fetches->Get());
  }

  // Test spriting CSS with options to write headers and use a hasher.
  void CombineCss(const StringPiece& id, Hasher* hasher,
                  const char* barrier_text, bool is_barrier) {
    // Here other_driver_ is used to do resource-only fetches for testing.
    //
    // The idea is that rewrite_driver_ (and its resource_manager) are running
    // on server A, while other_driver_ (and its other_resource_manager) are
    // running on server B. Server A gets a request that calls for combining
    // CSS. We test that server A can deal with a request for the combined
    // CSS file, but we also check that server B can (this requires server B to
    // decode the instructions for which files to combine and combining them).
    //
    // TODO(sligocki): These need to be isolated! Specifically, they both
    // use the same filename_prefix and http_cache. They should use separate
    // ones so that their memory is not shared.
    scoped_ptr<ResourceManager> resource_manager(NewResourceManager(hasher));
    scoped_ptr<ResourceManager>
        other_resource_manager(NewResourceManager(hasher));

    rewrite_driver_.SetResourceManager(resource_manager.get());
    other_driver_.SetResourceManager(other_resource_manager.get());

    rewrite_driver_.AddFilter(RewriteOptions::kCombineCss);
    other_driver_.AddFilter(RewriteOptions::kCombineCss);

    // URLs and content for HTML document and resources.
    CHECK_EQ(StringPiece::npos, id.find("/"));
    std::string html_url = StrCat("http://combine_css.test/", id, ".html");
    const char a_css_url[] = "http://combine_css.test/a.css";
    const char b_css_url[] = "http://combine_css.test/b.css";
    const char c_css_url[] = "http://combine_css.test/c.css";

    static const char html_input_format[] =
        "<head>\n"
        "  <link rel='stylesheet' href='a.css' type='text/css'>\n"
        "  <link rel='stylesheet' href='b.css' type='text/css'>\n"
        "  <title>Hello, Instaweb</title>\n"
        "%s"
        "</head>\n"
        "<body>\n"
        "  <div class=\"c1\">\n"
        "    <div class=\"c2\">\n"
        "      Yellow on Blue\n"
        "    </div>\n"
        "  </div>\n"
        "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
        "</body>\n";
    std::string html_input = StringPrintf(html_input_format, barrier_text);
    const char a_css_body[] = ".c1 {\n background-color: blue;\n}\n";
    const char b_css_body[] = ".c2 {\n color: yellow;\n}\n";
    const char c_css_body[] = ".c3 {\n font-weight: bold;\n}\n";

    // Put original CSS files into our fetcher.
    SimpleMetaData default_css_header;
    resource_manager->SetDefaultHeaders(&kContentTypeCss, &default_css_header);
    mock_url_fetcher_.SetResponse(a_css_url, default_css_header, a_css_body);
    mock_url_fetcher_.SetResponse(b_css_url, default_css_header, b_css_body);
    mock_url_fetcher_.SetResponse(c_css_url, default_css_header, c_css_body);

    ParseUrl(html_url, html_input);

    std::string headers;
    AppendDefaultHeaders(kContentTypeCss, resource_manager.get(), &headers);

    // Check for CSS files in the rewritten page.
    StringVector css_urls;
    CollectCssLinks(id, output_buffer_, &css_urls);
    EXPECT_LE(1UL, css_urls.size());
    const std::string& combine_url = css_urls[0];

    GURL gurl(combine_url);
    std::string path = GoogleUrl::PathAndLeaf(gurl);

    std::string combine_filename;
    filename_encoder_.Encode(file_prefix_, combine_url, &combine_filename);

    // Expected CSS combination.
    // This syntax must match that in css_combine_filter
    std::string expected_combination = StrCat(a_css_body, b_css_body);
    if (!is_barrier) {
      expected_combination.append(c_css_body);
    }

    static const char expected_output_format[] =
        "<head>\n"
        "  <link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">\n"
        "  \n"  // The whitespace from the original link is preserved here ...
        "  <title>Hello, Instaweb</title>\n"
        "%s"
        "</head>\n"
        "<body>\n"
        "  <div class=\"c1\">\n"
        "    <div class=\"c2\">\n"
        "      Yellow on Blue\n"
        "    </div>\n"
        "  </div>\n"
        "  %s\n"
        "</body>\n";
    std::string expected_output = StringPrintf(
        expected_output_format, combine_url.c_str(), barrier_text,
        is_barrier ? "<link rel='stylesheet' href='c.css' type='text/css'>"
                   : "");
    EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);

    std::string actual_combination;
    ASSERT_TRUE(file_system_.ReadFile(combine_filename.c_str(),
                                      &actual_combination, &message_handler_));
    EXPECT_EQ(headers + expected_combination, actual_combination);

    // Fetch the combination to make sure we can serve the result from above.
    SimpleMetaData request_headers, response_headers;
    std::string fetched_resource_content;
    StringWriter writer(&fetched_resource_content);
    DummyCallback dummy_callback;
    rewrite_driver_.FetchResource(combine_url, request_headers,
                                  &response_headers, &writer,
                                  &message_handler_, &dummy_callback);
    EXPECT_EQ(HttpStatus::kOK, response_headers.status_code()) << combine_url;
    EXPECT_EQ(expected_combination, fetched_resource_content);

    // Now try to fetch from another server (other_driver_) that does not
    // already have the combination cached.
    // TODO(sligocki): This has too much shared state with the first server.
    // See RewriteImage for details.
    SimpleMetaData other_response_headers;
    fetched_resource_content.clear();
    message_handler_.Message(kInfo, "Now with serving.");
    file_system_.enable();
    other_driver_.FetchResource(combine_url, request_headers,
                                &other_response_headers, &writer,
                                &message_handler_, &dummy_callback);
    EXPECT_EQ(HttpStatus::kOK, other_response_headers.status_code()) << combine_url;
    EXPECT_EQ(expected_combination, fetched_resource_content);
    ServeResourceFromNewContext(combine_url, combine_filename,
                                fetched_resource_content,
                                RewriteOptions::kCombineCss, path.c_str());
  }

  // Simple image rewrite test to check resource fetching functionality.
  void RewriteImage() {
    // other_* are used as in CombineCss.
    // TODO(sligocki): Isolate resource managers from each other and share the
    // objects with those in CombineCss.
    scoped_ptr<ResourceManager>
        resource_manager(NewResourceManager(&mock_hasher_));
    scoped_ptr<ResourceManager>
        other_resource_manager(NewResourceManager(&mock_hasher_));

    rewrite_driver_.SetResourceManager(resource_manager.get());
    other_driver_.SetResourceManager(other_resource_manager.get());

    RewriteOptions options;
    options.EnableFilter(RewriteOptions::kRewriteImages);
    options.EnableFilter(RewriteOptions::kInsertImgDimensions);
    options.set_img_inline_max_bytes(2000);
    rewrite_driver_.AddFilters(options);
    other_driver_.AddFilter(RewriteOptions::kRewriteImages);

    // URLs and content for HTML document and resources.
    const char html_url[] = "http://rewrite_image.test/RewriteImage.html";
    const char image_url[] = "http://rewrite_image.test/Puzzle.jpg";

    const char image_html[] = "<head/><body><img src=\"Puzzle.jpg\"/></body>";

    // Store image contents in a string.
    // TODO(sligocki): There's probably a lot of wasteful copying here.
    std::string image_filename =
        StrCat(GTestSrcDir(), kTestData, "Puzzle.jpg");
    std::string image_body;
    file_system_.ReadFile(image_filename.c_str(), &image_body,
                          &message_handler_);

    // Put original image into our fetcher.
    SimpleMetaData default_jpg_header;
    resource_manager->SetDefaultHeaders(&kContentTypeJpeg, &default_jpg_header);
    mock_url_fetcher_.SetResponse(image_url, default_jpg_header, image_body);

    // Rewrite the HTML page.
    ParseUrl(html_url, image_html);
    StringVector img_srcs;
    CollectImgSrcs("RewriteImage/collect_sources", output_buffer_, &img_srcs);
    // output_buffer_ should have exactly one image file (Puzzle.jpg).
    EXPECT_EQ(1UL, img_srcs.size());
    const std::string& src_string = img_srcs[0];
    EXPECT_EQ(url_prefix_, src_string.substr(0, url_prefix_.size()));
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
    DummyCallback dummy_callback;

    std::string headers;
    AppendDefaultHeaders(kContentTypeJpeg, resource_manager.get(), &headers);

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
    file_system_.disable();
    // 3) Disable the fetcher, so that the fetch doesn't finish imidiately.
    // TODO(sligocki): We could use the CallCallbacks() trick to get a more
    // correct response, rather than effectively shutting off our network.
    mock_url_fetcher_.Disable();

    fetched_resource_content.clear();
    SimpleMetaData redirect_headers;
    other_driver_.FetchResource(src_string, request_headers,
                                &redirect_headers, &writer,
                                &message_handler_, &dummy_callback);
    std::string expected_redirect =
        StrCat("<img src=\"", image_url, "\" alt=\"Temporarily Moved\"/>");

    EXPECT_EQ(HttpStatus::kTemporaryRedirect, redirect_headers.status_code());
    EXPECT_EQ(expected_redirect, fetched_resource_content);

    // Now we switch the file system and fetcher back on, corresponding to
    // the case where a prior async fetch completed and we're ready to rewrite.
    // This should yield the same results as the original html- driven rewrite.
    file_system_.enable();
    mock_url_fetcher_.Enable();

    message_handler_.Message(kInfo, "Now with serving.");
    fetched_resource_content.clear();
    writer.Write(headers, &message_handler_);
    EXPECT_EQ(headers, fetched_resource_content);
    SimpleMetaData other_headers;
    size_t header_size = fetched_resource_content.size();
    other_driver_.FetchResource(src_string, request_headers,
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
    ServeResourceFromNewContext(src_string, rewritten_filename,
                                fetched_resource_content.substr(header_size),
                                RewriteOptions::kRewriteImages, NULL);
  }

  void DataUrlResource() {
    scoped_ptr<ResourceManager>
        resource_manager(NewResourceManager(&mock_hasher_));

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
        resource_manager->CreateInputResourceAbsolute(cuppa_string,
                                                      &message_handler_));
    ASSERT_TRUE(cuppa_resource != NULL);
    EXPECT_TRUE(resource_manager->ReadIfCached(cuppa_resource.get(),
                                               &message_handler_));
    std::string cuppa_contents;
    cuppa_resource->contents().CopyToString(&cuppa_contents);
    // Now make sure axing the original cuppa_string doesn't affect the
    // internals of the cuppa_resource.
    scoped_ptr<Resource> other_resource(
        resource_manager->CreateInputResourceAbsolute(cuppa_string,
                                                      &message_handler_));
    ASSERT_TRUE(other_resource != NULL);
    cuppa_string.clear();
    EXPECT_TRUE(resource_manager->ReadIfCached(other_resource.get(),
                                               &message_handler_));
    std::string other_contents;
    cuppa_resource->contents().CopyToString(&other_contents);
    ASSERT_EQ(cuppa_contents, other_contents);
  }

  void TestInlineCss(const std::string& html_url,
                     const std::string& css_url,
                     const std::string& other_attrs,
                     const std::string& css_original_body,
                     bool expect_inline,
                     const std::string& css_rewritten_body) {
    scoped_ptr<ResourceManager> resource_manager(
        NewResourceManager(&mock_hasher_));
    rewrite_driver_.SetResourceManager(resource_manager.get());
    rewrite_driver_.AddFilter(RewriteOptions::kInlineCss);

    const std::string html_input =
        "<head>\n"
        "  <link rel=\"stylesheet\" href=\"" + css_url + "\"" +
        (other_attrs.empty() ? "" : " " + other_attrs) + ">\n"
        "</head>\n"
        "<body>Hello, world!</body>\n";

    // Put original CSS file into our fetcher.
    SimpleMetaData default_css_header;
    resource_manager->SetDefaultHeaders(&kContentTypeCss,
                                        &default_css_header);
    mock_url_fetcher_.SetResponse(css_url, default_css_header,
                                  css_original_body);

    // Rewrite the HTML page.
    ParseUrl(html_url, html_input);

    const std::string expected_output =
        (!expect_inline ? html_input :
         "<head>\n"
         "  <style>" + css_rewritten_body + "</style>\n"
         "</head>\n"
         "<body>Hello, world!</body>\n");
    EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);
  }

  void TestInlineJavascript(const std::string& html_url,
                            const std::string& js_url,
                            const std::string& js_inline_body,
                            const std::string& js_outline_body,
                            bool expect_inline) {
    scoped_ptr<ResourceManager> resource_manager(
        NewResourceManager(&mock_hasher_));
    rewrite_driver_.SetResourceManager(resource_manager.get());
    rewrite_driver_.AddFilter(RewriteOptions::kInlineJavascript);

    const std::string html_input =
        "<head>\n"
        "  <script src=\"" + js_url + "\">" + js_inline_body + "</script>\n"
        "</head>\n"
        "<body>Hello, world!</body>\n";

    // Put original Javascript file into our fetcher.
    SimpleMetaData default_js_header;
    resource_manager->SetDefaultHeaders(&kContentTypeJavascript,
                                        &default_js_header);
    mock_url_fetcher_.SetResponse(js_url, default_js_header, js_outline_body);

    // Rewrite the HTML page.
    ParseUrl(html_url, html_input);

    const std::string expected_output =
        (!expect_inline ? html_input :
         "<head>\n"
         "  <script>" + js_outline_body + "</script>\n"
         "</head>\n"
         "<body>Hello, world!</body>\n");
    EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);
  }

  // Test outlining styles with options to write headers and use a hasher.
  void OutlineStyle(const StringPiece& id, Hasher* hasher) {
    scoped_ptr<ResourceManager> resource_manager(NewResourceManager(hasher));
    rewrite_driver_.SetResourceManager(resource_manager.get());
    RewriteOptions options;
    options.EnableFilter(RewriteOptions::kOutlineCss);
    options.set_css_outline_min_bytes(0);
    rewrite_driver_.AddFilters(options);

    std::string style_text = "background_blue { background-color: blue; }\n"
                              "foreground_yellow { color: yellow; }\n";
    std::string outline_text;
    AppendDefaultHeaders(kContentTypeCss, resource_manager.get(),
                         &outline_text);
    outline_text += style_text;

    std::string hash = hasher->Hash(style_text);
    std::string outline_filename;
    std::string outline_url = StrCat(
        "http://test.com/", CssOutlineFilter::kFilterId, ".", hash, "._.css");
    filename_encoder_.Encode(file_prefix_, outline_url, &outline_filename);

    // Make sure the file we check later was written this time, rm any old one.
    DeleteFileIfExists(outline_filename);

    std::string html_input =
        "<head>\n"
        "  <title>Example style outline</title>\n"
        "  <!-- Style starts here -->\n"
        "  <style type='text/css'>" + style_text + "</style>\n"
        "  <!-- Style ends here -->\n"
        "</head>";
    std::string expected_output =
        "<head>\n"
        "  <title>Example style outline</title>\n"
        "  <!-- Style starts here -->\n"
        "  <link rel='stylesheet' href='" + outline_url + "' type='text/css'>\n"
        "  <!-- Style ends here -->\n"
        "</head>";
    ValidateExpected(id, html_input, expected_output);

    std::string actual_outline;
    ASSERT_TRUE(file_system_.ReadFile(outline_filename.c_str(),
                                      &actual_outline,
                                      &message_handler_));
    EXPECT_EQ(outline_text, actual_outline);
  }

  // TODO(sligocki): factor out common elements in OutlineStyle and Script.
  // Test outlining scripts with options to write headers and use a hasher.
  void OutlineScript(const StringPiece& id, Hasher* hasher) {
    scoped_ptr<ResourceManager> resource_manager(NewResourceManager(hasher));
    rewrite_driver_.SetResourceManager(resource_manager.get());
    RewriteOptions options;
    options.EnableFilter(RewriteOptions::kOutlineJavascript);
    options.set_js_outline_min_bytes(0);
    rewrite_driver_.AddFilters(options);

    std::string script_text = "FOOBAR";
    std::string outline_text;
    AppendDefaultHeaders(kContentTypeJavascript, resource_manager.get(),
                         &outline_text);
    outline_text += script_text;

    std::string hash = hasher->Hash(script_text);
    std::string outline_filename;
    std::string outline_url = StrCat(
        "http://test.com/", JsOutlineFilter::kFilterId, ".", hash, "._.js");
    filename_encoder_.Encode(file_prefix_, outline_url, &outline_filename);

    // Make sure the file we check later was written this time, rm any old one.
    DeleteFileIfExists(outline_filename);

    std::string html_input =
        "<head>\n"
        "  <title>Example style outline</title>\n"
        "  <!-- Script starts here -->\n"
        "  <script type='text/javascript'>" + script_text + "</script>\n"
        "  <!-- Script ends here -->\n"
        "</head>";
    std::string expected_output =
        "<head>\n"
        "  <title>Example style outline</title>\n"
        "  <!-- Script starts here -->\n"
        "  <script src='" + outline_url + "' type='text/javascript'></script>\n"
        "  <!-- Script ends here -->\n"
        "</head>";
    ValidateExpected(id, html_input, expected_output);

    std::string actual_outline;
    ASSERT_TRUE(file_system_.ReadFile(outline_filename.c_str(),
                                      &actual_outline,
                                      &message_handler_));
    EXPECT_EQ(outline_text, actual_outline);
  }

  // Helper class to collect CSS hrefs.
  class CssCollector : public EmptyHtmlFilter {
   public:
    CssCollector(HtmlParse* html_parse, StringVector* css_links)
        : css_links_(css_links),
          css_tag_scanner_(html_parse) {
    }

    virtual void EndElement(HtmlElement* element) {
      HtmlElement::Attribute* href;
      const char* media;
      if (css_tag_scanner_.ParseCssElement(element, &href, &media)) {
        css_links_->push_back(href->value());
      }
    }

    virtual const char* Name() const { return "CssCollector"; }

   private:
    StringVector* css_links_;
    CssTagScanner css_tag_scanner_;

    DISALLOW_COPY_AND_ASSIGN(CssCollector);
  };

  void CollectCssLinks(const StringPiece& id, const StringPiece& html,
                       StringVector* css_links) {
    HtmlParse html_parse(&message_handler_);
    CssCollector collector(&html_parse, css_links);
    html_parse.AddFilter(&collector);
    std::string dummy_url = StrCat("http://collect.css.links/", id, ".html");
    html_parse.StartParse(dummy_url);
    html_parse.ParseText(html.data(), html.size());
    html_parse.FinishParse();
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

  virtual HtmlParse* html_parse() { return rewrite_driver_.html_parse(); }

  // Another driver on a different server. For testing resource fetchs on
  // servers that did not perform the original HTML rewrite.
  RewriteDriver other_driver_;
  MD5Hasher md5_hasher_;

  DISALLOW_COPY_AND_ASSIGN(RewriterTest);
};

TEST_F(RewriterTest, AddHead) {
  rewrite_driver_.AddHead();
  ValidateExpected("add_head",
      "<body><p>text</p></body>",
      "<head/><body><p>text</p></body>");
}

TEST_F(RewriterTest, MergeHead) {
  rewrite_driver_.AddFilter(RewriteOptions::kCombineHeads);
  ValidateExpected("merge_2_heads",
      "<head a><p>1</p></head>4<head b>2<link x>3</head><link y>end",
      "<head a><p>1</p>2<link x>3</head>4<link y>end");
  ValidateExpected("merge_3_heads",
      "<head a><p>1</p></head>4<head b>2<link x>3</head><link y>"
      "<body>b<head><link z></head>ye</body>",
      "<head a><p>1</p>2<link x>3<link z></head>4<link y>"
      "<body>bye</body>");
}

TEST_F(RewriterTest, BaseTagNoHead) {
  rewrite_driver_.AddFilter(RewriteOptions::kAddBaseTag);
  rewrite_driver_.SetBaseUrl("http://base");
  ValidateExpected("base_tag",
      "<body><p>text</p></body>",
      "<head><base href=\"http://base\"></head><body><p>text</p></body>");
}

TEST_F(RewriterTest, BaseTagExistingHead) {
  rewrite_driver_.AddFilter(RewriteOptions::kAddBaseTag);
  rewrite_driver_.SetBaseUrl("http://base");
  ValidateExpected("base_tag",
      "<head><meta></head><body><p>text</p></body>",
      "<head><base href=\"http://base\"><meta></head><body><p>text</p></body>");
}

TEST_F(RewriterTest, BaseTagExistingHeadAndNonHrefBase) {
  rewrite_driver_.AddFilter(RewriteOptions::kAddBaseTag);
  rewrite_driver_.SetBaseUrl("http://base");
  ValidateExpected("base_tag",
      "<head><base x><meta></head><body></body>",
      "<head><base href=\"http://base\"><base x><meta></head><body></body>");
}

TEST_F(RewriterTest, BaseTagExistingHeadAndHrefBase) {
  rewrite_driver_.AddFilter(RewriteOptions::kAddBaseTag);
  rewrite_driver_.SetBaseUrl("http://base");
  ValidateExpected("base_tag",
      "<head><meta><base href=\"http://old\"></head><body></body>",
      "<head><base href=\"http://base\"><meta></head><body></body>");
}

TEST_F(RewriterTest, CombineCss) {
  CombineCss("combine_css_no_hash", &mock_hasher_, "", false);
}

TEST_F(RewriterTest, CombineCssMD5) {
  CombineCss("combine_css_md5", &md5_hasher_, "", false);
}


TEST_F(RewriterTest, CombineCssWithIEDirective) {
  const char ie_directive_barrier[] =
      "<!--[if IE]>\n"
      "<link rel=\"stylesheet\" type=\"text/css\" "
      "href=\"http://graphics8.nytimes.com/css/"
      "0.1/screen/build/homepage/ie.css\">\n"
      "<![endif]-->";
  CombineCss("combine_css_ie", &md5_hasher_, ie_directive_barrier, true);
}

TEST_F(RewriterTest, CombineCssWithStyle) {
  const char style_barrier[] = "<style>a { color: red }</style>\n";
  CombineCss("combine_css_style", &md5_hasher_, style_barrier, true);
}

TEST_F(RewriterTest, CombineCssWithMediaBarrier) {
  const char media_barrier[] =
      "<link rel='stylesheet' type='text/css' href='d.css' media='print'>\n";

  const char d_css_url[] = "http://combine_css.test/d.css";
  const char d_css_body[] = ".c4 {\n color: green;\n}\n";
  SimpleMetaData default_css_header;
  resource_manager_->SetDefaultHeaders(&kContentTypeCss, &default_css_header);
  mock_url_fetcher_.SetResponse(d_css_url, default_css_header, d_css_body);

  CombineCss("combine_css_media", &md5_hasher_, media_barrier, true);
}

TEST_F(RewriterTest, CombineCssWithNonMediaBarrier) {
  // Put original CSS files into our fetcher.
  const char html_url[] = "http://combine_css.test/no_media_barrier.html";
  const char a_css_url[] = "http://combine_css.test/a.css";
  const char b_css_url[] = "http://combine_css.test/b.css";
  const char c_css_url[] = "http://combine_css.test/c.css";
  const char d_css_url[] = "http://combine_css.test/d.css";

  const char a_css_body[] = ".c1 {\n background-color: blue;\n}\n";
  const char b_css_body[] = ".c2 {\n color: yellow;\n}\n";
  const char c_css_body[] = ".c3 {\n font-weight: bold;\n}\n";
  const char d_css_body[] = ".c4 {\n color: green;\n}\n";

  SimpleMetaData default_css_header;
  resource_manager_->SetDefaultHeaders(&kContentTypeCss, &default_css_header);
  mock_url_fetcher_.SetResponse(a_css_url, default_css_header, a_css_body);
  mock_url_fetcher_.SetResponse(b_css_url, default_css_header, b_css_body);
  mock_url_fetcher_.SetResponse(c_css_url, default_css_header, c_css_body);
  mock_url_fetcher_.SetResponse(d_css_url, default_css_header, d_css_body);

  // Only the first two CSS files should be combined.
  const char html_input[] =
      "<head>\n"
      "  <link rel='stylesheet' type='text/css' href='a.css' media='print'>\n"
      "  <link rel='stylesheet' type='text/css' href='b.css' media='print'>\n"
      "  <link rel='stylesheet' type='text/css' href='c.css'>\n"
      "  <link rel='stylesheet' type='text/css' href='d.css' media='print'>\n"
      "</head>";

  // Rewrite
  rewrite_driver_.AddFilter(RewriteOptions::kCombineCss);
  ParseUrl(html_url, html_input);

  // Check for CSS files in the rewritten page.
  StringVector css_urls;
  CollectCssLinks("combine_css_no_media-links", output_buffer_, &css_urls);
  EXPECT_EQ(3UL, css_urls.size());
  const std::string& combine_url = css_urls[0];

  const char expected_output_format[] =
      "<head>\n"
      "  <link rel=\"stylesheet\" type=\"text/css\" media=\"print\" "
      "href=\"%s\">\n"
      "  \n"
      "  <link rel='stylesheet' type='text/css' href='c.css'>\n"
      "  <link rel='stylesheet' type='text/css' href='d.css' media='print'>\n"
      "</head>";
  std::string expected_output = StringPrintf(expected_output_format,
                                              combine_url.c_str());

  EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);
}

TEST_F(RewriterTest, CombineCssShards) {
  num_shards_ = 10;
  url_prefix_ = "http://mysite%d/";
  CombineCss("combine_css_sha1", &mock_hasher_, "", false);
}

TEST_F(RewriterTest, CombineCssNoInput) {
  // TODO(sligocki): This is probably not working correctly, we need to put
  // a_broken.css and b.css in mock_fetcher_ ... not sure why this isn't
  // crashing right now.
  static const char html_input[] =
      "<head>\n"
      "  <link rel='stylesheet' href='a_broken.css' type='text/css'>\n"
      "  <link rel='stylesheet' href='b.css' type='text/css'>\n"
      "</head>\n"
      "<body><div class=\"c1\"><div class=\"c2\"><p>\n"
      "  Yellow on Blue</p></div></div></body>";
  ValidateNoChanges("combine_css_missing_input", html_input);
}

// TODO(sligocki): Test that using the DummyUrlFetcher causes FatalError.

TEST_F(RewriterTest, InlineCssSimple) {
  const std::string css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "", css, true, css);
}

TEST_F(RewriterTest, InlineCssAbsolutifyUrls1) {
  // CSS with a relative URL that needs to be changed:
  const std::string css1 =
      "BODY { background-image: url('bg.png'); }\n";
  const std::string css2 =
      "BODY { background-image: "
      "url('http://www.example.com/foo/bar/bg.png'); }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/foo/bar/baz.css",
                "", css1, true, css2);
}

TEST_F(RewriterTest, InlineCssAbsolutifyUrls2) {
  // CSS with a relative URL, this time with ".." in it:
  const std::string css1 =
      "BODY { background-image: url('../quux/bg.png'); }\n";
  const std::string css2 =
      "BODY { background-image: "
      "url('http://www.example.com/foo/quux/bg.png'); }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/foo/bar/baz.css",
                "", css1, true, css2);
}

TEST_F(RewriterTest, DoNotInlineCssWithMediaAttr) {
  const std::string css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "media=\"print\"", css, false, "");
}

TEST_F(RewriterTest, DoInlineCssWithMediaAll) {
  const std::string css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "media=\"all\"", css, true, css);
}

TEST_F(RewriterTest, DoNotInlineCssTooBig) {
  // CSS too large to inline:
  const int64 length = 2 * RewriteOptions::kDefaultCssInlineMaxBytes;
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css", "",
                ("BODY { background-image: url('" +
                 std::string(length, 'z') + ".png'); }\n"),
                false, "");
}

TEST_F(RewriterTest, DoNotInlineCssDifferentDomain) {
  // TODO(mdsteele): Is switching domains in fact an issue for CSS?
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.org/styles.css",
                "", "BODY { color: red; }\n", false, "");
}

TEST_F(RewriterTest, DoInlineJavascriptSimple) {
  // Simple case:
  TestInlineJavascript("http://www.example.com/index.html",
                       "http://www.example.com/script.js",
                       "",
                       "function id(x) { return x; }\n",
                       true);
}

TEST_F(RewriterTest, DoInlineJavascriptWhitespace) {
  // Whitespace between <script> and </script>:
  TestInlineJavascript("http://www.example.com/index2.html",
                       "http://www.example.com/script2.js",
                       "\n    \n  ",
                       "function id(x) { return x; }\n",
                       true);
}

TEST_F(RewriterTest, DoNotInlineJavascriptDifferentDomain) {
  // Different domains:
  TestInlineJavascript("http://www.example.net/index.html",
                       "http://scripts.example.org/script.js",
                       "",
                       "function id(x) { return x; }\n",
                       false);
}

TEST_F(RewriterTest, DoNotInlineJavascriptInlineContents) {
  // Inline contents:
  TestInlineJavascript("http://www.example.com/index.html",
                       "http://www.example.com/script.js",
                       "{\"json\": true}",
                       "function id(x) { return x; }\n",
                       false);
}

TEST_F(RewriterTest, DoNotInlineJavascriptTooBig) {
  // Javascript too long:
  const int64 length = 2 * RewriteOptions::kDefaultJsInlineMaxBytes;
  TestInlineJavascript("http://www.example.com/index.html",
                       "http://www.example.com/script.js",
                       "",
                       ("function longstr() { return '" +
                        std::string(length, 'z') + "'; }\n"),
                       false);
}

// Tests for Outlining styles.
TEST_F(RewriterTest, OutlineStyle) {
  OutlineStyle("outline_styles_no_hash", &mock_hasher_);
}

TEST_F(RewriterTest, OutlineStyleMD5) {
  OutlineStyle("outline_styles_md5", &md5_hasher_);
}


// Tests for outlining scripts.
TEST_F(RewriterTest, OutlineScript) {
  OutlineScript("outline_scripts_no_hash_with_headers", &mock_hasher_);
}

// Negative test.
TEST_F(RewriterTest, NoOutlineScript) {
  std::string file_prefix = GTestTempDir() + "/no_outline";
  std::string url_prefix = "http://mysite/no_outline";

  Hasher* hasher = NULL;
  scoped_ptr<ResourceManager> resource_manager(NewResourceManager(hasher));
  rewrite_driver_.SetResourceManager(resource_manager.get());
  RewriteOptions options;
  options.EnableFilter(RewriteOptions::kOutlineCss);
  options.EnableFilter(RewriteOptions::kOutlineJavascript);
  rewrite_driver_.AddFilters(options);

  // We need to make sure we don't create this file, so rm any old one
  DeleteFileIfExists(StrCat(file_prefix, JsOutlineFilter::kFilterId,
                            ".0._.js"));

  static const char html_input[] =
      "<head>\n"
      "  <title>Example style outline</title>\n"
      "  <!-- Script starts here -->\n"
      "  <script type='text/javascript' src='http://othersite/script.js'>"
      "</script>\n"
      "  <!-- Script ends here -->\n"
      "</head>";
  ValidateNoChanges("no_outline_script", html_input);

  // Check that it did *NOT* create the file.
  // TODO(jmarantz): this is pretty brittle, and perhaps obsolete.
  // We just change the test to ensure that we are not outlining when
  // we don't want to.
  EXPECT_FALSE(file_system_.Exists((file_prefix + "0._.js").c_str(),
                                   &message_handler_).is_true());
}

TEST_F(RewriterTest, ImageRewriteTest) {
  RewriteImage();
}

TEST_F(RewriterTest, DataUrlTest) {
  DataUrlResource();
}

// TODO(jmarantz): add CacheExtender test.  I want to refactor CombineCss,
// RewriteImage, and OutlineStyle first if I can cause there should be more
// sharing.

}  // namespace net_instaweb
