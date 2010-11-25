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
    std::string outline_url = Encode(
        "http://test.com/", CssOutlineFilter::kFilterId, hash, "_", "css");
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
};

// Tests for Outlining styles.
TEST_F(CssOutlineFilterTest, OutlineStyle) {
  OutlineStyle("outline_styles_no_hash", &mock_hasher_);
}

TEST_F(CssOutlineFilterTest, OutlineStyleMD5) {
  OutlineStyle("outline_styles_md5", &md5_hasher_);
}


}  // namespace

}  // namespace net_instaweb
