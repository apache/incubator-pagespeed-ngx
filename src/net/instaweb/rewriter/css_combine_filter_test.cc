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
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/url_multipart_encoder.h"

namespace net_instaweb {

namespace {

const char kDomain[] = "http://combine_css.test/";
const char kYellow[] = ".yellow {background-color: yellow;}";
const char kBlue[] = ".blue {color: blue;}\n";

class CssCombineFilterTest : public ResourceManagerTestBase {
 protected:
  // Test spriting CSS with options to write headers and use a hasher.
  void CombineCss(const StringPiece& id, Hasher* hasher,
                  const char* barrier_text, bool is_barrier) {
    CombineCssWithNames(id, hasher, barrier_text, is_barrier, "a.css", "b.css");
  }

  void CombineCssWithNames(const StringPiece& id, Hasher* hasher,
                           const char* barrier_text, bool is_barrier,
                           const char* a_css_name,
                           const char* b_css_name) {
    resource_manager_->set_hasher(hasher);
    other_resource_manager_.set_hasher(hasher);

    AddFilter(RewriteOptions::kCombineCss);
    AddOtherFilter(RewriteOptions::kCombineCss);

    // URLs and content for HTML document and resources.
    CHECK_EQ(StringPiece::npos, id.find("/"));
    std::string html_url = StrCat(kDomain, id, ".html");
    std::string a_css_url = StrCat(kDomain, a_css_name);
    std::string b_css_url = StrCat(kDomain, b_css_name);
    std::string c_css_url = StrCat(kDomain, "c.css");

    static const char html_input_format[] =
        "<head>\n"
        "  <link rel='stylesheet' href='%s' type='text/css'>\n"
        "  <link rel='stylesheet' href='%s' type='text/css'>\n"
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
    std::string html_input =
        StringPrintf(html_input_format, a_css_name, b_css_name, barrier_text);
    const char a_css_body[] = ".c1 {\n background-color: blue;\n}\n";
    const char b_css_body[] = ".c2 {\n color: yellow;\n}\n";
    const char c_css_body[] = ".c3 {\n font-weight: bold;\n}\n";

    // Put original CSS files into our fetcher.
    ResponseHeaders default_css_header;
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
    GoogleUrl gurl(combine_url);

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
    RequestHeaders request_headers;
    ResponseHeaders response_headers;
    std::string fetched_resource_content;
    StringWriter writer(&fetched_resource_content);
    DummyCallback dummy_callback(true);
    rewrite_driver_.FetchResource(combine_url, request_headers,
                                  &response_headers, &writer,
                                  &dummy_callback);
    EXPECT_EQ(HttpStatus::kOK, response_headers.status_code()) << combine_url;
    EXPECT_EQ(expected_combination, fetched_resource_content);

    // Now try to fetch from another server (other_rewrite_driver_) that
    // does not already have the combination cached.
    // TODO(sligocki): This has too much shared state with the first server.
    // See RewriteImage for details.
    ResponseHeaders other_response_headers;
    fetched_resource_content.clear();
    message_handler_.Message(kInfo, "Now with serving.");
    file_system_.Enable();
    dummy_callback.Reset();
    other_rewrite_driver_.FetchResource(combine_url, request_headers,
                                        &other_response_headers, &writer,
                                        &dummy_callback);
    EXPECT_EQ(HttpStatus::kOK, other_response_headers.status_code());
    EXPECT_EQ(expected_combination, fetched_resource_content);

    // Try to fetch from an independent server.
    ServeResourceFromManyContexts(combine_url, RewriteOptions::kCombineCss,
                                  hasher, fetched_resource_content);
  }

