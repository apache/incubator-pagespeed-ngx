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

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/mock_resource_callback.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"  // for ResourcePtr, etc
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class RewriteFilter;

class RewriteDriverTest : public ResourceManagerTestBase,
                          public ::testing::WithParamInterface<bool> {
 protected:
  RewriteDriverTest() {}

  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    SetAsynchronousRewrites(GetParam());
  }

  bool CanDecodeUrl(const StringPiece& url) {
    RewriteFilter* filter;
    OutputResourcePtr resource(
        rewrite_driver()->DecodeOutputResource(url, &filter));
    return (resource.get() != NULL);
  }

  const ContentType* DecodeContentType(const StringPiece& url) {
    const ContentType* type = NULL;
    RewriteFilter* filter;
    OutputResourcePtr output_resource(
        rewrite_driver()->DecodeOutputResource(url, &filter));
    if (output_resource.get() != NULL) {
      type = output_resource->type();
    }
    return type;
  }

  GoogleString BaseUrlSpec() {
    return rewrite_driver()->base_url().Spec().as_string();
  }

  DISALLOW_COPY_AND_ASSIGN(RewriteDriverTest);
};

TEST_P(RewriteDriverTest, NoChanges) {
  ValidateNoChanges("no_changes",
                    "<head><script src=\"foo.js\"></script></head>"
                    "<body><form method=\"post\">"
                    "<input type=\"checkbox\" checked>"
                    "</form></body>");
}

