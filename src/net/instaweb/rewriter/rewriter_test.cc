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

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"

#include "net/instaweb/rewriter/public/rewrite_driver.h"

namespace net_instaweb {

namespace {

class RewriterTest : public ResourceManagerTestBase {};

TEST_F(RewriterTest, AddHead) {
  AddFilter(RewriteOptions::kAddHead);
  ValidateExpected("add_head",
      "<body><p>text</p></body>",
      "<head/><body><p>text</p></body>");
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

TEST_F(RewriterTest, BaseTagNoHead) {
  AddFilter(RewriteOptions::kAddBaseTag);
  rewrite_driver_.SetBaseUrl("http://base");
  ValidateExpected("base_tag",
      "<body><p>text</p></body>",
      "<head><base href=\"http://base\"></head><body><p>text</p></body>");
}

TEST_F(RewriterTest, BaseTagExistingHead) {
  AddFilter(RewriteOptions::kAddBaseTag);
  rewrite_driver_.SetBaseUrl("http://base");
  ValidateExpected("base_tag",
      "<head><meta></head><body><p>text</p></body>",
      "<head><base href=\"http://base\"><meta></head><body><p>text</p></body>");
}

TEST_F(RewriterTest, BaseTagExistingHeadAndNonHrefBase) {
  AddFilter(RewriteOptions::kAddBaseTag);
  rewrite_driver_.SetBaseUrl("http://base");
  ValidateExpected("base_tag",
      "<head><base x><meta></head><body></body>",
      "<head><base href=\"http://base\"><base x><meta></head><body></body>");
}

TEST_F(RewriterTest, BaseTagExistingHeadAndHrefBase) {
  AddFilter(RewriteOptions::kAddBaseTag);
  rewrite_driver_.SetBaseUrl("http://base");
  ValidateExpected("base_tag",
      "<head><meta><base href=\"http://old\"></head><body></body>",
      "<head><base href=\"http://base\"><meta></head><body></body>");
}

TEST_F(RewriterTest, FailGracefullyOnInvalidUrls) {
  Hasher* hasher = &md5_hasher_;
  resource_manager_->set_hasher(hasher);
  AddFilter(RewriteOptions::kExtendCache);

  const char kCssData[] = "a { color: red }";
  InitMetaData("a.css", kContentTypeCss, kCssData, 100);

  // Fetching the real rewritten resource name should work.
  // TODO(sligocki): This will need to be regolded if naming format changes.
  std::string hash = hasher->Hash(kCssData);
  EXPECT_TRUE(
      TryFetchResource(Encode("http://test.com/", "ce", hash, "a.css", "css")));

  // Fetching variants should not cause system problems.
  // Changing hash still works.
  // Note: If any of these switch from true to false, that's probably fine.
  // We'd just like to keep track of what causes errors and what doesn't.
  EXPECT_TRUE(TryFetchResource(Encode("http://test.com/", "ce", "foobar",
                                      "a.css", "css")));
  EXPECT_TRUE(
      TryFetchResource(Encode("http://test.com/", "ce", hash, "a.css",
                              "ext")));

  // Changing other fields can lead to error.
  std::string bad_url = Encode("http://test.com/", "xz", hash, "a.css", "css");

  // Note that although we pass in a real value for 'request headers', we
  // null out the response and writer as those should never be called.
  SimpleMetaData request_headers;
  FetchCallback callback;
  bool fetched = rewrite_driver_.FetchResource(
      bad_url, request_headers, NULL, NULL, &message_handler_, &callback);
  EXPECT_FALSE(fetched);
  EXPECT_FALSE(callback.done());
}

}  // namespace

}  // namespace net_instaweb