  // Test what happens when CSS combine can't find a previously-rewritten
  // resource during a subsequent resource fetch.  This used to segfault.
  void CssCombineMissingResource() {
    std::string a_css_url = StrCat(kDomain, "a.css");
    std::string c_css_url = StrCat(kDomain, "c.css");

    const char a_css_body[] = ".c1 {\n background-color: blue;\n}\n";
    const char c_css_body[] = ".c3 {\n font-weight: bold;\n}\n";
    std::string expected_combination = StrCat(a_css_body, c_css_body);

    AddFilter(RewriteOptions::kCombineCss);

    // Put original CSS files into our fetcher.
    ResponseHeaders default_css_header;
    resource_manager_->SetDefaultHeaders(&kContentTypeCss, &default_css_header);
    mock_url_fetcher_.SetResponse(a_css_url, default_css_header, a_css_body);
    mock_url_fetcher_.SetResponse(c_css_url, default_css_header, c_css_body);

    // First make sure we can serve the combination of a & c.  This is to avoid
    // spurious test successes.

    std::string kACUrl = Encode(kDomain, "cc", "0", "a.css+c.css", "css");
    std::string kABCUrl = Encode(kDomain, "cc", "0", "a.css+bbb.css+c.css",
                                  "css");
    RequestHeaders request_headers;
    ResponseHeaders response_headers;
    std::string fetched_resource_content;
    StringWriter writer(&fetched_resource_content);
    DummyCallback dummy_callback(true);

    // NOTE: This first fetch used to return status 0 because response_headers
    // weren't initialized by the first resource fetch (but were cached
    // correctly).  Content was correct.
    EXPECT_TRUE(
        rewrite_driver_.FetchResource(kACUrl, request_headers,
                                      &response_headers, &writer,
                                      &dummy_callback));
    EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());
    EXPECT_EQ(expected_combination, fetched_resource_content);

    // We repeat the fetch to prove that it succeeds from cache:
    fetched_resource_content.clear();
    dummy_callback.Reset();
    response_headers.Clear();
    EXPECT_TRUE(
        rewrite_driver_.FetchResource(kACUrl, request_headers,
                                      &response_headers, &writer,
                                      &dummy_callback));
    EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());
    EXPECT_EQ(expected_combination, fetched_resource_content);

    // Now let's try fetching the url that references a missing resource
    // (bbb.css) in addition to the two that do exist, a.css and c.css.  Using
    // an entirely non-existent resource appears to test a strict superset of
    // filter code paths when compared with returning a 404 for the resource.
    mock_url_fetcher_.set_fail_on_unexpected(false);
    DummyCallback fail_callback(false);
    fetched_resource_content.clear();
    response_headers.Clear();
    EXPECT_TRUE(
        rewrite_driver_.FetchResource(kABCUrl, request_headers,
                                      &response_headers, &writer,
                                      &fail_callback));
    // What status we get here depends a lot on details of when exactly
    // we detect the failure. If done early enough, nothing will be set.
    // This test may change, but see also
    // ResourceCombinerTest.TestContinuingFetchWhenFastFailed
    EXPECT_FALSE(response_headers.headers_complete());
    EXPECT_EQ("", fetched_resource_content);
  }

  // Representation for a CSS tag.
  class CssLink {
   public:
    CssLink(const StringPiece& url, const StringPiece& content,
            const StringPiece& media, bool supply_mock)
        : url_(url.data(), url.size()),
          content_(content.data(), content.size()),
          media_(media.data(), media.size()),
          supply_mock_(supply_mock) {
    }

    // A vector of CssLink* should know how to accumulate and add.
    class Vector : public std::vector<CssLink*> {
     public:
      ~Vector() {
        STLDeleteElements(this);
      }

      void Add(const StringPiece& url, const StringPiece& content,
               const StringPiece& media, bool supply_mock) {
        push_back(new CssLink(url, content, media, supply_mock));
      }
    };

    // Parses a combined CSS elementand provides the segments from which
    // it came.
    bool DecomposeCombinedUrl(std::string* base, StringVector* segments,
                              MessageHandler* handler) {
      GoogleUrl gurl(url_);
      bool ret = false;
      if (gurl.is_valid()) {
        gurl.AllExceptLeaf().CopyToString(base);
        ResourceNamer namer;
        if (namer.Decode(gurl.LeafWithQuery()) &&
            (namer.id() == RewriteDriver::kCssCombinerId)) {
          UrlMultipartEncoder multipart_encoder;
          std::string segment;
          ret = multipart_encoder.Decode(namer.name(), segments, NULL, handler);
        }
      }
      return ret;
    }

    std::string url_;
    std::string content_;
    std::string media_;
    bool supply_mock_;
  };

  // Helper class to collect CSS hrefs.
  class CssCollector : public EmptyHtmlFilter {
   public:
    CssCollector(HtmlParse* html_parse, CssLink::Vector* css_links)
        : css_links_(css_links),
          css_tag_scanner_(html_parse) {
    }

    virtual void EndElement(HtmlElement* element) {
      HtmlElement::Attribute* href;
      const char* media;
      if (css_tag_scanner_.ParseCssElement(element, &href, &media)) {
        // TODO(jmarantz): collect content of the CSS files, before and
        // after combination, so we can diff.
        const char* content = "";
        css_links_->Add(href->value(), content, media, false);
      }
    }

