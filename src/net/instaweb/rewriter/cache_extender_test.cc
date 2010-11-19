/**
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

#define DOMAIN "http://test.com/"

const char kHtmlFormat[] =
    "<link rel='stylesheet' href='%s.css' type='text/css'>\n"
    "<img src='%s.jpg'/>\n"
    "<script type='text/javascript' src='%s.js'></script>\n";

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
    rewrite_driver_.AddFilter(RewriteOptions::kExtendCache);
    InitMetaData("a.css", kContentTypeCss, kCssData, ttl);
    InitMetaData("b.jpg", kContentTypeJpeg, kImageData, ttl);
    InitMetaData("c.js", kContentTypeJavascript, kJsData, ttl);
  }

  // Generate HTML loading 3 resources with the specified URLs
  std::string GenerateHtml(const char* a, const char* b, const char* c) {
    return StringPrintf(kHtmlFormat, a, b, c);
  }
};

TEST_F(CacheExtenderTest, DoExtend) {
  InitTest(100);
  for (int i = 0; i < 3; i++) {
    ValidateExpected("do_extend",
                     GenerateHtml("a", "b", "c").c_str(),
                     GenerateHtml(DOMAIN "ce.0.a,s",
                                  DOMAIN "ce.0.b,j",
                                  DOMAIN "ce.0.c,l").c_str());
  }
}

TEST_F(CacheExtenderTest, NoExtendAlreadyCachedProperly) {
  InitTest(100000000);  // cached for a long time to begin with
  ValidateExpected("no_extend_cached_properly",
                   GenerateHtml("a", "b", "c").c_str(),
                   GenerateHtml("a", "b", "c").c_str());
}

TEST_F(CacheExtenderTest, NoExtendOriginUncacheable) {
  InitTest(0);  // origin not cacheable
  ValidateExpected("no_extend_origin_not_cacheable",
                   GenerateHtml("a", "b", "c").c_str(),
                   GenerateHtml("a", "b", "c").c_str());
}

TEST_F(CacheExtenderTest, ServeFiles) {
  std::string content;

  InitTest(100);
  ASSERT_TRUE(ServeResource(DOMAIN, kFilterId, "a,s", "css", &content));
  EXPECT_EQ(std::string(kCssData), content);
  ASSERT_TRUE(ServeResource(DOMAIN, kFilterId, "b,j", "jpg", &content));
  EXPECT_EQ(std::string(kImageData), content);
  ASSERT_TRUE(ServeResource(DOMAIN, kFilterId, "c,l", "js", &content));
  EXPECT_EQ(std::string(kJsData), content);

  // TODO(jmarantz): make 3 variations of this test:
  //  1. Gets the data from the cache, with no mock fetchers, null file system
  //  2. Gets the data from the file system, with no cache, no mock fetchers.
  //  3. Gets the data from the mock fetchers: no cache, no file system.
}

TEST_F(CacheExtenderTest, ServeFilesFromDelayedFetch) {
  InitTest(100);
  ServeResourceFromManyContexts(DOMAIN "ce.0.a,s.css",
                                RewriteOptions::kExtendCache,
                                &mock_hasher_, kCssData);
  ServeResourceFromManyContexts(DOMAIN "ce.0.b,j.jpg",
                                RewriteOptions::kExtendCache,
                                &mock_hasher_, kImageData);
  ServeResourceFromManyContexts(DOMAIN "ce.0.c,l.js",
                                RewriteOptions::kExtendCache,
                                &mock_hasher_, kJsData);
}

TEST_F(CacheExtenderTest, MinimizeCacheHits) {
  RewriteOptions options;
  options.EnableFilter(RewriteOptions::kOutlineCss);
  options.EnableFilter(RewriteOptions::kExtendCache);
  options.set_css_outline_min_bytes(1);
  rewrite_driver_.AddFilters(options);
  std::string html_input = StrCat("<style>", kCssData, "</style>");
  const char html_output[] =
      "<link rel='stylesheet' href='http://test.com/co.0._.css'>";
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

}  // namespace

}  // namespace net_instaweb
