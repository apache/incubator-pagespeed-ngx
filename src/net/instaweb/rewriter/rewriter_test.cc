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

// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test some small filters.

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

class RewriterTest : public ResourceManagerTestBase {};

TEST_F(RewriterTest, AddHead) {
  AddFilter(RewriteOptions::kAddHead);
  ValidateExpected("add_head",
      "<body><p>text</p></body>",
      "<head/><body><p>text</p></body>");
}

TEST_F(RewriterTest, AddHeadNoBody) {
  // Test for proper diagnostic (regression test for Issue 134)
  AddFilter(RewriteOptions::kAddHead);
  ValidateExpected("add_head_no_body",
      "<p>text</p>",
      "<p>text</p>");
  EXPECT_EQ(0, message_handler_.SeriousMessages());
}

TEST_F(RewriterTest, MergeHead) {
  AddFilter(RewriteOptions::kCombineHeads);
  ValidateExpected("merge_2_heads",
      "<head a><p>1</p></head>4<head b>2<link x>3</head><link y>end",
      "<head a><p>1</p>2<link x>3</head>4<link y>end");
  ValidateExpected("merge_3_heads",
      "<head a><p>1</p></head>4<head b>2<link x>3</head><link y>"
      "<body>b<head><link z></head>ye</body>",
      "<head a><p>1</p>2<link x>3<link z></head>4<link y>"
      "<body>bye</body>");
}

TEST_F(RewriterTest, HandlingOfInvalidUrls) {
  Hasher* hasher = &md5_hasher_;
  resource_manager_->set_hasher(hasher);
  AddFilter(RewriteOptions::kRewriteCss);

  const char kCssData[] = "a { color: red }";
  const char kMinimizedCssData[] = "a{color:red}";
  InitResponseHeaders("a.css", kContentTypeCss, kCssData, 100);

  // Fetching the real rewritten resource name should work.
  // TODO(sligocki): This will need to be regolded if naming format changes.
  GoogleString hash = hasher->Hash(kMinimizedCssData);
  GoogleString good_url =
      Encode(kTestDomain, RewriteDriver::kCssFilterId, hash, "a.css", "css");
  EXPECT_TRUE(TryFetchResource(good_url));

  // Querying with an appended query should work fine, too, and cause a cache
  // hit from the above, not recomputation
  int inserts_before = lru_cache_->num_inserts();
  int hits_before = lru_cache_->num_hits();
  EXPECT_TRUE(TryFetchResource(StrCat(good_url, "?foo")));
  int inserts_after = lru_cache_->num_inserts();
  EXPECT_EQ(0, lru_cache_->num_identical_reinserts());
  EXPECT_EQ(inserts_before, inserts_after);
  EXPECT_EQ(hits_before + 1, lru_cache_->num_hits());

  // Fetching variants should not cause system problems.
  // Changing hash still works.
  // Note: If any of these switch from true to false, that's probably fine.
  // We'd just like to keep track of what causes errors and what doesn't.
  EXPECT_TRUE(TryFetchResource(Encode(kTestDomain, RewriteDriver::kCssFilterId,
                                      "foobar", "a.css", "css")));

  // ... however fetches with invalid extensions should fail
  EXPECT_FALSE(
      TryFetchResource(Encode(kTestDomain, RewriteDriver::kCssFilterId, hash,
                              "a.css", "ext")));

  // Changing other fields can lead to error.
  GoogleString bad_url = Encode(kTestDomain, "xz", hash, "a.css", "css");

  EXPECT_FALSE(TryFetchResource(bad_url));
}

}  // namespace

}  // namespace net_instaweb