    virtual const char* Name() const { return "CssCollector"; }

   private:
    CssLink::Vector* css_links_;
    CssTagScanner css_tag_scanner_;

    DISALLOW_COPY_AND_ASSIGN(CssCollector);
  };

  // Collects just the hrefs from CSS links into a string vector.
  void CollectCssLinks(const StringPiece& id, const StringPiece& html,
                       StringVector* css_links) {
    CssLink::Vector v;
    CollectCssLinks(id, html, &v);
    for (int i = 0, n = v.size(); i < n; ++i) {
      css_links->push_back(v[i]->url_);
    }
  }

  // Collects all information about CSS links into a CssLink::Vector.
  void CollectCssLinks(const StringPiece& id, const StringPiece& html,
                       CssLink::Vector* css_links) {
    HtmlParse html_parse(&message_handler_);
    CssCollector collector(&html_parse, css_links);
    html_parse.AddFilter(&collector);
    std::string dummy_url = StrCat("http://collect.css.links/", id, ".html");
    html_parse.StartParse(dummy_url);
    html_parse.ParseText(html.data(), html.size());
    html_parse.FinishParse();
  }

  // Common framework for testing barriers.  A null-terminated set of css
  // names is specified, with optional media tags.  E.g.
  //   static const char* link[] {
  //     "a.css",
  //     "styles/b.css",
  //     "print.css media=print",
  //   }
  //
  // The output of this function is the collected CSS links after rewrite.
  void BarrierTestHelper(
      const StringPiece& id,
      const CssLink::Vector& input_css_links,
      CssLink::Vector* output_css_links) {
    // TODO(sligocki): Allow other domains (this is constrained right now b/c
    // of InitResponseHeaders.
    std::string html_url = StrCat(kTestDomain, id, ".html");
    std::string html_input("<head>\n");
    for (int i = 0, n = input_css_links.size(); i < n; ++i) {
      const CssLink* link = input_css_links[i];
      if (!link->url_.empty()) {
        if (link->supply_mock_) {
          // If the css-vector contains a 'true' for this, then we supply the
          // mock fetcher with headers and content for the CSS file.
          InitResponseHeaders(link->url_, kContentTypeCss, link->content_, 600);
        }
        std::string style("  <");
        style += StringPrintf("link rel='stylesheet' type='text/css' href='%s'",
                              link->url_.c_str());
        if (!link->media_.empty()) {
          style += StrCat(" media='", link->media_, "'");
        }
        style += ">\n";
        html_input += style;
      } else {
        html_input += link->content_;
      }
    }
    html_input += "</head>\n<body>\n  <div class='yellow'>\n";
    html_input += "    Hello, mod_pagespeed!\n  </div>\n</body>\n";

    AddFilter(RewriteOptions::kCombineCss);
    ParseUrl(html_url, html_input);
    CollectCssLinks("combine_css_missing_files", output_buffer_,
                    output_css_links);

    // TODO(jmarantz): fetch all content and provide output as text.
  }

  // Helper for testing handling of URLs with trailing junk
  void TestCorruptUrl(const char* junk, bool should_fetch_ok) {
    CssLink::Vector css_in, css_out;
    css_in.Add("1.css", kYellow, "", true);
    css_in.Add("2.css", kYellow, "", true);
    BarrierTestHelper("no_ext_corrupt", css_in, &css_out);
    ASSERT_EQ(1, css_out.size());
    std::string normal_url = css_out[0]->url_;

    std::string out;
    EXPECT_EQ(should_fetch_ok,
              ServeResourceUrl(StrCat(normal_url, junk),  &out));

    // Now re-do it and make sure %22 didn't get stuck in the URL
    STLDeleteElements(&css_out);
    css_out.clear();
    BarrierTestHelper("no_ext_corrupt", css_in, &css_out);
    ASSERT_EQ(1, css_out.size());
    EXPECT_EQ(css_out[0]->url_, normal_url);
  }

