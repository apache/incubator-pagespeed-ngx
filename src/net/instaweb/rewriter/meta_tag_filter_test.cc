/*
 * Copyright 2011 Google Inc.
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

// Author: nforman@google.com (Naomi Forman)
//
// This file contains unit tests for the MetaTagFilter.

#include "net/instaweb/rewriter/public/meta_tag_filter.h"

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/meta_data.h"  // for HttpAttributes, etc
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {
// Test fixture for MetaTagFilter unit tests.
class MetaTagFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    options()->EnableFilter(RewriteOptions::kConvertMetaTags);
    RewriteTestBase::SetUp();
    rewrite_driver()->AddFilters();
    headers_.Clear();
    rewrite_driver()->set_response_headers_ptr(&headers_);
    headers_.Replace(HttpAttributes::kContentType, "text/html");
  }

  ResponseHeaders* headers() {
    return &headers_;
  }
 private:
  ResponseHeaders headers_;
};

const char kMetaTagDoc[] =
    "<html><head>"
    "<meta http-equiv=\"Content-Type\" content=\"text/html;  charset=UTF-8\">"
    "<META HTTP-EQUIV=\"CONTENT-LANGUAGE\" CONTENT=\"en-US,fr\">"
    "</head><body></body></html>";

TEST_F(MetaTagFilterTest, TestTags) {
  ValidateNoChanges("convert_tags", kMetaTagDoc);
  ConstStringStarVector values;
  EXPECT_TRUE(headers()->Lookup(HttpAttributes::kContentType, &values));
  ASSERT_EQ(1, values.size());
  EXPECT_TRUE(StringCaseEqual(*values[0], "text/html; charset=UTF-8"))
      << *values[0];
}

const char kMetaTagDoubleDoc[] =
    "<html><head>"
    "<meta http-equiv=\"Content-Type\" content=\"text/html;  charset=UTF-8\">"
    "<meta http-equiv=\"Content-Type\" content=\"text/html;  charset=UTF-8\">"
    "<META HTTP-EQUIV=\"CONTENT-LANGUAGE\" CONTENT=\"en-US,FR\">"
    "<META HTTP-EQUIV=\"CONTENT-LANGUAGE\" CONTENT=\"en-US,fr\">"
    "</head><body></body></html>";

TEST_F(MetaTagFilterTest, TestDoubleTags) {
  ValidateNoChanges("convert_tags_once", kMetaTagDoubleDoc);
  ConstStringStarVector values;
  EXPECT_TRUE(headers()->Lookup(HttpAttributes::kContentType, &values));
  ASSERT_EQ(1, values.size());
  EXPECT_TRUE(StringCaseEqual(*values[0], "text/html; charset=UTF-8"))
      << *values[0];
}

TEST_F(MetaTagFilterTest, TestEquivNoValue) {
  // Make sure we don't crash when a meta http-equiv has no content given.
  ValidateNoChanges("no_value", "<meta http-equiv='NoValue'>");
}

const char kMetaTagConflictDoc[] =
    "<html><head>"
    "<meta http-equiv=\"Content-Type\" content=\"text/html;  charset=UTF-8\">"
    "<meta http-equiv=\"Content-Type\" content=\"text/xml;  charset=UTF-16\">"
    "<meta http-equiv=\"Content-Type\" content=\"text/xml\">"
    "</head><body></body></html>";


TEST_F(MetaTagFilterTest, TestConflictingTags) {
  ValidateNoChanges("convert_tags_first", kMetaTagConflictDoc);
  ConstStringStarVector values;
  EXPECT_TRUE(headers()->Lookup(HttpAttributes::kContentType, &values));
  ASSERT_EQ(1, values.size());
  EXPECT_TRUE(StringCaseEqual(*values[0], "text/html; charset=UTF-8"))
      << *values[0];
}

const char kMetaTagCharset[] =
    "<html><head>"
    "<meta http-equiv=\"Content-Type\" content=\"text/html\">"
    "<meta charset=\"UTF-8\">"
    "<meta http-equiv=\"Content-Type\" content=\"text/xml; charset=UTF-16\">"
    "</head><body></body></html>";


TEST_F(MetaTagFilterTest, TestCharset) {
  ValidateNoChanges("convert_charset", kMetaTagCharset);
  ConstStringStarVector values;
  EXPECT_TRUE(headers()->Lookup(HttpAttributes::kContentType, &values));
  ASSERT_EQ(1, values.size());
  const GoogleString* val = values[0];
  EXPECT_TRUE(StringCaseEqual(*val, "text/html; charset=UTF-8")) << *val;
}

const char kMetaTagCharsetOnly[] =
    "<html><head>"
    "<meta charset=\"UTF-8\">"
    "</head><body></body></html>";

TEST_F(MetaTagFilterTest, TestCharsetOnly) {
  // Merges charset into pre-existing mimetype.
  ValidateNoChanges("convert_charset_only", kMetaTagCharsetOnly);
  ConstStringStarVector values;
  EXPECT_TRUE(headers()->Lookup(HttpAttributes::kContentType, &values));
  ASSERT_EQ(1, values.size());
  const GoogleString* val = values[0];
  EXPECT_TRUE(StringCaseEqual(*val, "text/html; charset=UTF-8")) << *val;
}

TEST_F(MetaTagFilterTest, TestCharsetNoUpstream) {
  // No mimetype to merge charset into, it gets dropped.
  headers()->RemoveAll(HttpAttributes::kContentType);
  ValidateNoChanges("convert_charset_only", kMetaTagCharsetOnly);
  ConstStringStarVector values;
  EXPECT_FALSE(headers()->Lookup(HttpAttributes::kContentType, &values));
}

const char kMetaTagDoNothing[] =
    "<html><head>"
    "<meta http-equiv=\"\" content=\"\">"
    "<meta http-equiv=\"  \" content=\"\">"
    "<meta http-equiv=\"  :\" content=\"\">"
    "<meta http-equiv=\"Content-Length\" content=\"123\">"
    "</head><body></body></html>";


TEST_F(MetaTagFilterTest, TestDoNothing) {
  ValidateNoChanges("do_nothing", kMetaTagDoNothing);
  ASSERT_EQ(1, headers()->NumAttributes());
  EXPECT_STREQ("text/html", headers()->Lookup1(HttpAttributes::kContentType));
}

const char kMetaTagNoScriptDoc[] =
    "<html><head>"
    "<noscript>"
    "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">"
    "</noscript>"
    "</head><body></body></html>";

TEST_F(MetaTagFilterTest, TestNoScript) {
  ValidateNoChanges("no_script", kMetaTagDoNothing);
  ASSERT_EQ(1, headers()->NumAttributes());
  EXPECT_STREQ("text/html", headers()->Lookup1(HttpAttributes::kContentType));
}

const char kMetaTagNoQuotes[] =
    "<html><head>"
    "<meta http-equiv=Content-Type content=text/html; charset=UTF-8>"
    "</head><body></body></html>";

TEST_F(MetaTagFilterTest, TestNoQuotes) {
  // See http://webdesign.about.com/od/metatags/qt/meta-charset.htm for an
  // explanation of why we are testing this invalid format.
  ValidateNoChanges("convert_tags", kMetaTagNoQuotes);
  ConstStringStarVector values;
  EXPECT_TRUE(headers()->Lookup(HttpAttributes::kContentType, &values));
  ASSERT_EQ(1, values.size());
  EXPECT_TRUE(StringCaseEqual(*values[0], "text/html; charset=UTF-8"))
      << *values[0];
}

TEST_F(MetaTagFilterTest, DoNotOverrideWithFakeXhtmlUnsure) {
  // We shouldn't override with XHTML even if mimetype is unknown.
  // This uses a bogus "XHTML" mimetype which we recognized for some versions.
  headers()->RemoveAll(HttpAttributes::kContentType);
  ValidateNoChanges("no_override", "<meta http-equiv=\"Content-Type\" "
                                   "content=\"text/xhtml; charset=UTF-8\">");
  ConstStringStarVector values;
  EXPECT_FALSE(headers()->Lookup(HttpAttributes::kContentType, &values));
}

TEST_F(MetaTagFilterTest, DoNotOverrideWithRealXhtmlUnsure) {
  // We shouldn't override with XHTML even if mimetype is unknown.
  headers()->RemoveAll(HttpAttributes::kContentType);
  ValidateNoChanges("no_override",
                    StrCat("<meta http-equiv=\"Content-Type\" content=\"",
                           kContentTypeXhtml.mime_type(),
                           " ;charset=UTF-8\">"));
  ConstStringStarVector values;
  EXPECT_FALSE(headers()->Lookup(HttpAttributes::kContentType, &values));
}

TEST_F(MetaTagFilterTest, DoNotOverrideWithFakeXhtmlKnown) {
  // We shouldn't override with XHTML if the server already knows it's HTML
  // This uses a bogus "XHTML" mimetype which we recognized for some versions.
  ValidateNoChanges("no_override", "<meta http-equiv=\"Content-Type\" "
                                   "content=\"text/xhtml; charset=UTF-8\">");
  ConstStringStarVector values;
  EXPECT_TRUE(headers()->Lookup(HttpAttributes::kContentType, &values));
  ASSERT_EQ(1, values.size());
  EXPECT_TRUE(StringCaseEqual(*values[0], "text/html")) << *values[0];
}

TEST_F(MetaTagFilterTest, DoNotOverrideWithRealXhtmlKnown) {
  // We shouldn't override with XHTML if the server already knows it's HTML
  ValidateNoChanges("no_override",
                    StrCat("<meta http-equiv=\"Content-Type\" content=\"",
                           kContentTypeXhtml.mime_type(), ";charset=UTF-8\">"));
  ConstStringStarVector values;
  EXPECT_TRUE(headers()->Lookup(HttpAttributes::kContentType, &values));
  ASSERT_EQ(1, values.size());
  EXPECT_TRUE(StringCaseEqual(*values[0], "text/html")) << *values[0];
}

TEST_F(MetaTagFilterTest, DoNotOverrideCharsetBothXhtml) {
  // An XHTML document specifying a non-utf8 encoding via a http-equiv meta
  // should not take effect, either.
  GoogleString initial_header = StrCat(kContentTypeXhtml.mime_type(),
                                       "; charset=UTF-8");
  headers()->Replace(HttpAttributes::kContentType, initial_header);

  ValidateNoChanges("no_override",
                    StrCat("<meta http-equiv=\"Content-Type\" content=\"",
                           kContentTypeXhtml.mime_type(),
                           "; charset=KOI8-R\">"));
  ConstStringStarVector values;
  EXPECT_TRUE(headers()->Lookup(HttpAttributes::kContentType, &values));
  ASSERT_EQ(1, values.size());
  EXPECT_EQ(initial_header, *values[0]);
}

}  // namespace

}  // namespace net_instaweb
