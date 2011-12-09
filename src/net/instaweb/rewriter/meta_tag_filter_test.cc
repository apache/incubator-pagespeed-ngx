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

#include "net/instaweb/http/public/meta_data.h"  // for HttpAttributes, etc
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {
// Test fixture for MetaTagFilter unit tests.
class MetaTagFilterTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    options()->EnableFilter(RewriteOptions::kConvertMetaTags);
    ResourceManagerTestBase::SetUp();
    rewrite_driver()->AddFilters();
    headers_.Clear();
    rewrite_driver()->set_response_headers_ptr(&headers_);
  }

  const ResponseHeaders* headers() {
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
  EXPECT_TRUE(StringCaseEqual(values[0]->c_str(), "text/html;  charset=UTF-8"));
  EXPECT_TRUE(headers()->Lookup(HttpAttributes::kContentLanguage, &values));
  ASSERT_EQ(1, values.size());
  EXPECT_TRUE(StringCaseEqual(values[0]->c_str(), "en-US,fr"));
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
  EXPECT_TRUE(StringCaseEqual(values[0]->c_str(), "text/html;  charset=UTF-8"));
  EXPECT_TRUE(headers()->Lookup(HttpAttributes::kContentLanguage, &values));
  ASSERT_EQ(1, values.size());
  EXPECT_TRUE(StringCaseEqual(values[0]->c_str(), "en-US,fr"));
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
  EXPECT_TRUE(StringCaseEqual(values[0]->c_str(), "text/html;  charset=UTF-8"));
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
  EXPECT_TRUE(StringCaseEqual(val->c_str(), "text/html; charset=UTF-8"));
}

const char kMetaTagCharsetOnly[] =
    "<html><head>"
    "<meta charset=\"UTF-8\">"
    "</head><body></body></html>";

TEST_F(MetaTagFilterTest, TestCharsetOnly) {
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
  ASSERT_EQ(0, headers()->NumAttributes());
}

}  // namespace

}  // namespace net_instaweb