  // Test to make sure we don't miscombine things when handling the input
  // as XHTML producing non-flat <link>'s from the parser
  void TestXhtml(bool flush) {
    const char kXhtmlDtd[] =
        "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" "
        "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">";

    AddFilter(RewriteOptions::kCombineCss);
    std::string a_css_url = StrCat(kTestDomain, "a.css");
    std::string b_css_url = StrCat(kTestDomain, "b.css");

    ResponseHeaders default_css_header;
    resource_manager_->SetDefaultHeaders(&kContentTypeCss, &default_css_header);
    mock_url_fetcher_.SetResponse(a_css_url, default_css_header, "A");
    mock_url_fetcher_.SetResponse(b_css_url, default_css_header, "B");

    std::string combined_url =
        StrCat(kTestDomain, "a.css+b.css.pagespeed.cc.0.css");

    SetupWriter();
    rewrite_driver_.StartParse(kTestDomain);
    std::string input_beginning =
        StrCat(kXhtmlDtd, "<div><link rel=stylesheet href=a.css>",
               "<link rel=stylesheet href=b.css>");
    rewrite_driver_.ParseText(input_beginning);

    if (flush) {
      // This is a regression test: previously getting a flush here would
      // cause attempts to modify data structures, as we would only
      // start seeing the links at the </div>
      rewrite_driver_.Flush();
    }
    rewrite_driver_.ParseText("</div>");
    rewrite_driver_.FinishParse();

    if (!flush) {
      // Note: if this test begins failing because the <link> in the output is
      // no  longer  a <link/> it will need to be revised to make sure it's
      // still parsed as XHTML.
      EXPECT_EQ(
          StrCat(kXhtmlDtd, "<div>",
                "<link rel=\"stylesheet\" type=\"text/css\" href=\"",
                combined_url, "\"/></div>"),
          output_buffer_);
    } else {
      // In the case of the flush we cannot actually rewrite this, as the
      // <links> are still open and hence cannot be combined.
      EXPECT_EQ(StrCat(input_beginning, "</div>"), output_buffer_);
    }
  }

  std::string CombineWithBaseTag(const char* html_input) {
    // Put original CSS files into our fetcher.
    std::string html_url = StrCat(kDomain, "base_url.html");
    const char a_css_url[] = "http://other_domain.test/foo/a.css";
    const char b_css_url[] = "http://other_domain.test/foo/b.css";

    const char a_css_body[] = ".c1 {\n background-color: blue;\n}\n";
    const char b_css_body[] = ".c2 {\n color: yellow;\n}\n";

    ResponseHeaders default_css_header;
    resource_manager_->SetDefaultHeaders(&kContentTypeCss, &default_css_header);
    mock_url_fetcher_.SetResponse(a_css_url, default_css_header, a_css_body);
    mock_url_fetcher_.SetResponse(b_css_url, default_css_header, b_css_body);

    // Rewrite
    AddFilter(RewriteOptions::kCombineCss);
    ParseUrl(html_url, html_input);

    // Check for CSS files in the rewritten page.
    StringVector css_urls;
    CollectCssLinks("combine_css_no_media-links", output_buffer_, &css_urls);
    EXPECT_EQ(1UL, css_urls.size());
    return css_urls[0];
  }
};

TEST_F(CssCombineFilterTest, CombineCss) {
  CombineCss("combine_css_no_hash", &mock_hasher_, "", false);
}

TEST_F(CssCombineFilterTest, CombineCssMD5) {
  CombineCss("combine_css_md5", &md5_hasher_, "", false);
}

// Make sure that if we re-parse the same html twice we do not
// end up recomputing the CSS (and writing to cache) again
TEST_F(CssCombineFilterTest, CombineCssRecombine) {
  CombineCss("combine_css_recombine", &md5_hasher_, "", false);
  int inserts_before = lru_cache_->num_inserts();

  CombineCss("combine_css_recombine", &md5_hasher_, "", false);
  int inserts_after = lru_cache_->num_inserts();
  EXPECT_EQ(inserts_before, inserts_after);
  EXPECT_EQ(0, lru_cache_->num_identical_reinserts());
}


