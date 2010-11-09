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

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"

namespace net_instaweb {

namespace {

class CssCombineFilterTest : public ResourceManagerTestBase {
 protected:
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
};

TEST_F(CssCombineFilterTest, CombineCss) {
  CombineCss("combine_css_no_hash", &mock_hasher_, "", false);
}

TEST_F(CssCombineFilterTest, CombineCssMD5) {
  CombineCss("combine_css_md5", &md5_hasher_, "", false);
}


TEST_F(CssCombineFilterTest, CombineCssWithIEDirective) {
  const char ie_directive_barrier[] =
      "<!--[if IE]>\n"
      "<link rel=\"stylesheet\" type=\"text/css\" "
      "href=\"http://graphics8.nytimes.com/css/"
      "0.1/screen/build/homepage/ie.css\">\n"
      "<![endif]-->";
  CombineCss("combine_css_ie", &md5_hasher_, ie_directive_barrier, true);
}

TEST_F(CssCombineFilterTest, CombineCssWithStyle) {
  const char style_barrier[] = "<style>a { color: red }</style>\n";
  CombineCss("combine_css_style", &md5_hasher_, style_barrier, true);
}

TEST_F(CssCombineFilterTest, CombineCssWithBogusLink) {
  const char bogus_barrier[] = "<link rel='stylesheet' type='text/css' "
      "href='crazee://big/blue/fake'>\n";
  CombineCss("combine_css_bogus_link", &md5_hasher_, bogus_barrier, true);
}

TEST_F(CssCombineFilterTest, CombineCssWithImport) {
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

TEST_F(CssCombineFilterTest, CombineCssWithNoscriptBarrier) {
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

TEST_F(CssCombineFilterTest, CombineCssWithFakeNoscriptBarrier) {
  const char non_barrier[] =
      "<noscript>\n"
      "  <p>You have no scripts installed</p>\n"
      "</noscript>\n";
  CombineCss("combine_css_fake_noscript", &md5_hasher_, non_barrier, false);
}

TEST_F(CssCombineFilterTest, CombineCssWithMediaBarrier) {
  const char media_barrier[] =
      "<link rel='stylesheet' type='text/css' href='d.css' media='print'>\n";

  const char d_css_url[] = "http://combine_css.test/d.css";
  const char d_css_body[] = ".c4 {\n color: green;\n}\n";
  SimpleMetaData default_css_header;
  resource_manager_->SetDefaultHeaders(&kContentTypeCss, &default_css_header);
  mock_url_fetcher_.SetResponse(d_css_url, default_css_header, d_css_body);

  CombineCss("combine_css_media", &md5_hasher_, media_barrier, true);
}

TEST_F(CssCombineFilterTest, CombineCssWithNonMediaBarrier) {
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

TEST_F(CssCombineFilterTest, CombineCssBaseUrl) {
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

TEST_F(CssCombineFilterTest, CombineCssShards) {
  num_shards_ = 10;
  url_prefix_ = "http://mysite%d/";
  CombineCss("combine_css_sha1", &mock_hasher_, "", false);
}

TEST_F(CssCombineFilterTest, CombineCssNoInput) {
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

TEST_F(CssCombineFilterTest, CombineCssMissingResource) {
  CssCombineMissingResource();
}

}  // namespace

}  // namespace net_instaweb
