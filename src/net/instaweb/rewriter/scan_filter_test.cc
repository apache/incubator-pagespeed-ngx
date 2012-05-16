/*
 * Copyright 2012 Google Inc.
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

// Author: matterbury@google.com (Matt Atterbury)
//
// Unit tests for ScanFilter.

#include "net/instaweb/rewriter/public/scan_filter.h"

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/charset_util.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// Test fixture for ScanFilter unit tests.
class ScanFilterTest : public ResourceManagerTestBase {
};

TEST_F(ScanFilterTest, EmptyPage) {
  // By default the base is the URL, which is set by ValidateNoChanges.
  const char kTestName[] = "empty_page";
  ValidateNoChanges(kTestName, "<head></head>");
  EXPECT_STREQ(StrCat(kTestDomain, kTestName, ".html"),
               rewrite_driver()->base_url().Spec());
  EXPECT_FALSE(rewrite_driver()->refs_before_base());
}

TEST_F(ScanFilterTest, SetBase) {
  // The default base (the URL) is overridden by a base tag.
  const char kTestName[] = "set_base";
  const char kNewBase[] = "http://example.com/index.html";
  ValidateNoChanges(kTestName,
                    StrCat("<head>"
                           "<base href=\"", kNewBase, "\">"
                           "</head>"));
  EXPECT_STREQ(kNewBase, rewrite_driver()->base_url().Spec());
  EXPECT_FALSE(rewrite_driver()->refs_before_base());
}

TEST_F(ScanFilterTest, RefsAfterBase) {
  // Check that we don't flag refs after the base tag.
  const char kTestName[] = "refs_after_base";
  const char kNewBase[] = "http://example.com/index.html";
  ValidateNoChanges(kTestName,
                    StrCat("<head>"
                           "<base href=\"", kNewBase, "\">"
                           "<a href=\"help.html\">link</a>"
                           "</head>"));
  EXPECT_STREQ(kNewBase, rewrite_driver()->base_url().Spec());
  EXPECT_FALSE(rewrite_driver()->refs_before_base());
}

TEST_F(ScanFilterTest, RefsBeforeBase) {
  // Check that we do flag refs before the base tag.
  const char kTestName[] = "refs_after_base";
  const char kNewBase[] = "http://example.com/index.html";
  ValidateNoChanges(kTestName,
                    StrCat("<head>"
                           "<a href=\"help.html\">link</a>"
                           "<base href=\"", kNewBase, "\">"
                           "</head>"));
  EXPECT_STREQ(kNewBase, rewrite_driver()->base_url().Spec());
  EXPECT_TRUE(rewrite_driver()->refs_before_base());
}

TEST_F(ScanFilterTest, NoCharset) {
  // Check that the charset is empty if we don't set it in any way.
  const char kTestName[] = "no_charset";
  ValidateNoChanges(kTestName, "<head></head>");
  EXPECT_TRUE(rewrite_driver()->containing_charset().empty());
}

TEST_F(ScanFilterTest, CharsetFromResponseHeaders) {
  // Check that the charset is taken from the response headers.
  const char kTestName[] = "charset_from_response_headers";
  ResponseHeaders headers;
  headers.MergeContentType("text/html; charset=iso-8859-1");
  rewrite_driver()->set_response_headers_ptr(&headers);
  ValidateNoChanges(kTestName, "<head></head>");
  EXPECT_STREQ("iso-8859-1", rewrite_driver()->containing_charset());
}

TEST_F(ScanFilterTest, CharsetFromBomDoesntOverride) {
  // Check that a BOM does not override the charset from the headers.
  const char kTestName[] = "charset_from_bom_doesnt_override";
  ResponseHeaders headers;
  headers.MergeContentType("text/html; charset=iso-8859-1");
  rewrite_driver()->set_response_headers_ptr(&headers);
  SetDoctype(kUtf8Bom);
  ValidateNoChanges(kTestName, "<head></head>");
  EXPECT_STREQ("iso-8859-1", rewrite_driver()->containing_charset());
}

TEST_F(ScanFilterTest, CharsetFromBom) {
  // Check that a BOM sets the charset.
  const char kTestName[] = "charset_from_bom";
  SetDoctype(kUtf8Bom);
  ValidateNoChanges(kTestName, "<head></head>");
  EXPECT_STREQ(kUtf8Charset, rewrite_driver()->containing_charset());
}

TEST_F(ScanFilterTest, CharsetFromMetaTagDoesntOverrideHeaders) {
  // Check that a meta tag does not override the charset from the headers.
  const char kTestName[] = "charset_from_meta_tag_doesnt_override_headers";
  ResponseHeaders headers;
  headers.MergeContentType("text/html; charset=iso-8859-1");
  rewrite_driver()->set_response_headers_ptr(&headers);
  ValidateNoChanges(kTestName,
                    "<head>"
                    "<meta charset=\"UTF-8\">"
                    "</head>");
  EXPECT_STREQ("iso-8859-1", rewrite_driver()->containing_charset());
}

TEST_F(ScanFilterTest, CharsetFromMetaTagDoesntOverrideBom) {
  // Check that a meta tag does not override the charset from a BOM.
  const char kTestName[] = "charset_from_meta_tag_doesnt_override_bom";
  SetDoctype(kUtf8Bom);
  ValidateNoChanges(kTestName,
                    "<head>"
                    "<meta charset=\"us-ascii\">"
                    "</head>");
  EXPECT_STREQ(kUtf8Charset, rewrite_driver()->containing_charset());
}

TEST_F(ScanFilterTest, CharsetFromMetaTag) {
  // Check that a meta tag sets the charset.
  const char kTestName[] = "charset_from_meta_tag";
  ValidateNoChanges(kTestName,
                    "<head>"
                    "<meta charset=\"UTF-8\">"
                    "</head>");
  EXPECT_STREQ("UTF-8", rewrite_driver()->containing_charset());
}

TEST_F(ScanFilterTest, CharsetFromFirstMetaTag) {
  // Check that the first meta tag is used.
  const char kTestName[] = "charset_from_first_meta_tag";
  ValidateNoChanges(kTestName,
                    "<head>"
                    "<meta http-equiv=\"Content-Type\" "
                          "content=\"text/xml; charset=us-ascii\">"
                    "<meta charset=\"UTF-8\">"
                    "</head>");
  EXPECT_STREQ("us-ascii", rewrite_driver()->containing_charset());
}

TEST_F(ScanFilterTest, CharsetFromFirstMetaTagWithCharset) {
  // Check that the first meta tag is used.
  const char kTestName[] = "charset_from_first_meta_tag_with_charset";
  ValidateNoChanges(kTestName,
                    "<head>"
                    "<meta http-equiv=\"Content-Type\">"
                    "<meta charset=\"UTF-8\">"
                    "</head>");
  EXPECT_STREQ("UTF-8", rewrite_driver()->containing_charset());
}

TEST_F(ScanFilterTest, CharsetFromMetaTagMissingQuotes) {
  // Check that the first meta tag is used even if it's missing the quotes.
  const char kTestName[] = "charset_from_meta_tag_missing_quotes";
  ValidateNoChanges(kTestName,
                    "<head>"
                    "<meta http-equiv=Content-Type "
                          "content=text/html; charset=us-ascii>"
                    "<meta charset=\"UTF-8\">"
                    "</head>");
  EXPECT_STREQ("us-ascii", rewrite_driver()->containing_charset());
}

}  // namespace net_instaweb