// http://code.google.com/p/modpagespeed/issues/detail?q=css&id=39
TEST_F(CssCombineFilterTest, DealWithParams) {
  CombineCssWithNames("deal_with_params", &mock_hasher_, "", false,
                      "a.css?U", "b.css?rev=138");
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

TEST_F(CssCombineFilterTest, CombineCssWithImportInFirst) {
  CssLink::Vector css_in, css_out;
  css_in.Add("1.css", "@Import \"1a.css\"", "", true);
  css_in.Add("2.css", kYellow, "", true);
  css_in.Add("3.css", kYellow, "", true);
  BarrierTestHelper("combine_css_with_import1", css_in, &css_out);
  EXPECT_EQ(1, css_out.size());
}

TEST_F(CssCombineFilterTest, CombineCssWithImportInSecond) {
  CssLink::Vector css_in, css_out;
  css_in.Add("1.css", kYellow, "", true);
  css_in.Add("2.css", "@Import \"2a.css\"", "", true);
  css_in.Add("3.css", kYellow, "", true);
  BarrierTestHelper("combine_css_with_import1", css_in, &css_out);
  EXPECT_EQ("1.css", css_out[0]->url_);
  EXPECT_EQ(2, css_out.size());
}

TEST_F(CssCombineFilterTest, CombineCssWithNoscriptBarrier) {
  const char noscript_barrier[] =
      "<noscript>\n"
      "  <link rel='stylesheet' type='text/css' href='d.css'>\n"
      "</noscript>\n";

  // Put this in the Test class to remove repetition here and below.
  std::string d_css_url = StrCat(kDomain, "d.css");
  const char d_css_body[] = ".c4 {\n color: green;\n}\n";
  ResponseHeaders default_css_header;
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

  std::string d_css_url = StrCat(kDomain, "d.css");
  const char d_css_body[] = ".c4 {\n color: green;\n}\n";
  ResponseHeaders default_css_header;
  resource_manager_->SetDefaultHeaders(&kContentTypeCss, &default_css_header);
  mock_url_fetcher_.SetResponse(d_css_url, default_css_header, d_css_body);

  CombineCss("combine_css_media", &md5_hasher_, media_barrier, true);
}

TEST_F(CssCombineFilterTest, CombineCssWithNonMediaBarrier) {
  // Put original CSS files into our fetcher.
  std::string html_url = StrCat(kDomain, "no_media_barrier.html");
  std::string a_css_url = StrCat(kDomain, "a.css");
  std::string b_css_url = StrCat(kDomain, "b.css");
  std::string c_css_url = StrCat(kDomain, "c.css");
  std::string d_css_url = StrCat(kDomain, "d.css");

  const char a_css_body[] = ".c1 {\n background-color: blue;\n}\n";
  const char b_css_body[] = ".c2 {\n color: yellow;\n}\n";
  const char c_css_body[] = ".c3 {\n font-weight: bold;\n}\n";
  const char d_css_body[] = ".c4 {\n color: green;\n}\n";

  ResponseHeaders default_css_header;
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
  AddFilter(RewriteOptions::kCombineCss);
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

// This test, as rewritten as of Feb 2011, is testing an invalid HTML construct,
// where no hrefs should precede a base tag.  This test has been altered to
// reflect our actual behavior, which is that we effectively look ahead
// at the base-tag and let it apply to URLs that show up earlier in the doc.
//
// This is not obviously the most intuitive response to invalid HTML but it's
// what naturally occurs in our implementation, so we are noting that with this
// test.
TEST_F(CssCombineFilterTest, CombineCssBaseUrlOutOfOrder) {
  // Here, the base-tag between two links is basically treated as if it were
  // before the first link.  In fact this construct is semantically invalid
  // HTML.  See http://www.whatwg.org/specs/web-apps/current-work/multipage
  // /semantics.html#the-base-element
  std::string combine_url = CombineWithBaseTag(
      "<head>\n"
      "  <link rel='stylesheet' type='text/css' href='a.css'>\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "  <link rel='stylesheet' type='text/css' href='b.css'>\n"
      "</head>\n");
  const char expected_output_format[] =
      "<head>\n"
      "  <link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "  \n"
      "</head>\n";
  std::string expected_output = StringPrintf(expected_output_format,
                                              combine_url.c_str());
  EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);
  EXPECT_EQ("http://other_domain.test/foo/a.css+b.css.pagespeed.cc.0.css",
            combine_url);
  EXPECT_TRUE(GoogleUrl(combine_url.c_str()).is_valid());
}

// Here's the same test, legalized to have the base url before the first
// link.  It doesn't change the result.  The only difference between this
// test and the above
TEST_F(CssCombineFilterTest, CombineCssBaseUrlCorrectlyOrdered) {
  // <base> tag correctly precedes any urls.
  std::string combine_url = CombineWithBaseTag(
      "<head>\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "  <link rel='stylesheet' type='text/css' href='a.css'>\n"
      "  <link rel='stylesheet' type='text/css' href='b.css'>\n"
      "</head>\n");

  const char expected_output_format[] =
      "<head>\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "  <link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">\n"
      "  \n"
      "</head>\n";
  std::string expected_output = StringPrintf(expected_output_format,
                                              combine_url.c_str());
  EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);
  EXPECT_EQ("http://other_domain.test/foo/a.css+b.css.pagespeed.cc.0.css",
            combine_url);
  EXPECT_TRUE(GoogleUrl(combine_url.c_str()).is_valid());
}