TEST_P(RewriteDriverTest, TestLegacyUrl) {
  rewrite_driver()->AddFilters();
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

TEST_P(RewriteDriverTest, TestInferContentType) {
  rewrite_driver()->AddFilters();
  SetBaseUrlForFetch("http://example.com/dir/123/index.html");
  EXPECT_TRUE(DecodeContentType("http://example.com/z.pagespeed.jm.0.unknown")
              == NULL);
  EXPECT_EQ(&kContentTypeJavascript,
            DecodeContentType("http://example.com/orig.pagespeed.jm.0.js"));
  EXPECT_EQ(&kContentTypeCss,
            DecodeContentType("http://example.com/orig.pagespeed.cf.0.css"));
  EXPECT_EQ(&kContentTypeJpeg,
            DecodeContentType("http://example.com/xorig.pagespeed.ic.0.jpg"));
  EXPECT_EQ(&kContentTypePng,
            DecodeContentType("http://example.com/orig.pagespeed.ce.0.png"));
  EXPECT_EQ(&kContentTypeGif,
            DecodeContentType("http://example.com/dir/xy.pagespeed.ic.0.gif"));
}

TEST_P(RewriteDriverTest, TestModernUrl) {
  rewrite_driver()->AddFilters();

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
TEST_P(RewriteDriverTest, TestCacheUse) {
  AddFilter(RewriteOptions::kRewriteCss);

  const char kCss[] = "* { display: none; }";
  const char kMinCss[] = "*{display:none}";
  InitResponseHeaders("a.css", kContentTypeCss, kCss, 100);

  GoogleString cssMinifiedUrl =
      Encode(kTestDomain, RewriteDriver::kCssFilterId,
             hasher()->Hash(kMinCss), "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(cssMinifiedUrl));

  // We should have 2 or 3 things inserted, depending on the mode:
  // 1) the source data
  // 2) the result
  // 3) the rname entry for the result --- if sync; in async case
  // we do not write out this mapping on resource reconstruction.
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(rewrite_driver()->asynchronous_rewrites() ? 2 : 3,
            cold_num_inserts);

  // Warm load. This one should not change the number of inserts at all
  EXPECT_TRUE(TryFetchResource(cssMinifiedUrl));
  EXPECT_EQ(cold_num_inserts, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
}

// Similar to the above, but with cache-extender which reconstructs on the fly.
TEST_P(RewriteDriverTest, TestCacheUseOnTheFly) {
  bool async = rewrite_driver()->asynchronous_rewrites();
  AddFilter(RewriteOptions::kExtendCache);

  const char kCss[] = "* { display: none; }";
  InitResponseHeaders("a.css", kContentTypeCss, kCss, 100);

  GoogleString cacheExtendedUrl =
      Encode(kTestDomain, RewriteDriver::kCacheExtenderId,
             hasher()->Hash(kCss), "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(cacheExtendedUrl));

  // We should have 1 or 2 things inserted:
  // 1) the source data
  // 2) the rname entry for the result (only in sync)
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(async ? 1 : 2, cold_num_inserts);

  // Warm load. In sync, this one re-inserts in the rname entry,
  // without changing it.
  EXPECT_TRUE(TryFetchResource(cacheExtendedUrl));
  EXPECT_EQ(cold_num_inserts, lru_cache()->num_inserts());
  EXPECT_EQ(async ? 0 : 1, lru_cache()->num_identical_reinserts());
}


TEST_P(RewriteDriverTest, BaseTags) {
  // Starting the parse, the base-tag will be derived from the html url.
  ASSERT_TRUE(rewrite_driver()->StartParse("http://example.com/index.html"));
  rewrite_driver()->Flush();
  EXPECT_EQ("http://example.com/index.html", BaseUrlSpec());

  // If we then encounter a base tag, that will become the new base.
  rewrite_driver()->ParseText("<base href='http://new.example.com/subdir/'>");
  rewrite_driver()->Flush();
  EXPECT_EQ(0, message_handler_.TotalMessages());
  EXPECT_EQ("http://new.example.com/subdir/", BaseUrlSpec());

  // A second base tag will be ignored, and an info message will be printed.
  rewrite_driver()->ParseText("<base href=http://second.example.com/subdir2>");
  rewrite_driver()->Flush();
  EXPECT_EQ(1, message_handler_.TotalMessages());
  EXPECT_EQ("http://new.example.com/subdir/", BaseUrlSpec());

  // Restart the parse with a new URL and we start fresh.
  rewrite_driver()->FinishParse();
  ASSERT_TRUE(rewrite_driver()->StartParse(
      "http://restart.example.com/index.html"));
  rewrite_driver()->Flush();
  EXPECT_EQ("http://restart.example.com/index.html", BaseUrlSpec());

  // We should be able to reset again.
  rewrite_driver()->ParseText("<base href='http://new.example.com/subdir/'>");
  rewrite_driver()->Flush();
  EXPECT_EQ(1, message_handler_.TotalMessages());
  EXPECT_EQ("http://new.example.com/subdir/", BaseUrlSpec());
}

TEST_P(RewriteDriverTest, RelativeBaseTag) {
  // Starting the parse, the base-tag will be derived from the html url.
  ASSERT_TRUE(rewrite_driver()->StartParse("http://example.com/index.html"));
  rewrite_driver()->ParseText("<base href='subdir/'>");
  rewrite_driver()->Flush();
  EXPECT_EQ(0, message_handler_.TotalMessages());
  EXPECT_EQ("http://example.com/subdir/", BaseUrlSpec());
}

TEST_P(RewriteDriverTest, InvalidBaseTag) {
  // Encountering an invalid base tag should be ignored (except info message).
  ASSERT_TRUE(rewrite_driver()->StartParse("slwly://example.com/index.html"));
  rewrite_driver()->ParseText("<base href='subdir_not_allowed_on_slwly/'>");
  rewrite_driver()->Flush();

  EXPECT_EQ(1, message_handler_.TotalMessages());
  EXPECT_EQ("slwly://example.com/index.html", BaseUrlSpec());

  // And we will accept a subsequent base-tag with legal aboslute syntax.
  rewrite_driver()->ParseText("<base href='http://example.com/absolute/'>");
  rewrite_driver()->Flush();
  EXPECT_EQ("http://example.com/absolute/", BaseUrlSpec());
}

TEST_P(RewriteDriverTest, CreateOutputResourceTooLong) {
  const ContentType* content_types[] = { NULL, &kContentTypeJpeg};
  const OutputResourceKind resource_kinds[] = {
    kRewrittenResource,
    kOnTheFlyResource,
    kOutlinedResource,
  };

  // short_path.size() < options()->max_url_size() < long_path.size()
  GoogleString short_path = "http://www.example.com/dir/";
  GoogleString long_path = short_path;
  for (int i = 0; 2 * i < options()->max_url_size(); ++i) {
    long_path += "z/";
  }

  // short_name.size() < options()->max_url_segment_size() < long_name.size()
  GoogleString short_name = "foo.html";
  GoogleString long_name =
      StrCat("foo.html?",
             GoogleString(options()->max_url_segment_size() + 1, 'z'));

  GoogleString dummy_filter_id = "xy";

  OutputResourcePtr resource;
  for (int t = 0; t < arraysize(content_types); ++t) {
    for (int k = 0; k < arraysize(resource_kinds); ++k) {
      for (int use_async_flow = 0; use_async_flow < 2; ++use_async_flow) {
        // Short name should always succeed at creating new resource.
        resource.reset(rewrite_driver()->CreateOutputResourceWithPath(
            short_path, dummy_filter_id, short_name,
            content_types[t], resource_kinds[k], use_async_flow != 0));
        EXPECT_TRUE(NULL != resource.get());

        // Long leaf-name should always fail at creating new resource.
        resource.reset(rewrite_driver()->CreateOutputResourceWithPath(
            short_path, dummy_filter_id, long_name,
            content_types[t], resource_kinds[k], use_async_flow != 0));
        EXPECT_TRUE(NULL == resource.get());

        // Long total URL length should always fail at creating new resource.
        resource.reset(rewrite_driver()->CreateOutputResourceWithPath(
            long_path, dummy_filter_id, short_name,
            content_types[t], resource_kinds[k], use_async_flow != 0));
        EXPECT_TRUE(NULL == resource.get());
      }
    }
  }
}

TEST_P(RewriteDriverTest, MultipleDomains) {
  // Make sure we authorize domains for resources properly. This is a regression
  // test for where loading things from a domain would prevent loads from an
  // another domain from the same RewriteDriver.

  const char kCss[] = "* { display: none; }";
  const char kAltDomain[] = "http://www.example.co.uk/";
  InitResponseHeaders(StrCat(kTestDomain, "a.css"), kContentTypeCss, kCss, 100);
  InitResponseHeaders(StrCat(kAltDomain, "b.css"), kContentTypeCss, kCss, 100);

  GoogleString rewritten1 = Encode(kTestDomain, RewriteDriver::kCacheExtenderId,
                                   hasher()->Hash(kCss), "a.css", "css");

  GoogleString rewritten2 = Encode(kAltDomain, RewriteDriver::kCacheExtenderId,
                                   hasher()->Hash(kCss), "b.css", "css");

  EXPECT_TRUE(TryFetchResource(rewritten1));
  rewrite_driver()->Clear();
  EXPECT_TRUE(TryFetchResource(rewritten2));
}

// Test caching behavior for normal UrlInputResources.
// This is the base case that LoadResourcesFromFiles below contrasts with.
TEST_P(RewriteDriverTest, LoadResourcesFromTheWeb) {
  const char kStaticUrlPrefix[] = "http://www.example.com/";
  const char kResourceName[ ]= "foo.css";
  GoogleString resource_url = StrCat(kStaticUrlPrefix, kResourceName);
  const char kResourceContents1[] = "body { background: red; }";
  const char kResourceContents2[] = "body { background: blue; }";
  ResponseHeaders resource_headers;
  // This sets 1 year cache lifetime :/ TODO(sligocki): Shorten this.
  SetDefaultLongCacheHeaders(&kContentTypeCss, &resource_headers);

  // Set the fetch value.
  SetFetchResponse(resource_url, resource_headers, kResourceContents1);
  // Make sure file can be loaded. Note this cannot be loaded through the
  // mock_url_fetcher, because it has not been set in that fetcher.
  ResourcePtr resource(
      rewrite_driver()->CreateInputResourceAbsoluteUnchecked(resource_url));
  MockResourceCallback mock_callback(resource);
  EXPECT_TRUE(resource.get() != NULL);
  resource_manager()->ReadAsync(&mock_callback);
  EXPECT_TRUE(mock_callback.done());
  EXPECT_TRUE(mock_callback.success());
  EXPECT_EQ(kResourceContents1, resource->contents());
  // TODO(sligocki): Check it was cached.

  // Change the fetch value.
  SetFetchResponse(resource_url, resource_headers, kResourceContents2);
  // Check that the resource loads cached.
  ResourcePtr resource2(
      rewrite_driver()->CreateInputResourceAbsoluteUnchecked(resource_url));
  MockResourceCallback mock_callback2(resource2);
  EXPECT_TRUE(resource2.get() != NULL);
  resource_manager()->ReadAsync(&mock_callback2);
  EXPECT_TRUE(mock_callback2.done());
  EXPECT_TRUE(mock_callback2.success());
  EXPECT_EQ(kResourceContents1, resource2->contents());

  // Advance timer and check that the resource loads updated.
  mock_timer()->AdvanceMs(10 * Timer::kYearMs);

  // Check that the resource loads updated.
  ResourcePtr resource3(
      rewrite_driver()->CreateInputResourceAbsoluteUnchecked(resource_url));
  MockResourceCallback mock_callback3(resource3);
  EXPECT_TRUE(resource3.get() != NULL);
  resource_manager()->ReadAsync(&mock_callback3);
  EXPECT_TRUE(mock_callback3.done());
  EXPECT_EQ(kResourceContents2, resource3->contents());
}

// Test that we successfully load specified resources from files and that
// file resources have the appropriate properties, such as being loaded from
// file every time they are fetched (not being cached).
TEST_P(RewriteDriverTest, LoadResourcesFromFiles) {
  const char kStaticUrlPrefix[] = "http://www.example.com/static/";
  const char kStaticFilenamePrefix[] = "/htmlcontent/static/";
  const char kResourceName[ ]= "foo.css";
  GoogleString resource_filename = StrCat(kStaticFilenamePrefix, kResourceName);
  GoogleString resource_url = StrCat(kStaticUrlPrefix, kResourceName);
  const char kResourceContents1[] = "body { background: red; }";
  const char kResourceContents2[] = "body { background: blue; }";

  // Tell RewriteDriver to associate static URLs with filenames.
  options()->file_load_policy()->Associate(kStaticUrlPrefix,
                                           kStaticFilenamePrefix);

  // Write a file.
  WriteFile(resource_filename.c_str(), kResourceContents1);
  // Make sure file can be loaded. Note this cannot be loaded through the
  // mock_url_fetcher, because it has not been set in that fetcher.
  ResourcePtr resource(
      rewrite_driver()->CreateInputResourceAbsoluteUnchecked(resource_url));
  MockResourceCallback mock_callback(resource);
  EXPECT_TRUE(resource.get() != NULL);
  resource_manager()->ReadAsync(&mock_callback);
  EXPECT_TRUE(mock_callback.done());
  EXPECT_TRUE(mock_callback.success());
  EXPECT_EQ(kResourceContents1, resource->contents());
  // TODO(sligocki): Check it wasn't cached.

  // Change the file.
  WriteFile(resource_filename.c_str(), kResourceContents2);
  // Make sure the resource loads updated.
  ResourcePtr resource2(
      rewrite_driver()->CreateInputResourceAbsoluteUnchecked(resource_url));
  MockResourceCallback mock_callback2(resource2);
  EXPECT_TRUE(resource2.get() != NULL);
  resource_manager()->ReadAsync(&mock_callback2);
  EXPECT_TRUE(mock_callback2.done());
  EXPECT_TRUE(mock_callback2.success());
  EXPECT_EQ(kResourceContents2, resource2->contents());
}

TEST_P(RewriteDriverTest, ResolveAnchorUrl) {
  ASSERT_TRUE(rewrite_driver()->StartParse("http://example.com/index.html"));
  GoogleUrl resolved(rewrite_driver()->base_url(), "#anchor");
  EXPECT_EQ("http://example.com/index.html#anchor", resolved.Spec());
  rewrite_driver()->FinishParse();
}

// We test with asynchronous_rewrites() == GetParam() as both true and false.
INSTANTIATE_TEST_CASE_P(RewriteDriverTestInstance,
                        RewriteDriverTest,
                        ::testing::Bool());

}  // namespace net_instaweb
