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

// Author: mdsteele@google.com (Matthew D. Steele)

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"

namespace net_instaweb {

namespace {

class JsInlineFilterTest : public ResourceManagerTestBase {
 protected:
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
};

TEST_F(JsInlineFilterTest, DoInlineJavascriptSimple) {
  // Simple case:
  TestInlineJavascript("http://www.example.com/index.html",
                       "http://www.example.com/script.js",
                       "",
                       "function id(x) { return x; }\n",
                       true);
}

TEST_F(JsInlineFilterTest, DoInlineJavascriptWhitespace) {
  // Whitespace between <script> and </script>:
  TestInlineJavascript("http://www.example.com/index2.html",
                       "http://www.example.com/script2.js",
                       "\n    \n  ",
                       "function id(x) { return x; }\n",
                       true);
}

TEST_F(JsInlineFilterTest, DoNotInlineJavascriptDifferentDomain) {
  // Different domains:
  TestInlineJavascript("http://www.example.net/index.html",
                       "http://scripts.example.org/script.js",
                       "",
                       "function id(x) { return x; }\n",
                       false);
}

TEST_F(JsInlineFilterTest, DoNotInlineJavascriptInlineContents) {
  // Inline contents:
  TestInlineJavascript("http://www.example.com/index.html",
                       "http://www.example.com/script.js",
                       "{\"json\": true}",
                       "function id(x) { return x; }\n",
                       false);
}

TEST_F(JsInlineFilterTest, DoNotInlineJavascriptTooBig) {
  // Javascript too long:
  const int64 length = 2 * RewriteOptions::kDefaultJsInlineMaxBytes;
  TestInlineJavascript("http://www.example.com/index.html",
                       "http://www.example.com/script.js",
                       "",
                       ("function longstr() { return '" +
                        std::string(length, 'z') + "'; }\n"),
                       false);
}

}  // namespace

}  // namespace net_instaweb
