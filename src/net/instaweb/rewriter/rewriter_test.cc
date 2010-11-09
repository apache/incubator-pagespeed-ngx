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

// Author: jmarantz@google.com (Joshua Marantz)
//     and sligocki@google.com (Shawn Ligocki)

// Unit-test the html rewriter

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/rewriter/public/css_move_to_head_filter.h"
#include "net/instaweb/rewriter/public/css_outline_filter.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/js_outline_filter.h"
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
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

namespace net_instaweb {

class RewriterTest : public ResourceManagerTestBase {
 protected:
  RewriterTest() {}

  // Test spriting CSS with options to write headers and use a hasher.
  void CombineCss(const StringPiece& id, Hasher* hasher,
                  const char* barrier_text, bool is_barrier) {
    resource_manager_->set_hasher(hasher);
    other_resource_manager_.set_hasher(hasher);

    rewrite_driver_.AddFilter(RewriteOptions::kCombineCss);
    other_rewrite_driver_.AddFilter(RewriteOptions::kCombineCss);

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
    resource_manager_->SetDefaultHeaders(&kContentTypeCss, &default_css_header);
    mock_url_fetcher_.SetResponse(a_css_url, default_css_header, a_css_body);
    mock_url_fetcher_.SetResponse(b_css_url, default_css_header, b_css_body);
    mock_url_fetcher_.SetResponse(c_css_url, default_css_header, c_css_body);

    ParseUrl(html_url, html_input);

    std::string headers;
    AppendDefaultHeaders(kContentTypeCss, resource_manager_, &headers);

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

    // Now try to fetch from another server (other_rewrite_driver_) that
    // does not already have the combination cached.
    // TODO(sligocki): This has too much shared state with the first server.
    // See RewriteImage for details.
    SimpleMetaData other_response_headers;
    fetched_resource_content.clear();
    message_handler_.Message(kInfo, "Now with serving.");
    file_system_.Enable();
    dummy_callback.Reset();
    other_rewrite_driver_.FetchResource(combine_url, request_headers,
                                        &other_response_headers, &writer,
                                        &message_handler_, &dummy_callback);
    EXPECT_EQ(HttpStatus::kOK, other_response_headers.status_code());
    EXPECT_EQ(expected_combination, fetched_resource_content);
    ServeResourceFromNewContext(combine_url, combine_filename,
                                fetched_resource_content,
                                RewriteOptions::kCombineCss, path.c_str());
  }

  // Test what happens when CSS combine can't find a previously-rewritten
  // resource during a subsequent resource fetch.  This used to segfault.
  void CssCombineMissingResource() {
    const char a_css_url[] = "http://combine_css.test/a.css";
    const char c_css_url[] = "http://combine_css.test/c.css";

    const char a_css_body[] = ".c1 {\n background-color: blue;\n}\n";
    const char c_css_body[] = ".c3 {\n font-weight: bold;\n}\n";
    std::string expected_combination = StrCat(a_css_body, c_css_body);

    rewrite_driver_.AddFilter(RewriteOptions::kCombineCss);

    // Put original CSS files into our fetcher.
    SimpleMetaData default_css_header;
    resource_manager_->SetDefaultHeaders(&kContentTypeCss, &default_css_header);
    mock_url_fetcher_.SetResponse(a_css_url, default_css_header, a_css_body);
    mock_url_fetcher_.SetResponse(c_css_url, default_css_header, c_css_body);

    // First make sure we can serve the combination of a & c.  This is to avoid
    // spurious test successes.

    const char kACUrl[] = "http://combine_css.test/cc.0.a,s+c,s.css";
    const char kABCUrl[] = "http://combine_css.test/cc.0.a,s+bbb,s+c,s.css";
    SimpleMetaData request_headers, response_headers;
    std::string fetched_resource_content;
    StringWriter writer(&fetched_resource_content);
    DummyCallback dummy_callback;

    // NOTE: This first fetch used to return status 0 because response_headers
    // weren't initialized by the first resource fetch (but were cached
    // correctly).  Content was correct.
    EXPECT_TRUE(
        rewrite_driver_.FetchResource(kACUrl, request_headers,
                                      &response_headers, &writer,
                                      &message_handler_, &dummy_callback));
    EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());
    EXPECT_EQ(expected_combination, fetched_resource_content);

