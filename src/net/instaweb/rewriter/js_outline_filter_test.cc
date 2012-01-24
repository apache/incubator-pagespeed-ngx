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

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

class JsOutlineFilterTest : public ResourceManagerTestBase {
 protected:
  // We need an explicitly called method here rather than using SetUp so
  // that NoOutlineScript can call another AddFilter function first.
  void SetupOutliner() {
    options()->set_js_outline_min_bytes(0);
    options()->EnableFilter(RewriteOptions::kOutlineJavascript);
    rewrite_driver()->AddFilters();
  }

  // TODO(sligocki): factor out common elements in OutlineStyle and Script.
  // Test outlining scripts with options to write headers.
  void OutlineScript(const StringPiece& id, bool expect_outline) {
    GoogleString script_text = "FOOBAR";
    GoogleString outline_text;
    AppendDefaultHeaders(kContentTypeJavascript, &outline_text);
    outline_text += script_text;

    GoogleString hash = hasher()->Hash(script_text);
    GoogleString outline_url = Encode(
        kTestDomain, JsOutlineFilter::kFilterId, hash, "_", "js");

    GoogleString wrong_hash_outline_url = Encode(
      kTestDomain, JsOutlineFilter::kFilterId, "not" + hash, "_", "js");

    GoogleString html_input =
        "<head>\n"
        "  <title>Example style outline</title>\n"
        "  <!-- Script starts here -->\n"
        "  <script type='text/javascript'>" + script_text + "</script>\n"
        "  <!-- Script ends here -->\n"
        "</head>";
    GoogleString expected_output = !expect_outline ? html_input :
        "<head>\n"
        "  <title>Example style outline</title>\n"
        "  <!-- Script starts here -->\n"
        "  <script type='text/javascript'"
        " src=\"" + outline_url + "\"></script>\n"
        "  <!-- Script ends here -->\n"
        "</head>";
    ValidateExpected(id, html_input, expected_output);

    if (expect_outline) {
      GoogleString actual_outline;
      ResponseHeaders headers;
      EXPECT_TRUE(FetchResourceUrl(outline_url, &actual_outline, &headers));
      EXPECT_EQ(outline_text, StrCat(headers.ToString(), actual_outline));

      // Make sure we don't try anything funny with fallbacks if the hash
      // given is wrong. It'd be an attack vector otherwise since outlined
      // resources may contain things from private pages.
      EXPECT_FALSE(
          FetchResourceUrl(wrong_hash_outline_url, &actual_outline, &headers));
    }
  }
};

// Tests for outlining scripts.
TEST_F(JsOutlineFilterTest, OutlineScript) {
  SetupOutliner();
  OutlineScript("outline_scripts_no_hash", true);
}

TEST_F(JsOutlineFilterTest, OutlineScriptMd5) {
  UseMd5Hasher();
  SetupOutliner();
  OutlineScript("outline_scripts_md5", true);
}

// Make sure we don't misplace things into domain of the base tag,
// as we may not be able to fetch from it.
// (The leaf in base href= also covers a previous check failure)
TEST_F(JsOutlineFilterTest, OutlineScriptWithBase) {
  SetupOutliner();

  const char kInput[] =
      "<base href='http://cdn.example.com/file.html'><script>42;</script>";
  GoogleString expected_output =
      StrCat("<base href='http://cdn.example.com/file.html'>",
             "<script src=\"",
             EncodeWithBase("http://cdn.example.com/", kTestDomain,
                            "jo", "0", "_", "js"),
             "\"></script>");
  ValidateExpected("test.html", kInput, expected_output);
}

// Negative test.
TEST_F(JsOutlineFilterTest, NoOutlineScript) {
  GoogleString file_prefix = GTestTempDir() + "/no_outline";
  GoogleString url_prefix = "http://mysite/no_outline";

  // TODO(sligocki): Maybe test with other hashers.
  // SetHasher(hasher);

  options()->EnableFilter(RewriteOptions::kOutlineCss);
  SetupOutliner();

  static const char html_input[] =
      "<head>\n"
      "  <title>Example style outline</title>\n"
      "  <!-- Script starts here -->\n"
      "  <script type='text/javascript' src='http://othersite/script.js'>"
      "</script>\n"
      "  <!-- Script ends here -->\n"
      "</head>";
  ValidateNoChanges("no_outline_script", html_input);
}


// By default we succeed at outlining.
TEST_F(JsOutlineFilterTest, UrlNotTooLong) {
  SetupOutliner();
  OutlineScript("url_not_too_long", true);
}

// But if we set max_url_size too small, it will fail cleanly.
TEST_F(JsOutlineFilterTest, UrlTooLong) {
  options()->set_max_url_size(0);
  SetupOutliner();
  OutlineScript("url_too_long", false);
}

}  // namespace

}  // namespace net_instaweb
