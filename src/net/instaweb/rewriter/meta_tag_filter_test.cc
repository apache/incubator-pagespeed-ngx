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

}  // namespace

}  // namespace net_instaweb