// TODO(jmaessen): Re-write for new sharding story when it exists.
// TEST_F(CssCombineFilterTest, CombineCssShards) {
//   num_shards_ = 10;
//   url_prefix_ = "http://mysite%d/";
//   CombineCss("combine_css_sha1", &mock_hasher_, "", false);
// }

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

TEST_F(CssCombineFilterTest, CombineCssXhtml) {
  TestXhtml(false);
}

TEST_F(CssCombineFilterTest, CombineCssXhtmlWithFlush) {
  TestXhtml(true);
}

TEST_F(CssCombineFilterTest, CombineCssMissingResource) {
  CssCombineMissingResource();
}

TEST_F(CssCombineFilterTest, CombineCssManyFiles) {
  // Prepare an HTML fragment with too many CSS files to combine,
  // exceeding the char limit.
  //
  // It looks like we can fit a limited number of encodings of
  // "yellow%d.css" in the buffer.  It might be more general to base
  // this on the constant declared in RewriteOptions but I think it's
  // easier to understand leaving these exposed as constants; we can
  // abstract them later.
  const int kNumCssLinks = 100;
  // Note: Without CssCombine::Partnership::kUrlSlack this was:
  //const int kNumCssInCombination = 18
  const int kNumCssInCombination = 70;  // based on how we encode "yellow%d.css"
  CssLink::Vector css_in, css_out;
  for (int i = 0; i < kNumCssLinks; ++i) {
    css_in.Add(StringPrintf("styles/yellow%d.css", i),
               kYellow, "", true);
  }
  BarrierTestHelper("combine_css_many_files", css_in, &css_out);
  ASSERT_EQ(2, css_out.size());

  // Check that the first element is really a combination.
  std::string base;
  StringVector segments;
  ASSERT_TRUE(css_out[0]->DecomposeCombinedUrl(&base, &segments,
                                               &message_handler_));
  EXPECT_EQ(StrCat(kTestDomain, "styles/"), base);
  EXPECT_EQ(kNumCssInCombination, segments.size());

  segments.clear();
  ASSERT_TRUE(css_out[1]->DecomposeCombinedUrl(&base, &segments,
                                               &message_handler_));
  EXPECT_EQ(StrCat(kTestDomain, "styles/"), base);
  EXPECT_EQ(kNumCssLinks - kNumCssInCombination, segments.size());
}

TEST_F(CssCombineFilterTest, CombineCssManyFilesOneOrphan) {
  // This test differs from the previous test in we have exactly one CSS file
  // that stays on its own.
  // Note: Without CssCombine::Partnership::kUrlSlack this was:
  //const int kNumCssInCombination = 18
  const int kNumCssInCombination = 70;  // based on how we encode "yellow%d.css"
  const int kNumCssLinks = kNumCssInCombination + 1;
  CssLink::Vector css_in, css_out;
  for (int i = 0; i < kNumCssLinks - 1; ++i) {
    css_in.Add(StringPrintf("styles/yellow%d.css", i),
               kYellow, "", true);
  }
  css_in.Add("styles/last_one.css",
             kYellow, "", true);
  BarrierTestHelper("combine_css_many_files", css_in, &css_out);
  ASSERT_EQ(2, css_out.size());

  // Check that the first element is really a combination.
  std::string base;
  StringVector segments;
  ASSERT_TRUE(css_out[0]->DecomposeCombinedUrl(&base, &segments,
                                               &message_handler_));
  EXPECT_EQ(StrCat(kTestDomain, "styles/"), base);
  EXPECT_EQ(kNumCssInCombination, segments.size());
  EXPECT_EQ("styles/last_one.css", css_out[1]->url_);
}

