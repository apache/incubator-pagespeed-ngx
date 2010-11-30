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

static const char kDomain[] = "http://test.com/";

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
    InitMetaData("a.css", kContentTypeCss, kCssData, ttl);
    InitMetaData("b.jpg", kContentTypeJpeg, kImageData, ttl);
    InitMetaData("c.js", kContentTypeJavascript, kJsData, ttl);
  }

  // Generate HTML loading 3 resources with the specified URLs
  std::string GenerateHtml(const std::string& a,
                            const std::string& b,
                            const std::string& c) {
    return StringPrintf(kHtmlFormat, a.c_str(), b.c_str(), c.c_str());
  }
};

TEST_F(CacheExtenderTest, DoExtend) {
  InitTest(100);
  for (int i = 0; i < 3; i++) {
    ValidateExpected(
        "do_extend",
        GenerateHtml("a.css", "b.jpg", "c.js"),
        GenerateHtml(
            Encode(kDomain, "ce", "0", "a.css", "css"),
            Encode(kDomain, "ce", "0", "b.jpg", "jpg"),
            Encode(kDomain, "ce", "0", "c.js", "js")));
  }
}

TEST_F(CacheExtenderTest, NoExtendAlreadyCachedProperly) {
  InitTest(100000000);  // cached for a long time to begin with
  ValidateExpected("no_extend_cached_properly",
                   GenerateHtml("a.css", "b.jpg", "c.js"),
                   GenerateHtml("a.css", "b.jpg", "c.js"));
}

TEST_F(CacheExtenderTest, NoExtendOriginUncacheable) {
  InitTest(0);  // origin not cacheable
  ValidateExpected("no_extend_origin_not_cacheable",
                   GenerateHtml("a.css", "b.jpg", "c.js"),
                   GenerateHtml("a.css", "b.jpg", "c.js"));
}

TEST_F(CacheExtenderTest, ServeFiles) {
  std::string content;

  InitTest(100);
  ASSERT_TRUE(ServeResource(kDomain, kFilterId, "a.css", "css", &content));
  EXPECT_EQ(std::string(kCssData), content);
  ASSERT_TRUE(ServeResource(kDomain, kFilterId, "b.jpg", "jpg", &content));
  EXPECT_EQ(std::string(kImageData), content);
  ASSERT_TRUE(ServeResource(kDomain, kFilterId, "c.js", "js", &content));
  EXPECT_EQ(std::string(kJsData), content);
}

TEST_F(CacheExtenderTest, ServeFilesFromDelayedFetch) {
  InitTest(100);
  ServeResourceFromManyContexts(Encode(kDomain, "ce", "0", "a.css", "css"),
                                RewriteOptions::kExtendCache,
                                &mock_hasher_, kCssData);
  ServeResourceFromManyContexts(Encode(kDomain, "ce", "0", "b.jpg", "jpg"),
                                RewriteOptions::kExtendCache,
                                &mock_hasher_, kImageData);
  ServeResourceFromManyContexts(Encode(kDomain, "ce", "0", "c.js", "js"),
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
      Encode(kDomain, "co", "0", "_", "css").c_str());
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