    // We repeat the fetch to prove that it succeeds from cache:
    fetched_resource_content.clear();
    dummy_callback.Reset();
    response_headers.Clear();
    EXPECT_TRUE(
        rewrite_driver_.FetchResource(kACUrl, request_headers,
                                      &response_headers, &writer,
                                      &message_handler_, &dummy_callback));
    EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());
    EXPECT_EQ(expected_combination, fetched_resource_content);

    // Now let's try fetching the url that references a missing resource
    // (bbb.css) in addition to the two that do exist, a.css and c.css.  Using
    // an entirely non-existent resource appears to test a strict superset of
    // filter code paths when compared with returning a 404 for the resource.
    mock_url_fetcher_.set_fail_on_unexpected(false);
    FailCallback fail_callback;
    fetched_resource_content.clear();
    response_headers.Clear();
    EXPECT_TRUE(
        rewrite_driver_.FetchResource(kABCUrl, request_headers,
                                      &response_headers, &writer,
                                      &message_handler_, &fail_callback));
    EXPECT_EQ(HttpStatus::kNotFound, response_headers.status_code());
    EXPECT_EQ("", fetched_resource_content);
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

  void TestInlineCss(const std::string& html_url,
                     const std::string& css_url,
                     const std::string& other_attrs,
                     const std::string& css_original_body,
                     bool expect_inline,
                     const std::string& css_rewritten_body) {
    rewrite_driver_.AddFilter(RewriteOptions::kInlineCss);

    const std::string html_input =
        "<head>\n"
        "  <link rel=\"stylesheet\" href=\"" + css_url + "\"" +
        (other_attrs.empty() ? "" : " " + other_attrs) + ">\n"
        "</head>\n"
        "<body>Hello, world!</body>\n";

    // Put original CSS file into our fetcher.
    SimpleMetaData default_css_header;
    resource_manager_->SetDefaultHeaders(&kContentTypeCss,
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
    rewrite_driver_.AddFilter(RewriteOptions::kInlineJavascript);

    const std::string html_input =
        "<head>\n"
        "  <script src=\"" + js_url + "\">" + js_inline_body + "</script>\n"
        "</head>\n"
        "<body>Hello, world!</body>\n";

    // Put original Javascript file into our fetcher.
    SimpleMetaData default_js_header;
    resource_manager_->SetDefaultHeaders(&kContentTypeJavascript,
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
    resource_manager_->set_hasher(hasher);

    RewriteOptions options;
    options.EnableFilter(RewriteOptions::kOutlineCss);
    options.set_css_outline_min_bytes(0);
    rewrite_driver_.AddFilters(options);

    std::string style_text = "background_blue { background-color: blue; }\n"
                              "foreground_yellow { color: yellow; }\n";
    std::string outline_text;
    AppendDefaultHeaders(kContentTypeCss, resource_manager_,
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
    resource_manager_->set_hasher(hasher);

    RewriteOptions options;
    options.EnableFilter(RewriteOptions::kOutlineJavascript);
    options.set_js_outline_min_bytes(0);
    rewrite_driver_.AddFilters(options);

    std::string script_text = "FOOBAR";
    std::string outline_text;
    AppendDefaultHeaders(kContentTypeJavascript, resource_manager_,
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

  virtual HtmlParse* html_parse() { return rewrite_driver_.html_parse(); }

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

TEST_F(RewriterTest, CombineCssWithBogusLink) {
  const char bogus_barrier[] = "<link rel='stylesheet' type='text/css' "
      "href='crazee://big/blue/fake'>\n";
  CombineCss("combine_css_bogus_link", &md5_hasher_, bogus_barrier, true);
}

TEST_F(RewriterTest, CombineCssWithImport) {
  const char import_barrier[] =
      "<link rel='stylesheet' type='text/css' href='d.css'>\n";

  const char d_css_url[] = "http://combine_css.test/d.css";
  // Note: We cannot combine a CSS file with @import.
  const char d_css_body[] = "@import \"c.css\";\n.c4 { color: green; }\n";
  SimpleMetaData default_css_header;
  resource_manager_->SetDefaultHeaders(&kContentTypeCss, &default_css_header);
  mock_url_fetcher_.SetResponse(d_css_url, default_css_header, d_css_body);

  CombineCss("combine_css_import", &md5_hasher_, import_barrier, true);
}

TEST_F(RewriterTest, CombineCssWithNoscriptBarrier) {
  const char noscript_barrier[] =
      "<noscript>\n"
      "  <link rel='stylesheet' type='text/css' href='d.css'>\n"
      "</noscript>\n";

  // Put this in the Test class to remove repetition here and below.
  const char d_css_url[] = "http://combine_css.test/d.css";
  const char d_css_body[] = ".c4 {\n color: green;\n}\n";
  SimpleMetaData default_css_header;
  resource_manager_->SetDefaultHeaders(&kContentTypeCss, &default_css_header);
  mock_url_fetcher_.SetResponse(d_css_url, default_css_header, d_css_body);

  CombineCss("combine_css_noscript", &md5_hasher_, noscript_barrier, true);
}

TEST_F(RewriterTest, CombineCssWithFakeNoscriptBarrier) {
  const char non_barrier[] =
      "<noscript>\n"
      "  <p>You have no scripts installed</p>\n"
      "</noscript>\n";
  CombineCss("combine_css_fake_noscript", &md5_hasher_, non_barrier, false);
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

TEST_F(RewriterTest, CombineCssBaseUrl) {
  // Put original CSS files into our fetcher.
  const char html_url[] = "http://combine_css.test/base_url.html";
  // TODO(sligocki): This is broken, but much less so than before. Now we are
  // just failing to keep track of the old base URL long enough.
  //const char a_css_url[] = "http://combine_css.test/a.css";
  const char a_css_url[] = "http://other_domain.test/foo/a.css";
  const char b_css_url[] = "http://other_domain.test/foo/b.css";

  const char a_css_body[] = ".c1 {\n background-color: blue;\n}\n";
  const char b_css_body[] = ".c2 {\n color: yellow;\n}\n";

  SimpleMetaData default_css_header;
  resource_manager_->SetDefaultHeaders(&kContentTypeCss, &default_css_header);
  mock_url_fetcher_.SetResponse(a_css_url, default_css_header, a_css_body);
  mock_url_fetcher_.SetResponse(b_css_url, default_css_header, b_css_body);

  // Second stylesheet is on other domain.
  const char html_input[] =
      "<head>\n"
      "  <link rel='stylesheet' type='text/css' href='a.css'>\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "  <link rel='stylesheet' type='text/css' href='b.css'>\n"
      "</head>\n";

  // Rewrite
  rewrite_driver_.AddFilter(RewriteOptions::kCombineCss);
  ParseUrl(html_url, html_input);

  // Check for CSS files in the rewritten page.
  StringVector css_urls;
  CollectCssLinks("combine_css_no_media-links", output_buffer_, &css_urls);
  EXPECT_EQ(1UL, css_urls.size());
  const std::string& combine_url = css_urls[0];

  const char expected_output_format[] =
      "<head>\n"
      "  <link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "  \n"
      "</head>\n";
  std::string expected_output = StringPrintf(expected_output_format,
                                              combine_url.c_str());

  EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);

  EXPECT_TRUE(GURL(combine_url.c_str()).is_valid());
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

TEST_F(RewriterTest, CombineCssMissingResource) {
  CssCombineMissingResource();
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

  // TODO(sligocki): Maybe test with other hashers.
  //resource_manager_->set_hasher(hasher);

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

TEST_F(RewriterTest, DataUrlTest) {
  DataUrlResource();
}

// TODO(jmarantz): add CacheExtender test.  I want to refactor CombineCss,
// RewriteImage, and OutlineStyle first if I can cause there should be more
// sharing.

}  // namespace net_instaweb
