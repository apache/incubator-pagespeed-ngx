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

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"

namespace net_instaweb {

namespace {

class CssOutlineFilterTest : public ResourceManagerTestBase {
 protected:
  // Test general situations.
  void TestOutlineCss(const std::string& html_url,
                      const std::string& other_content,  // E.g. <base href>
                      const std::string& css_original_body,
                      bool expect_outline,
                      const std::string& css_rewritten_body) {
    // TODO(sligocki): Test with outline threshold > 0.
    options_.set_css_outline_min_bytes(0);
    AddFilter(RewriteOptions::kOutlineCss);

    // Figure out outline_url.
    std::string hash = resource_manager_->hasher()->Hash(css_rewritten_body);
    GoogleUrl html_gurl(html_url);
    GoogleUrl outline_gurl(
        html_gurl,
        Encode("", CssOutlineFilter::kFilterId, hash, "_", "css"));
    StringPiece spec = outline_gurl.Spec();
    std::string outline_url(spec.data(), spec.length());
    // Figure out outline_filename.
    std::string outline_filename;
    filename_encoder_.Encode(file_prefix_, outline_url, &outline_filename);
    // Make sure the file we check later was written this time, rm any old one.
    DeleteFileIfExists(outline_filename);

    const std::string html_input =
        "<head>\n" +
        other_content +
        "  <style>" + css_original_body + "</style>\n"
        "</head>\n"
        "<body>Hello, world!</body>\n";

    // Rewrite the HTML page.
    ParseUrl(html_url, html_input);

    // Check output HTML.
    const std::string expected_output =
        (!expect_outline ? html_input :
         "<head>\n" +
         other_content +
         "  <link rel=\"stylesheet\" href=\"" + outline_url + "\">\n"
         "</head>\n"
         "<body>Hello, world!</body>\n");
    EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);

    // Expected headers.
    std::string expected_headers;
    AppendDefaultHeaders(kContentTypeCss, resource_manager_, &expected_headers);

    // Check file was written.
    // TODO(sligocki): Should we check this or only the fetch below?
    std::string actual_outline;
    ASSERT_TRUE(file_system_.ReadFile(outline_filename.c_str(),
                                      &actual_outline,
                                      &message_handler_));
    EXPECT_EQ(expected_headers + css_rewritten_body, actual_outline);

    // Check fetched resource.
    actual_outline.clear();
    EXPECT_TRUE(ServeResourceUrl(outline_url, &actual_outline));
    // TODO(sligocki): Check headers?
    EXPECT_EQ(css_rewritten_body, actual_outline);
  }

  // Test with different hashers for a specific situation.
  void OutlineStyle(const StringPiece& id, Hasher* hasher) {
    resource_manager_->set_hasher(hasher);

    std::string html_url = StrCat("http://outline_style.test/", id, ".html");
    std::string style_text = "background_blue { background-color: blue; }\n"
                              "foreground_yellow { color: yellow; }\n";
    TestOutlineCss(html_url, "", style_text, true, style_text);
  }
};

// Tests for Outlining styles.
TEST_F(CssOutlineFilterTest, OutlineStyle) {
  OutlineStyle("outline_styles_no_hash", &mock_hasher_);
}

TEST_F(CssOutlineFilterTest, OutlineStyleMD5) {
  OutlineStyle("outline_styles_md5", &md5_hasher_);
}


TEST_F(CssOutlineFilterTest, NoAbsolutifySameDir) {
  const std::string css = "body { background-image: url('bg.png'); }";
  TestOutlineCss("http://outline_style.test/index.html", "",
                 css, true, css);
}

TEST_F(CssOutlineFilterTest, AbsolutifyDifferentDir) {
  const std::string css1 = "body { background-image: url('bg.png'); }";
  const std::string css2 =
      "body { background-image: url('http://other_site.test/foo/bg.png'); }";
  TestOutlineCss("http://outline_style.test/index.html",
                 "  <base href=\"http://other_site.test/foo/\">\n",
                 css1, true, css2);
}

}  // namespace

}  // namespace net_instaweb
