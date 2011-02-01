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

// Unit-test the cache extender.


#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/cache_extender.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

namespace {

const char kHtmlFormat[] =
    "<link rel='stylesheet' href='%s' type='text/css'>\n"
    "<img src='%s'/>\n"
    "<script type='text/javascript' src='%s'></script>\n";

const char kCssData[] = ".blue {color: blue;}";
const char kImageData[] = "Invalid JPEG but it does not matter for this test";
const char kJsData[] = "alert('hello, world!')";
const char kFilterId[] = "ce";

class CacheExtenderTest : public ResourceManagerTestBase {
 protected:

  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
  }

  void InitTest(int64 ttl) {
    AddFilter(RewriteOptions::kExtendCache);
    InitResponseHeaders("a.css", kContentTypeCss, kCssData, ttl);
    InitResponseHeaders("b.jpg", kContentTypeJpeg, kImageData, ttl);
    InitResponseHeaders("c.js", kContentTypeJavascript, kJsData, ttl);
  }

  // Generate HTML loading 3 resources with the specified URLs
  std::string GenerateHtml(const std::string& a,
                            const std::string& b,
                            const std::string& c) {
    return StringPrintf(kHtmlFormat, a.c_str(), b.c_str(), c.c_str());
  }

  // Helper to test for how we handle trailing junk in URLs
  void TestCorruptUrl(const char* junk, bool should_fetch_ok) {
    InitTest(100);
    std::string a_ext = Encode(kTestDomain, "ce", "0", "a.css", "css");
    std::string b_ext = Encode(kTestDomain, "ce", "0", "b.jpg", "jpg");
    std::string c_ext = Encode(kTestDomain, "ce", "0", "c.js", "js");

    ValidateExpected("no_ext_corrupt", GenerateHtml("a.css", "b.jpg", "c.js"),
                    GenerateHtml(a_ext, b_ext, c_ext));
    std::string output;
    EXPECT_EQ(should_fetch_ok, ServeResourceUrl(StrCat(a_ext, junk), &output));
    EXPECT_EQ(should_fetch_ok, ServeResourceUrl(StrCat(b_ext, junk), &output));
    EXPECT_EQ(should_fetch_ok, ServeResourceUrl(StrCat(c_ext, junk), &output));
    ValidateExpected("no_ext_corrupt", GenerateHtml("a.css", "b.jpg", "c.js"),
                    GenerateHtml(a_ext, b_ext, c_ext));
  }
};

TEST_F(CacheExtenderTest, DoExtend) {
  InitTest(100);
  for (int i = 0; i < 3; i++) {
    ValidateExpected(
        "do_extend",
        GenerateHtml("a.css", "b.jpg", "c.js"),
        GenerateHtml(
            Encode(kTestDomain, "ce", "0", "a.css", "css"),
            Encode(kTestDomain, "ce", "0", "b.jpg", "jpg"),
            Encode(kTestDomain, "ce", "0", "c.js", "js")));
  }
}

TEST_F(CacheExtenderTest, NoInputResource) {
  InitTest(100);
  // Test for not crashing on bad/disallowed URL.
  ValidateNoChanges("bad url",
                    GenerateHtml("swly://example.com/a.css",
                                 "http://evil.com/b.jpg",
                                 "http://moreevil.com/c.js"));
}

TEST_F(CacheExtenderTest, NoExtendAlreadyCachedProperly) {
  InitTest(100000000);  // cached for a long time to begin with
  ValidateNoChanges("no_extend_cached_properly",
                    GenerateHtml("a.css", "b.jpg", "c.js"));
}

TEST_F(CacheExtenderTest, ExtendIfSharded) {
  InitTest(100000000);  // cached for a long time to begin with
  EXPECT_TRUE(options_.domain_lawyer()->AddShard(
      "test.com", "shard0.com,shard1.com", &message_handler_));
  // shard0 is always selected in the test because of our mock hasher
  // that always returns 0.
  ValidateExpected("extend_if_sharded",
                   GenerateHtml("a.css", "b.jpg", "c.js"),
                   GenerateHtml("http://shard0.com/a.css.pagespeed.ce.0.css",
                                "http://shard0.com/b.jpg.pagespeed.ce.0.jpg",
                                "http://shard0.com/c.js.pagespeed.ce.0.js"));
}

TEST_F(CacheExtenderTest, ExtendIfRewritten) {
  InitTest(100000000);  // cached for a long time to begin with

  EXPECT_TRUE(options_.domain_lawyer()->AddRewriteDomainMapping(
      "cdn.com", "test.com", &message_handler_));
  ValidateExpected("extend_if_rewritten",
                   GenerateHtml("a.css", "b.jpg", "c.js"),
                   GenerateHtml("http://cdn.com/a.css.pagespeed.ce.0.css",
                                "http://cdn.com/b.jpg.pagespeed.ce.0.jpg",
                                "http://cdn.com/c.js.pagespeed.ce.0.js"));
}

