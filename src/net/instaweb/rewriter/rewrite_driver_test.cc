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

// Author: jmarantz@google.com (Joshua D. Marantz)

#include "net/instaweb/rewriter/public/rewrite_driver.h"

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"

namespace net_instaweb {

class RewriteDriverTest : public ResourceManagerTestBase {
 protected:
  RewriteDriverTest() {}

  bool CanDecodeUrl(const StringPiece& url) {
    RewriteFilter* filter;
    scoped_ptr<OutputResource> resource(
        rewrite_driver_.DecodeOutputResource(url, &filter));
    return (resource.get() != NULL);
  }

  std::string BaseUrlSpec() {
    return rewrite_driver_.base_url().Spec().as_string();
  }

  DISALLOW_COPY_AND_ASSIGN(RewriteDriverTest);
};

TEST_F(RewriteDriverTest, NoChanges) {
  ValidateNoChanges("no_changes",
                    "<head><script src=\"foo.js\"></script></head>"
                    "<body><form method=\"post\">"
                    "<input type=\"checkbox\" checked>"
                    "</form></body>");
}

TEST_F(RewriteDriverTest, TestLegacyUrl) {
  rewrite_driver_.AddFilters();
  EXPECT_FALSE(CanDecodeUrl("http://example.com/dir/123/jm.0.orig"))
      << "not enough dots";
  EXPECT_TRUE(CanDecodeUrl("http://example.com/dir/123/jm.0.orig.js"));
  EXPECT_TRUE(CanDecodeUrl(
      "http://x.com/dir/123/jm.0123456789abcdef0123456789ABCDEF.orig.js"));
  EXPECT_FALSE(CanDecodeUrl("http://example.com/dir/123/xx.0.orig.js"))
      << "invalid filter xx";
  ASSERT_FALSE(CanDecodeUrl("http://example.com/dir/123/jm.z.orig.js"))
      << "invalid hash code -- not hex";
  ASSERT_FALSE(CanDecodeUrl("http://example.com/dir/123/jm.ab.orig.js"))
      << "invalid hash code -- not 1 or 32 chars";
  ASSERT_FALSE(CanDecodeUrl("http://example.com/dir/123/jm.0.orig.x"))
      << "invalid extension";
}

TEST_F(RewriteDriverTest, TestModernUrl) {
  rewrite_driver_.AddFilters();

  // Sanity-check on a valid one
  EXPECT_TRUE(
      CanDecodeUrl("http://example.com/Puzzle.jpg.pagespeed.ce.HASH.jpg"));

  // Query is OK, too.
  EXPECT_TRUE(
      CanDecodeUrl("http://example.com/Puzzle.jpg.pagespeed.ce.HASH.jpg?s=ok"));

  // Invalid filter code
  EXPECT_FALSE(
      CanDecodeUrl("http://example.com/Puzzle.jpg.pagespeed.nf.HASH.jpg"));

  // Nonsense extension
  EXPECT_FALSE(
      CanDecodeUrl("http://example.com/Puzzle.jpg.pagespeed.ce.HASH.jpgif"));

  // No hash
  EXPECT_FALSE(
      CanDecodeUrl("http://example.com/Puzzle.jpg.pagespeed.ce..jpg"));
}

// Test to make sure we do not put in extra things into the cache.
// This is using the CSS rewriter, which caches the output.
TEST_F(RewriteDriverTest, TestCacheUse) {
  AddFilter(RewriteOptions::kRewriteCss);

  const char kCss[] = "* { display: none; }";
  const char kMinCss[] = "*{display:none}";
  InitResponseHeaders("a.css", kContentTypeCss, kCss, 100);

  std::string cssMinifiedUrl =
      Encode(kTestDomain, RewriteDriver::kCssFilterId,
             mock_hasher_.Hash(kMinCss), "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(cssMinifiedUrl));

  // We should have 3 things inserted:
  // 1) the source data
  // 2) the result
  // 3) the rname entry for the result
  int cold_num_inserts = lru_cache_->num_inserts();
  EXPECT_EQ(3, cold_num_inserts);

  // Warm load. This one should not change the number of inserts at all
  EXPECT_TRUE(TryFetchResource(cssMinifiedUrl));
  EXPECT_EQ(cold_num_inserts, lru_cache_->num_inserts());
  EXPECT_EQ(0, lru_cache_->num_identical_reinserts());
}

// Similar to the above, but with cache-extender which reconstructs on the fly.
TEST_F(RewriteDriverTest, TestCacheUseOnTheFly) {
  AddFilter(RewriteOptions::kExtendCache);

  const char kCss[] = "* { display: none; }";
  InitResponseHeaders("a.css", kContentTypeCss, kCss, 100);

  std::string cacheExtendedUrl =
      Encode(kTestDomain, RewriteDriver::kCacheExtenderId,
             mock_hasher_.Hash(kCss), "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(cacheExtendedUrl));

  // We should have 2 things inserted:
  // 1) the source data
  // 2) the rname entry for the result
  int cold_num_inserts = lru_cache_->num_inserts();
  EXPECT_EQ(2, cold_num_inserts);

  // Warm load. This one re-inserts in the rname entry, without changing it.
  EXPECT_TRUE(TryFetchResource(cacheExtendedUrl));
  EXPECT_EQ(cold_num_inserts, lru_cache_->num_inserts());
  EXPECT_EQ(1, lru_cache_->num_identical_reinserts());
}


TEST_F(RewriteDriverTest, BaseTags) {
  // Starting the parse, the base-tag will be derived from the html url.
  ASSERT_TRUE(rewrite_driver_.StartParse("http://example.com/index.html"));
  rewrite_driver_.Flush();
  EXPECT_EQ("http://example.com/", BaseUrlSpec());

  // If we then encounter a base tag, that will become the new base.
  rewrite_driver_.ParseText("<base href='http://new.example.com/subdir/'>");
  rewrite_driver_.Flush();
  EXPECT_EQ(0, message_handler_.TotalMessages());
  EXPECT_EQ("http://new.example.com/subdir/", BaseUrlSpec());

  // A second base tag will be ignored, and an info message will be printed.
  rewrite_driver_.ParseText("<base href='http://second.example.com/subdir2'>");
  rewrite_driver_.Flush();
  EXPECT_EQ(1, message_handler_.TotalMessages());
  EXPECT_EQ("http://new.example.com/subdir/", BaseUrlSpec());

  // Restart the parse with a new URL and we start fresh.
  rewrite_driver_.FinishParse();
  ASSERT_TRUE(rewrite_driver_.StartParse(
      "http://restart.example.com/index.html"));
  rewrite_driver_.Flush();
  EXPECT_EQ("http://restart.example.com/", BaseUrlSpec());

  // We should be able to reset again.
  rewrite_driver_.ParseText("<base href='http://new.example.com/subdir/'>");
  rewrite_driver_.Flush();
  EXPECT_EQ(1, message_handler_.TotalMessages());
  EXPECT_EQ("http://new.example.com/subdir/", BaseUrlSpec());
}

TEST_F(RewriteDriverTest, RelativeBaseTag) {
  // Starting the parse, the base-tag will be derived from the html url.
  ASSERT_TRUE(rewrite_driver_.StartParse("http://example.com/index.html"));
  rewrite_driver_.ParseText("<base href='subdir/'>");
  rewrite_driver_.Flush();
  EXPECT_EQ(0, message_handler_.TotalMessages());
  EXPECT_EQ("http://example.com/subdir/", BaseUrlSpec());
}

TEST_F(RewriteDriverTest, InvalidBaseTag) {
  // Encountering an invalid base tag should be ignored (except info message).
  ASSERT_TRUE(rewrite_driver_.StartParse("slwly://example.com/index.html"));
  rewrite_driver_.ParseText("<base href='subdir_not_allowed_on_slwly/'>");
  rewrite_driver_.Flush();

  EXPECT_EQ(1, message_handler_.TotalMessages());
  EXPECT_EQ("slwly://example.com/", BaseUrlSpec());

  // And we will accept a subsequent base-tag with legal aboslute syntax.
  rewrite_driver_.ParseText("<base href='http://example.com/absolute/'>");
  rewrite_driver_.Flush();
  EXPECT_EQ("http://example.com/absolute/", BaseUrlSpec());
}

TEST_F(RewriteDriverTest, CreateOutputResourceTooLong) {
  const ContentType* content_types[] = { NULL, &kContentTypeJpeg};
  const OutputResource::Kind resource_kinds[] = {
    OutputResource::kRewrittenResource,
    OutputResource::kOnTheFlyResource,
    OutputResource::kOutlinedResource,
  };

  // short_path.size() < options_.max_url_size() < long_path.size()
  std::string short_path = "http://www.example.com/dir/";
  std::string long_path = short_path;
  for (int i = 0; 2 * i < options_.max_url_size(); ++i) {
    long_path += "z/";
  }

  // short_name.size() < options_.max_url_segment_size() < long_name.size()
  std::string short_name = "foo.html";
  std::string long_name =
      StrCat("foo.html?",
             std::string(options_.max_url_segment_size() + 1, 'z'));

  std::string dummy_filter_id = "xy";

  scoped_ptr<OutputResource> resource;
  for (int t = 0; t < arraysize(content_types); ++t) {
    for (int k = 0; k < arraysize(resource_kinds); ++k) {
      // Short name should always succeed at creating new resource.
      resource.reset(rewrite_driver_.CreateOutputResourceWithPath(
          short_path, dummy_filter_id, short_name,
          content_types[t], resource_kinds[k]));
      EXPECT_TRUE(NULL != resource.get());

      // Long leaf-name should always fail at creating new resource.
      resource.reset(rewrite_driver_.CreateOutputResourceWithPath(
          short_path, dummy_filter_id, long_name,
          content_types[t], resource_kinds[k]));
      EXPECT_TRUE(NULL == resource.get());

      // Long total URL length should always fail at creating new resource.
      resource.reset(rewrite_driver_.CreateOutputResourceWithPath(
          long_path, dummy_filter_id, short_name,
          content_types[t], resource_kinds[k]));
      EXPECT_TRUE(NULL == resource.get());
    }
  }
}

}  // namespace net_instaweb
