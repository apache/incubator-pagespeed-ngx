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
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/img_tag_scanner.h"
#include "net/instaweb/rewriter/public/outline_filter.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/hasher.h"
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
const std::string kPuzzle = "Puzzle.jpg";

class RewriterTest : public ResourceManagerTestBase {
 protected:
  RewriterTest() : nobase_driver_(&message_handler_, &file_system_,
                                    &dummy_url_async_fetcher_) {
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

  void ServeResourceFromNewContext(const StringPiece& resource,
                                   const std::string& output_filename,
                                   const StringPiece& expected_content,
                                   const std::string& filter_names) {
    scoped_ptr<ResourceManager> resource_manager(
        NewResourceManager(&mock_hasher_));
    SimpleStats stats;
    RewriteDriver::Initialize(&stats);
    resource_manager->set_statistics(&stats);
    RewriteDriver driver(&message_handler_, &file_system_,
                         &dummy_url_async_fetcher_);
    driver.SetResourceManager(resource_manager.get());
    driver.AddFiltersByCommaSeparatedList(filter_names);
    Variable* resource_fetches =
        stats.GetVariable(RewriteDriver::kResourceFetches);
    SimpleMetaData request_headers, response_headers;
    std::string contents;
    StringWriter writer(&contents);
    DummyCallback callback;

    // Delete the output resource from the cache and the file system.
    std::string cache_key = resource_manager->GenerateUrl(resource);
    EXPECT_EQ(CacheInterface::kAvailable, http_cache_.Query(cache_key));
    lru_cache_->Clear();

    // Now delete it from the file system, so it must be recomputed.
    EXPECT_TRUE(file_system_.RemoveFile(output_filename.c_str(),
                                        &message_handler_));

    driver.FetchResource(resource, request_headers, &response_headers, &writer,
                         &message_handler_, &callback);
    EXPECT_EQ(expected_content, contents);
    EXPECT_EQ(1, resource_fetches->Get());
  }

  // Test spriting CSS with options to write headers and use a hasher.
  // Use hasher = NULL to use FilenameResources.
  void CombineCss(const char* id, Hasher* hasher) {
    // Here nobase_driver_ is used to do resource-only fetches for testing.
    // The nobase_resource_manager is the corresponding resource manager
    // (a resource manager for resource-only requests).
    // We initialize rewrite_driver_ and nobase_driver_ together as they
    // ought to match *except for the call to rewrite_driver_.SetBaseUrl*.
    // TODO(jmaessen): Actually do resource fetch testing here.
    scoped_ptr<ResourceManager> resource_manager(NewResourceManager(hasher));
    scoped_ptr<ResourceManager>
        nobase_resource_manager(NewResourceManager(hasher));

    rewrite_driver_.SetResourceManager(resource_manager.get());
    nobase_driver_.SetResourceManager(nobase_resource_manager.get());

    rewrite_driver_.AddFiltersByCommaSeparatedList("combine_css");
    nobase_driver_.AddFiltersByCommaSeparatedList("combine_css");

    const char a_css[] = ".c1 {\n background-color: blue;\n}\n";
    const char b_css[] = ".c2 {\n color: yellow;\n}\n";

    std::string headers, expected_combination;
    AppendDefaultHeaders(kContentTypeCss, resource_manager.get(), &headers);

    // This syntax must match that in css_combine_filter
    expected_combination = StrCat(headers, a_css,
                                  "\n@Media print {\n",
                                  b_css,
                                  "\n}\n");
    std::string hash = (hasher == NULL) ? "0" :
        hasher->Hash(expected_combination);
    std::string combine_url = StrCat(url_prefix_, hash, ".css");
    std::string a_css_file = GTestTempDir() + "/a.css";
    std::string b_css_file = GTestTempDir() + "/b.css";
    ASSERT_TRUE(file_system_.WriteFile(a_css_file.c_str(),
                                       a_css, &message_handler_));
    ASSERT_TRUE(file_system_.WriteFile(b_css_file.c_str(),
                                       b_css, &message_handler_));

    static const char html_input[] =
        "<head>\n"
        "  <link rel='stylesheet' href='a.css' type='text/css'>\n"
        "  <link rel='stylesheet' href='b.css' type='text/css' media='print'>\n"
        "</head>\n"
        "<body><div class=\"c1\"><div class=\"c2\"><p>\n"
        "  Yellow on Blue</p></div></div></body>";

    std::string base_url("file://");
    base_url += GTestTempDir() + "/";
    rewrite_driver_.SetBaseUrl(base_url.c_str());

    Parse(id, html_input);
    StringVector css_hrefs;
    CollectCssLinks(id, output_buffer_, &css_hrefs);
    // output_buffer_ should have exactly one CSS file (the combined one).
    EXPECT_EQ(1UL, css_hrefs.size());
    const std::string& href_string = css_hrefs[0];
    int shard;
    const char* resource = resource_manager->SplitUrl(
        href_string.c_str(), &shard);
    EXPECT_TRUE(resource != NULL);

    std::string combine_filename;
    filename_encoder_.Encode(file_prefix_, resource, &combine_filename);

    std::string expected_output =
        "<head>\n"
        "  \n"  // The whitespace from the original links is preserved
        "  \n"
        "<link rel=\"stylesheet\" type=\"text/css\" "
        "href=\"" + href_string + "\"></head>\n"
        "<body><div class=\"c1\"><div class=\"c2\"><p>\n"
        "  Yellow on Blue</p></div></div></body>";
    EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);

    std::string actual_combination;
    ASSERT_TRUE(file_system_.ReadFile(combine_filename.c_str(),
                                      &actual_combination, &message_handler_));
    EXPECT_EQ(expected_combination, actual_combination);

    // Also fetch the resource to ensure it can be created dynamically
    SimpleMetaData request_headers, response_headers;
    std::string fetched_resource_content;
    StringWriter writer(&fetched_resource_content);
    DummyCallback dummy_callback;
    rewrite_driver_.FetchResource(resource, request_headers,
                                  &response_headers, &writer,
                                  &message_handler_, &dummy_callback);
    EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());
    EXPECT_EQ(expected_combination, headers + fetched_resource_content);
    // TODO(jmaessen): How do we deal with the "initial fetch" problem
    // for css concatenation?  We can't redirect, as we aren't doing a
    // 1-for-1 replacement.  It is likely we will need to support async
    // resource provision so that the server can block until the resources
    // are available.  Perhaps we need a listener-type interface for
    // resources that are being fetched.
    //     SimpleMetaData redirect_headers;
    //     file_system_.disable();
    //     fetched_resource_content.clear();
    //     nobase_driver_.FetchResource(resource, request_headers,
    //                                  &redirect_headers, &writer,
    //                                  &message_handler_, &dummy_callback);
    //     std::string expected_redirect = ???
    //     EXPECT_EQ(???, redirect_headers.status_code());
    //     EXPECT_EQ(expected_redirect, fetched_resource_content);
    // Now we switch the file system back on, corresponding to the case where a
    // prior async fetch completed and we're ready to rewrite.  This should
    // yield the same results as the original html- driven rewrite.
    SimpleMetaData nobase_headers;
    message_handler_.Message(kInfo, "Now with serving.");
    file_system_.enable();
    fetched_resource_content.clear();
    nobase_driver_.FetchResource(resource, request_headers,
                                 &nobase_headers, &writer,
                                 &message_handler_, &dummy_callback);
    EXPECT_EQ(HttpStatus::kOK, nobase_headers.status_code());
    EXPECT_EQ(expected_combination, headers + fetched_resource_content);
    ServeResourceFromNewContext(resource, combine_filename,
                                fetched_resource_content, "combine_css");
  }

  // Simple image rewrite test to check resource fetching functionality.
  void RewriteImage() {
    static const char* kId = "RewriteImage";
    // As above, here nobase_driver_ is used to do resource-only fetches for
    // testing.  The nobase_resource_manager is the corresponding resource
    // manager (a resource manager for resource-only requests).  We again
    // initialize rewrite_driver_ and nobase_driver_ together as they ought to
    // match *except for the call to rewrite_driver_.SetBaseUrl*.
    scoped_ptr<ResourceManager>
        resource_manager(NewResourceManager(&mock_hasher_));
    scoped_ptr<ResourceManager>
        nobase_resource_manager(NewResourceManager(&mock_hasher_));

    rewrite_driver_.SetResourceManager(resource_manager.get());
    nobase_driver_.SetResourceManager(nobase_resource_manager.get());

    rewrite_driver_.AddFiltersByCommaSeparatedList("rewrite_images");
    nobase_driver_.AddFiltersByCommaSeparatedList("rewrite_images");

    std::string baseUrl = StrCat("file://", GTestSrcDir(), kTestData);
    rewrite_driver_.SetBaseUrl(baseUrl);

    const std::string image_html =
        StrCat("<head/><body><img src=\"", kPuzzle, "\"/></body>");

    std::string headers;
    AppendDefaultHeaders(kContentTypeJpeg, resource_manager.get(), &headers);

    Parse(kId, image_html.c_str());
    StringVector img_srcs;
    CollectImgSrcs(kId, output_buffer_, &img_srcs);
    // output_buffer_ should have exactly one image file (Puzzle.jpg).
    EXPECT_EQ(1UL, img_srcs.size());
    const std::string& src_string = img_srcs[0];
    EXPECT_EQ(url_prefix_, src_string.substr(0, url_prefix_.size()));

    const StringPiece resource =
        StringPiece(src_string).substr(url_prefix_.size());
    std::string rewritten_data;

    std::string expected_output =
        StrCat("<head/><body><img src=\"", src_string, "\"/></body>");
    EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);

    std::string rewritten_filename;
    filename_encoder_.Encode(file_prefix_, resource, &rewritten_filename);

    std::string rewritten_image_data;
    ASSERT_TRUE(file_system_.ReadFile(rewritten_filename.c_str(),
                                      &rewritten_image_data,
                                      &message_handler_));

    // Also fetch the resource to ensure it can be created dynamically
    SimpleMetaData request_headers, response_headers;
    std::string fetched_resource_content;
    StringWriter writer(&fetched_resource_content);
    DummyCallback dummy_callback;
    writer.Write(headers, &message_handler_);
    rewrite_driver_.FetchResource(resource, request_headers,
                                  &response_headers, &writer,
                                  &message_handler_, &dummy_callback);
    EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());
    // For readability, only do EXPECT_EQ on initial portions of data
    // as most of it isn't human-readable.  This will show us the headers
    // and the start of the image data.  So far every failure fails this
    // first, and we caught doubled headers this way.
    EXPECT_EQ(rewritten_image_data.substr(0, 100),
              fetched_resource_content.substr(0, 100));
    EXPECT_TRUE(rewritten_image_data == fetched_resource_content);
    // Now fetch without a base url set.  First simulate the "first get" of a
    // rewritten url that this server instance has never seen before.  We do
    // this by disabling the file system, which corresponds to having the
    // initial resource fetch fail while an asynchronous fetch is initiated.  We
    // want to make sure that FetchResource deals with this case robustly so the
    // client doesn't notice.
    //
    // We must also flush the cache.
    lru_cache_->Clear();
    SimpleMetaData redirect_headers;
    file_system_.disable();
    fetched_resource_content.clear();

    nobase_driver_.FetchResource(resource, request_headers,
                                 &redirect_headers, &writer,
                                 &message_handler_, &dummy_callback);
    std::string expected_redirect =
        StrCat("<img src=\"", baseUrl, kPuzzle,
               "\" alt=\"Temporarily Moved\"/>");
    EXPECT_EQ(HttpStatus::kTemporaryRedirect, redirect_headers.status_code());
    EXPECT_EQ(expected_redirect, fetched_resource_content);
    // Now we switch the file system back on, corresponding to the case where a
    // prior async fetch completed and we're ready to rewrite.  This should
    // yield the same results as the original html- driven rewrite.
    SimpleMetaData nobase_headers;
    message_handler_.Message(kInfo, "Now with serving.");
    file_system_.enable();
    fetched_resource_content.clear();
    writer.Write(headers, &message_handler_);
    EXPECT_EQ(headers, fetched_resource_content);
    size_t header_size = fetched_resource_content.size();
    nobase_driver_.FetchResource(resource, request_headers,
                                 &nobase_headers, &writer,
                                 &message_handler_, &dummy_callback);
    EXPECT_EQ(HttpStatus::kOK, nobase_headers.status_code());
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
    ServeResourceFromNewContext(resource, rewritten_filename,
                                fetched_resource_content.substr(header_size),
                                "rewrite_images");
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
        resource_manager->CreateInputResource(cuppa_string, &message_handler_));
    ASSERT_TRUE(cuppa_resource != NULL);
    EXPECT_TRUE(resource_manager->ReadIfCached(cuppa_resource.get(),
                                               &message_handler_));
    std::string cuppa_contents;
    cuppa_resource->contents().CopyToString(&cuppa_contents);
    // Now make sure axing the original cuppa_string doesn't affect the
    // internals of the cuppa_resource.
    scoped_ptr<Resource> other_resource(
        resource_manager->CreateInputResource(cuppa_string, &message_handler_));
    ASSERT_TRUE(other_resource != NULL);
    cuppa_string.clear();
    EXPECT_TRUE(resource_manager->ReadIfCached(other_resource.get(),
                                               &message_handler_));
    std::string other_contents;
    cuppa_resource->contents().CopyToString(&other_contents);
    ASSERT_EQ(cuppa_contents, other_contents);
  }

  // Test outlining styles with options to write headers and use a hasher.
  // Use hasher = NULL to use FilenameResources.
  void OutlineStyle(const char* id, Hasher* hasher) {
    scoped_ptr<ResourceManager> resource_manager(NewResourceManager(hasher));
    rewrite_driver_.SetResourceManager(resource_manager.get());
    rewrite_driver_.AddFiltersByCommaSeparatedList("outline_css");

    std::string style_text = "background_blue { background-color: blue; }\n"
                              "foreground_yellow { color: yellow; }\n";
    std::string outline_text;
    AppendDefaultHeaders(kContentTypeCss, resource_manager.get(),
                         &outline_text);
    outline_text += style_text;

    std::string hash = (hasher == NULL) ? "0" : hasher->Hash(style_text);

    std::string outline_filename;
    std::string ending = StrCat("of.", hash, "._.css");
    filename_encoder_.Encode(file_prefix_, ending, &outline_filename);
    std::string outline_url = StrCat(url_prefix_, "of.", hash, "._.css");

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
  // Use hasher = NULL to use FilenameResources.
  void OutlineScript(const char* id, Hasher* hasher) {
    scoped_ptr<ResourceManager> resource_manager(NewResourceManager(hasher));
    rewrite_driver_.SetResourceManager(resource_manager.get());
    rewrite_driver_.AddFiltersByCommaSeparatedList("outline_javascript");

    std::string script_text = "FOOBAR";
    std::string outline_text;
    AppendDefaultHeaders(kContentTypeJavascript, resource_manager.get(),
                         &outline_text);
    outline_text += script_text;

    std::string hash = (hasher == NULL) ? "0" : hasher->Hash(script_text);
    std::string outline_filename;
    std::string ending = StrCat("of.", hash, "._.js");
    filename_encoder_.Encode(file_prefix_, ending, &outline_filename);
    std::string outline_url = StrCat(url_prefix_, "of.", hash, "._.js");

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

  void CollectCssLinks(const char* name, const StringPiece& html,
                       StringVector* css_links) {
    HtmlParse html_parse(&message_handler_);
    CssCollector collector(&html_parse, css_links);
    html_parse.AddFilter(&collector);
    html_parse.StartParse(name);
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
  void CollectImgSrcs(const char* name, const StringPiece& html,
                       StringVector* img_srcs) {
    HtmlParse html_parse(&message_handler_);
    ImgCollector collector(&html_parse, img_srcs);
    html_parse.AddFilter(&collector);
    html_parse.StartParse(name);
    html_parse.ParseText(html.data(), html.size());
    html_parse.FinishParse();
  }

  virtual HtmlParse* html_parse() { return rewrite_driver_.html_parse(); }

  RewriteDriver nobase_driver_;  // No base url, for testing bare resource get

  DISALLOW_COPY_AND_ASSIGN(RewriterTest);
};

TEST_F(RewriterTest, AddHead) {
  rewrite_driver_.AddHead();
  ValidateExpected("add_head",
      "<body><p>text</p></body>",
      "<head/><body><p>text</p></body>");
}

TEST_F(RewriterTest, MergeHead) {
  rewrite_driver_.AddHead();
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
  rewrite_driver_.AddFiltersByCommaSeparatedList("add_base_tag");
  rewrite_driver_.SetBaseUrl("http://base");
  ValidateExpected("base_tag",
      "<body><p>text</p></body>",
      "<head><base href=\"http://base\"></head><body><p>text</p></body>");
}

TEST_F(RewriterTest, BaseTagExistingHead) {
  rewrite_driver_.AddFiltersByCommaSeparatedList("add_base_tag");
  rewrite_driver_.SetBaseUrl("http://base");
  ValidateExpected("base_tag",
      "<head><meta></head><body><p>text</p></body>",
      "<head><base href=\"http://base\"><meta></head><body><p>text</p></body>");
}

TEST_F(RewriterTest, BaseTagExistingHeadAndNonHrefBase) {
  rewrite_driver_.AddFiltersByCommaSeparatedList("add_base_tag");
  rewrite_driver_.SetBaseUrl("http://base");
  ValidateExpected("base_tag",
      "<head><base x><meta></head><body></body>",
      "<head><base href=\"http://base\"><base x><meta></head><body></body>");
}

TEST_F(RewriterTest, BaseTagExistingHeadAndHrefBase) {
  rewrite_driver_.AddFiltersByCommaSeparatedList("add_base_tag");
  rewrite_driver_.SetBaseUrl("http://base");
  ValidateExpected("base_tag",
      "<head><meta><base href=\"http://old\"></head><body></body>",
      "<head><base href=\"http://base\"><meta></head><body></body>");
}

TEST_F(RewriterTest, CombineCss) {
  CombineCss("combine_no_hash", &mock_hasher_);
}


TEST_F(RewriterTest, CombineCssShards) {
  num_shards_ = 10;
  url_prefix_ = "http://mysite%d/";
  CombineCss("combine_sha1", &mock_hasher_);
}

TEST_F(RewriterTest, CombineCssNoInput) {
  static const char html_input[] =
      "<head>\n"
      "  <link rel='stylesheet' href='a_broken.css' type='text/css'>\n"
      "  <link rel='stylesheet' href='b.css' type='text/css'>\n"
      "</head>\n"
      "<body><div class=\"c1\"><div class=\"c2\"><p>\n"
      "  Yellow on Blue</p></div></div></body>";
  ValidateNoChanges("combine_css_no_input", html_input);
}

// TODO(sligocki): Test that using the DummyUrlFetcher causes FatalError.


// Tests for Outlining styles.
TEST_F(RewriterTest, OutlineStyle) {
  OutlineStyle("outline_styles_no_hash", &mock_hasher_);
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
  rewrite_driver_.AddFiltersByCommaSeparatedList(
      "outline_css,outline_javascript");

  // We need to make sure we don't create this file, so rm any old one
  DeleteFileIfExists(file_prefix + "of.0._.js");

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

TEST_F(RewriterTest, MoveCssToHeadTest) {
  static const char html_input[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css'>"  // no newline
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"  // no indent
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  static const char expected_output[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "<link rel='stylesheet' href='a.css' type='text/css'>"  // no newline
      "<link rel='stylesheet' href='b.css' type='text/css'>"  // no newline
      "<link rel='stylesheet' href='c.css' type='text/css'>"  // no newline
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  \n"
      "  World!\n"
      "  \n"
      "</body>\n";
  CssMoveToHeadFilter move_to_head(rewrite_driver_.html_parse(), NULL);
  rewrite_driver_.html_parse()->AddFilter(&move_to_head);
  ValidateExpected("move_css_to_head", html_input, expected_output);
}

}  // namespace net_instaweb