TEST_F(CacheExtenderTest, ExtendIfShardedAndRewritten) {
  InitTest(100000000);  // cached for a long time to begin with

  EXPECT_TRUE(options_.domain_lawyer()->AddRewriteDomainMapping(
      "cdn.com", "test.com", &message_handler_));

  // Domain-rewriting is performed first.  Then we shard.
  EXPECT_TRUE(options_.domain_lawyer()->AddShard(
      "cdn.com", "shard0.com,shard1.com", &message_handler_));
  // shard0 is always selected in the test because of our mock hasher
  // that always returns 0.
  ValidateExpected("extend_if_sharded_and_rewritten",
                   GenerateHtml("a.css", "b.jpg", "c.js"),
                   GenerateHtml("http://shard0.com/a.css.pagespeed.ce.0.css",
                                "http://shard0.com/b.jpg.pagespeed.ce.0.jpg",
                                "http://shard0.com/c.js.pagespeed.ce.0.js"));
}

// TODO(jmarantz): consider implementing and testing the sharding and
// domain-rewriting of uncacheable resources -- just don't sign the URLs.

TEST_F(CacheExtenderTest, NoExtendOriginUncacheable) {
  InitTest(0);  // origin not cacheable
  ValidateNoChanges("no_extend_origin_not_cacheable",
                    GenerateHtml("a.css", "b.jpg", "c.js"));
}

TEST_F(CacheExtenderTest, ServeFiles) {
  std::string content;

  InitTest(100);
  ASSERT_TRUE(ServeResource(kTestDomain, kFilterId, "a.css", "css", &content));
  EXPECT_EQ(std::string(kCssData), content);
  ASSERT_TRUE(ServeResource(kTestDomain, kFilterId, "b.jpg", "jpg", &content));
  EXPECT_EQ(std::string(kImageData), content);
  ASSERT_TRUE(ServeResource(kTestDomain, kFilterId, "c.js", "js", &content));
  EXPECT_EQ(std::string(kJsData), content);
}

TEST_F(CacheExtenderTest, ServeFilesFromDelayedFetch) {
  InitTest(100);
  ServeResourceFromManyContexts(Encode(kTestDomain, "ce", "0", "a.css", "css"),
                                RewriteOptions::kExtendCache,
                                &mock_hasher_, kCssData);
  ServeResourceFromManyContexts(Encode(kTestDomain, "ce", "0", "b.jpg", "jpg"),
                                RewriteOptions::kExtendCache,
                                &mock_hasher_, kImageData);
  ServeResourceFromManyContexts(Encode(kTestDomain, "ce", "0", "c.js", "js"),
                                RewriteOptions::kExtendCache,
                                &mock_hasher_, kJsData);

  // TODO(jmarantz): make ServeResourceFromManyContexts check:
  //  1. Gets the data from the cache, with no mock fetchers, null file system
  //  2. Gets the data from the file system, with no cache, no mock fetchers.
  //  3. Gets the data from the mock fetchers: no cache, no file system.
}

TEST_F(CacheExtenderTest, MinimizeCacheHits) {
  options_.EnableFilter(RewriteOptions::kOutlineCss);
  options_.EnableFilter(RewriteOptions::kExtendCache);
  options_.set_css_outline_min_bytes(1);
  rewrite_driver_.AddFilters();
  std::string html_input = StrCat("<style>", kCssData, "</style>");
  std::string html_output = StringPrintf(
      "<link rel='stylesheet' href='%s'>",
      Encode(kTestDomain, "co", "0", "_", "css").c_str());
  ValidateExpected("no_extend_origin_not_cacheable", html_input, html_output);

  // The key thing about this test is that the CacheExtendFilter should
  // not pound the cache looking to see if it's already rewritten this
  // resource.  If we try, in the cache extend filter, to this already-optimized
  // resource from the cache, then we'll get a cache-hit and decide that
  // it's already got a long cache lifetime.  But we should know, just from
  // the name of the resource, that it should not be cache extended.
  EXPECT_EQ(0, lru_cache_->num_hits());
  EXPECT_EQ(1, lru_cache_->num_misses());
}

TEST_F(CacheExtenderTest, NoExtensionCorruption) {
  TestCorruptUrl("%22", false);
}

TEST_F(CacheExtenderTest, NoQueryCorruption) {
  TestCorruptUrl("?query", true);
}

}  // namespace

}  // namespace net_instaweb
