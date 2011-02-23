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

#include "net/instaweb/rewriter/public/js_outline_filter.h"

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"

namespace net_instaweb {

namespace {

class JsOutlineFilterTest : public ResourceManagerTestBase {
 protected:
  // TODO(sligocki): factor out common elements in OutlineStyle and Script.
  // Test outlining scripts with options to write headers and use a hasher.
  void OutlineScript(const StringPiece& id, Hasher* hasher) {
    resource_manager_->set_hasher(hasher);

    options_.set_js_outline_min_bytes(0);
    AddFilter(RewriteOptions::kOutlineJavascript);

    std::string script_text = "FOOBAR";
    std::string outline_text;
    AppendDefaultHeaders(kContentTypeJavascript, resource_manager_,
                         &outline_text);
    outline_text += script_text;

    std::string hash = hasher->Hash(script_text);
    std::string outline_filename;
    std::string outline_url = Encode(
        kTestDomain, JsOutlineFilter::kFilterId,  hash, "_", "js");
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
        "  <script type='text/javascript'"
        " src=\"" + outline_url + "\"></script>\n"
        "  <!-- Script ends here -->\n"
        "</head>";
    ValidateExpected(id, html_input, expected_output);

    std::string actual_outline;
    ASSERT_TRUE(file_system_.ReadFile(outline_filename.c_str(),
                                      &actual_outline,
                                      &message_handler_));
    EXPECT_EQ(outline_text, actual_outline);
  }
};

// Tests for outlining scripts.
TEST_F(JsOutlineFilterTest, OutlineScript) {
  OutlineScript("outline_scripts_no_hash_with_headers", &mock_hasher_);
}

// Negative test.
TEST_F(JsOutlineFilterTest, NoOutlineScript) {
  std::string file_prefix = GTestTempDir() + "/no_outline";
  std::string url_prefix = "http://mysite/no_outline";

  // TODO(sligocki): Maybe test with other hashers.
  //resource_manager_->set_hasher(hasher);

  options_.EnableFilter(RewriteOptions::kOutlineCss);
  options_.EnableFilter(RewriteOptions::kOutlineJavascript);
  rewrite_driver_.AddFilters();

  // We need to make sure we don't create this file, so rm any old one
  std::string filename = Encode(file_prefix, JsOutlineFilter::kFilterId, "0",
                                 "_", "js");
  DeleteFileIfExists(filename.c_str());

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
  EXPECT_FALSE(file_system_.Exists(filename.c_str(),
                                   &message_handler_).is_true());
}

}  // namespace

}  // namespace net_instaweb
