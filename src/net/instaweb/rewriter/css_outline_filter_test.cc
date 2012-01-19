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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/css_outline_filter.h"

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

class CssOutlineFilterTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    options()->set_css_outline_min_bytes(0);
    options()->EnableFilter(RewriteOptions::kOutlineCss);
    rewrite_driver()->AddFilters();
  }

  // Test general situations.
  void TestOutlineCss(const GoogleString& html_url,
                      const GoogleString& base_ref,
                      const GoogleString& css_original_body,
                      bool expect_outline,
                      const GoogleString& css_rewritten_body,
                      // css_url_base only needed if different than html_url,
                      // e.g. domain rewriting.
                      const GoogleString& css_url_base) {
    // TODO(sligocki): Test with outline threshold > 0.

    // Figure out outline_url.
    GoogleString hash = hasher()->Hash(css_rewritten_body);
    GoogleUrl css_gurl_base;
    if (css_url_base.empty()) {
      css_gurl_base.Reset(html_url);
    } else {
      css_gurl_base.Reset(css_url_base);
    }
    GoogleUrl base_ref_gurl;
    if (base_ref.empty()) {
      base_ref_gurl.Reset(html_url);
    } else {
      base_ref_gurl.Reset(base_ref);
    }
    GoogleString css_gurl_base_origin = StrCat(css_gurl_base.Origin(), "/");
    GoogleString base_ref_gurl_origin = StrCat(base_ref_gurl.Origin(), "/");
    GoogleString outline_url = EncodeWithBase(base_ref_gurl_origin,
                                              css_gurl_base_origin,
                                              CssOutlineFilter::kFilterId,
                                              hash, "_", "css");
    // Add a base href to the HTML iff specified.
    GoogleString other_content;
    if (!base_ref.empty()) {
      other_content = StrCat("  <base href=\"", base_ref, "\">\n");
    }

    const GoogleString html_input =
        "<head>\n" +
        other_content +
        "  <style>" + css_original_body + "</style>\n"
        "</head>\n"
        "<body>Hello, world!</body>\n";

    // Rewrite the HTML page.
    ParseUrl(html_url, html_input);

    // Check output HTML.
    const GoogleString expected_output =
        (!expect_outline ? html_input :
         "<head>\n" +
         other_content +
         "  <link rel=\"stylesheet\" href=\"" + outline_url + "\">\n"
         "</head>\n"
         "<body>Hello, world!</body>\n");
    EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);

    if (expect_outline) {
      // Expected headers.
      GoogleString expected_headers;
      AppendDefaultHeaders(kContentTypeCss, &expected_headers);

      // Check fetched resource.
      GoogleString actual_outline;
      ResponseHeaders actual_headers;
      EXPECT_TRUE(ServeResourceUrl(outline_url, &actual_outline,
                                   &actual_headers));
      EXPECT_EQ(expected_headers, actual_headers.ToString());
      EXPECT_EQ(css_rewritten_body, actual_outline);
    }
  }

  void OutlineStyle(const StringPiece& id) {
    GoogleString html_url = StrCat("http://outline_style.test/", id, ".html");
    GoogleString style_text = "background_blue { background-color: blue; }\n"
                              "foreground_yellow { color: yellow; }\n";
    TestOutlineCss(html_url, "", style_text, true, style_text, "");
  }
};

// Tests for Outlining styles.
TEST_F(CssOutlineFilterTest, OutlineStyle) {
  OutlineStyle("outline_styles_no_hash");
}

TEST_F(CssOutlineFilterTest, OutlineStyleMD5) {
  UseMd5Hasher();
  OutlineStyle("outline_styles_md5");
}


TEST_F(CssOutlineFilterTest, NoAbsolutifySameDir) {
  const GoogleString css = "body { background-image: url('bg.png'); }";
  TestOutlineCss("http://outline_style.test/index.html", "",
                 css, true, css, "");
}

TEST_F(CssOutlineFilterTest, AbsolutifyDifferentDir) {
  const GoogleString css1 = "body { background-image: url('bg.png'); }";
  const GoogleString css2 =
      "body { background-image: url('http://other_site.test/foo/bg.png'); }";
  TestOutlineCss("http://outline_style.test/index.html",
                 "http://other_site.test/foo/", css1, true, css2, "");
}

TEST_F(CssOutlineFilterTest, ShardSubresources) {
  UseMd5Hasher();
  DomainLawyer* lawyer = options()->domain_lawyer();
  lawyer->AddShard("outline_style.test", "shard1.com,shard2.com",
                   &message_handler_);

  const GoogleString css_in =
      ".p1 { background-image: url('b1.png'); }"
      ".p2 { background-image: url('b2.png'); }";
  const GoogleString css_out =
      ".p1 { background-image: url('http://shard2.com/b1.png'); }"
      ".p2 { background-image: url('http://shard1.com/b2.png'); }";
  TestOutlineCss("http://outline_style.test/index.html", "",
                 css_in, true, css_out, "http://shard1.com/");
}

TEST_F(CssOutlineFilterTest, UrlTooLong) {
  GoogleString html_url = "http://outline_style.test/url_size_test.html";
  GoogleString style_text = "background_blue { background-color: blue; }\n"
                            "foreground_yellow { color: yellow; }\n";

  // By default we succeed at outlining.
  TestOutlineCss(html_url, "", style_text, true, style_text, "");

  // But if we set max_url_size too small, it will fail cleanly.
  options()->ClearSignatureForTesting();
  options()->set_max_url_size(0);
  resource_manager()->ComputeSignature(options());
  TestOutlineCss(html_url, "", style_text, false, style_text, "");
}

}  // namespace

}  // namespace net_instaweb