// Note -- this test is redundant with CombineCssMissingResource -- this
// is a taste test.  This new mechanism is more code per test but I think
// the failures are more obvious and the expect/assert tests are in the
// top level of the test which might make it easier to debug.
TEST_F(CssCombineFilterTest, CombineCssNotCached) {
  CssLink::Vector css_in, css_out;
  css_in.Add("1.css", kYellow, "", true);
  css_in.Add("2.css", kYellow, "", true);
  css_in.Add("3.css", kYellow, "", false);
  css_in.Add("4.css", kYellow, "", true);
  mock_url_fetcher_.set_fail_on_unexpected(false);
  BarrierTestHelper("combine_css_not_cached", css_in, &css_out);
  EXPECT_EQ(3, css_out.size());
  std::string base;
  StringVector segments;
  ASSERT_TRUE(css_out[0]->DecomposeCombinedUrl(&base, &segments,
                                               &message_handler_));
  EXPECT_EQ(2, segments.size());
  EXPECT_EQ("1.css", segments[0]);
  EXPECT_EQ("2.css", segments[1]);
  EXPECT_EQ("3.css", css_out[1]->url_);
  EXPECT_EQ("4.css", css_out[2]->url_);
}

// Note -- this test is redundant with CombineCssWithIEDirective -- this
// is a taste test.
TEST_F(CssCombineFilterTest, CombineStyleTag) {
  CssLink::Vector css_in, css_out;
  css_in.Add("1.css", kYellow, "", true);
  css_in.Add("2.css", kYellow, "", true);
  css_in.Add("", "<style>a { color: red }</style>\n", "", false);
  css_in.Add("4.css", kYellow, "", true);
  BarrierTestHelper("combine_css_with_style", css_in, &css_out);
  EXPECT_EQ(2, css_out.size());
  std::string base;
  StringVector segments;
  ASSERT_TRUE(css_out[0]->DecomposeCombinedUrl(&base, &segments,
                                               &message_handler_));
  EXPECT_EQ(2, segments.size());
  EXPECT_EQ("1.css", segments[0]);
  EXPECT_EQ("2.css", segments[1]);
  EXPECT_EQ("4.css", css_out[1]->url_);
}

TEST_F(CssCombineFilterTest, NoAbsolutifySameDir) {
  CssLink::Vector css_in, css_out;
  css_in.Add("1.css", ".yellow {background-image: url('1.png');}\n", "", true);
  css_in.Add("2.css", ".yellow {background-image: url('2.png');}\n", "", true);
  BarrierTestHelper("combine_css_with_style", css_in, &css_out);
  EXPECT_EQ(1, css_out.size());

  // Note: the urls are not absolutified.
  std::string expected_combination =
      ".yellow {background-image: url('1.png');}\n"
      ".yellow {background-image: url('2.png');}\n";

  // Check fetched resource.
  std::string actual_combination;
  EXPECT_TRUE(ServeResourceUrl(css_out[0]->url_, &actual_combination));
  // TODO(sligocki): Check headers?
  EXPECT_EQ(expected_combination, actual_combination);
}

TEST_F(CssCombineFilterTest, DoAbsolutifyDifferentDir) {
  CssLink::Vector css_in, css_out;
  css_in.Add("1.css", ".yellow {background-image: url('1.png');}\n", "", true);
  css_in.Add("foo/2.css", ".yellow {background-image: url('2.png');}\n",
             "", true);
  BarrierTestHelper("combine_css_with_style", css_in, &css_out);
  EXPECT_EQ(1, css_out.size());

  std::string expected_combination = StrCat(
      ".yellow {background-image: url('1.png');}\n"
      ".yellow {background-image: url('", kTestDomain, "foo/2.png');}\n");

  // Check fetched resource.
  std::string actual_combination;
  EXPECT_TRUE(ServeResourceUrl(css_out[0]->url_, &actual_combination));
  // TODO(sligocki): Check headers?
  EXPECT_EQ(expected_combination, actual_combination);
}

// Verifies that we don't produce URLs that are too long in a corner case.
TEST_F(CssCombineFilterTest, CrossAcrossPathsExceedingUrlSize) {
  CssLink::Vector css_in, css_out;
  std::string long_name(600, 'z');
  css_in.Add(long_name + "/a.css", "a", "", true);
  css_in.Add(long_name + "/b.css", "b", "", true);

  // This last 'Add' causes the resolved path to change from long_path to "/".
  // Which makes the encoding way too long. So we expect this URL not to be
  // added to the combination and for the combination base to remain long_path.
  css_in.Add("sites/all/modules/ckeditor/ckeditor.css?3", "z", "", true);
  BarrierTestHelper("cross_paths", css_in, &css_out);
  EXPECT_EQ(2, css_out.size());
  std::string actual_combination;
  EXPECT_TRUE(ServeResourceUrl(css_out[0]->url_, &actual_combination));
  GoogleUrl gurl(css_out[0]->url_);
  EXPECT_EQ("/" + long_name + "/", gurl.PathSansLeaf());
  ResourceNamer namer;
  ASSERT_TRUE(namer.Decode(gurl.LeafWithQuery()));
  EXPECT_EQ("a.css+b.css", namer.name());
  EXPECT_EQ("ab", actual_combination);
}

// Verifies that we don't allow path-crossing URLs if that option is turned off.
TEST_F(CssCombineFilterTest, CrossAcrossPathsDisallowed) {
  options_.set_combine_across_paths(false);
  CssLink::Vector css_in, css_out;
  css_in.Add("a/a.css", "a", "", true);
  css_in.Add("b/b.css", "b", "", true);
  BarrierTestHelper("cross_paths", css_in, &css_out);
  ASSERT_EQ(2, css_out.size());
  EXPECT_EQ("a/a.css", css_out[0]->url_);
  EXPECT_EQ("b/b.css", css_out[1]->url_);
}

TEST_F(CssCombineFilterTest, CrossMappedDomain) {
  CssLink::Vector css_in, css_out;
  DomainLawyer* laywer = options_.domain_lawyer();
  laywer->AddRewriteDomainMapping("a.com", "b.com", &message_handler_);
  bool supply_mock = false;
  css_in.Add("http://a.com/1.css", kYellow, "", supply_mock);
  css_in.Add("http://b.com/2.css", kBlue, "", supply_mock);
  ResponseHeaders default_css_header;
  resource_manager_->SetDefaultHeaders(&kContentTypeCss, &default_css_header);
  mock_url_fetcher_.SetResponse("http://a.com/1.css", default_css_header,
                                kYellow);
  mock_url_fetcher_.SetResponse("http://b.com/2.css", default_css_header,
                                kBlue);
  BarrierTestHelper("combine_css_with_style", css_in, &css_out);
  EXPECT_EQ(1, css_out.size());
  std::string actual_combination;
  EXPECT_TRUE(ServeResourceUrl(css_out[0]->url_, &actual_combination));
  EXPECT_EQ(StringPiece("http://a.com/1.css+2.css.pagespeed.cc.0.css"),
            css_out[0]->url_);
  EXPECT_EQ(StrCat(kYellow, kBlue), actual_combination);
}

// Verifies that we cannot do the same cross-domain combo when we lack
// the domain mapping.
TEST_F(CssCombineFilterTest, CrossUnmappedDomain) {
  CssLink::Vector css_in, css_out;
  DomainLawyer* laywer = options_.domain_lawyer();
  laywer->AddDomain("a.com", &message_handler_);
  laywer->AddDomain("b.com", &message_handler_);
  bool supply_mock = false;
  const char kUrl1[] = "http://a.com/1.css";
  const char kUrl2[] = "http://b.com/2.css";
  css_in.Add(kUrl1, kYellow, "", supply_mock);
  css_in.Add(kUrl2, kBlue, "", supply_mock);
  ResponseHeaders default_css_header;
  resource_manager_->SetDefaultHeaders(&kContentTypeCss, &default_css_header);
  mock_url_fetcher_.SetResponse(kUrl1, default_css_header, kYellow);
  mock_url_fetcher_.SetResponse(kUrl2, default_css_header, kBlue);
  BarrierTestHelper("combine_css_with_style", css_in, &css_out);
  EXPECT_EQ(2, css_out.size());
  std::string actual_combination;
  EXPECT_EQ(kUrl1, css_out[0]->url_);
  EXPECT_EQ(kUrl2, css_out[1]->url_);
}

// Make sure bad requests do not corrupt our extension.
TEST_F(CssCombineFilterTest, NoExtensionCorruption) {
  TestCorruptUrl("%22", false);
}

TEST_F(CssCombineFilterTest, NoQueryCorruption) {
  TestCorruptUrl("?query", true);
}

/*
  TODO(jmarantz): cover intervening FLUSH
  TODO(jmarantz): consider converting some of the existing tests to this
   format, covering
           IE Directive
           @Import in any css element except the first
           link in noscript tag
           change in 'media'
           incompatible domain
           intervening inline style tag (TODO: outline first?)
*/

}  // namespace

}  // namespace net_instaweb
