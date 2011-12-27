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

#include "net/instaweb/htmlparse/html_event.h"
#include "net/instaweb/htmlparse/html_testing_peer.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/fake_url_async_fetcher.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/mock_resource_callback.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"  // for ResourcePtr, etc
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/test_url_namer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_scheduler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/scheduler.h"
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
  }

  bool CanDecodeUrl(const StringPiece& url) {
    GoogleUrl gurl(url);
    RewriteFilter* filter;
    OutputResourcePtr resource(
        rewrite_driver()->DecodeOutputResource(gurl, &filter));
    return (resource.get() != NULL);
  }

  const ContentType* DecodeContentType(const StringPiece& url) {
    GoogleUrl gurl(url);
    const ContentType* type = NULL;
    RewriteFilter* filter;
    OutputResourcePtr output_resource(
        rewrite_driver()->DecodeOutputResource(gurl, &filter));
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
  EXPECT_TRUE(DecodeContentType(
      Encode("http://example.com/", "jm", "0", "z", "unknown")) == NULL);
  EXPECT_EQ(&kContentTypeJavascript,
            DecodeContentType(
                Encode("http://example.com/", "jm", "0", "orig", "js")));
  EXPECT_EQ(&kContentTypeCss,
            DecodeContentType(
                Encode("http://example.com/", "cf", "0", "orig", "css")));
  EXPECT_EQ(&kContentTypeJpeg,
            DecodeContentType(
                Encode("http://example.com/", "ic", "0", "orig", "jpg")));
  EXPECT_EQ(&kContentTypePng,
            DecodeContentType(
                Encode("http://example.com/", "ce", "0", "orig", "png")));
  EXPECT_EQ(&kContentTypeGif,
            DecodeContentType(
                Encode("http://example.com/dir/", "ic", "0", "xy", "gif")));
}

TEST_P(RewriteDriverTest, TestModernUrl) {
  rewrite_driver()->AddFilters();

  // Sanity-check on a valid one
  EXPECT_TRUE(CanDecodeUrl(
      Encode("http://example.com/", "ce", "HASH", "Puzzle.jpg", "jpg")));

  // Query is OK, too.
  EXPECT_TRUE(CanDecodeUrl(
      StrCat(Encode("http://example.com/", "ce", "HASH", "Puzzle.jpg", "jpg"),
             "?s=ok")));

  // Invalid filter code
  EXPECT_FALSE(CanDecodeUrl(
      Encode("http://example.com/", "NOFILTER", "HASH", "Puzzle.jpg", "jpg")));

  // Nonsense extension
  EXPECT_FALSE(CanDecodeUrl(
      Encode("http://example.com/", "ce", "HASH", "Puzzle.jpg", "jpgif")));

  // No hash
  GoogleString encoded_url(Encode("http://example.com/", "ce", "123456789",
                                  "Puzzle.jpg", "jpg"));
  GlobalReplaceSubstring("123456789", "", &encoded_url);
  EXPECT_FALSE(CanDecodeUrl(encoded_url));
}

class RewriteDriverTestUrlNamer : public RewriteDriverTest {
 public:
  RewriteDriverTestUrlNamer() {
    SetUseTestUrlNamer(true);
  }
};

TEST_P(RewriteDriverTestUrlNamer, TestEncodedUrls) {
  rewrite_driver()->AddFilters();

  // Sanity-check on a valid one
  EXPECT_TRUE(CanDecodeUrl(
      Encode("http://example.com/", "ce", "HASH", "Puzzle.jpg", "jpg")));

  // Query is OK, too.
  EXPECT_TRUE(CanDecodeUrl(
      StrCat(Encode("http://example.com/", "ce", "HASH", "Puzzle.jpg", "jpg"),
             "?s=ok")));

  // Invalid filter code
  EXPECT_FALSE(CanDecodeUrl(
      Encode("http://example.com/", "NOFILTER", "HASH", "Puzzle.jpg", "jpg")));

  // Nonsense extension
  EXPECT_FALSE(CanDecodeUrl(
      Encode("http://example.com/", "ce", "HASH", "Puzzle.jpg", "jpgif")));

  // No hash
  GoogleString encoded_url(Encode("http://example.com/", "ce", "123456789",
                                  "Puzzle.jpg", "jpg"));
  GlobalReplaceSubstring("123456789", "", &encoded_url);
  EXPECT_FALSE(CanDecodeUrl(encoded_url));

  // Valid proxy domain but invalid decoded URL.
  encoded_url = Encode("http://example.com/", "ce", "0", "Puzzle.jpg", "jpg");
  GlobalReplaceSubstring("example.com/",
                         "example.comWYTHQ000JRJFCAAKYU1EMA6VUBDTS4DESLRWIPMS"
                         "KKMQH0XYN1FURDBBSQ9AYXVX3TZDKZEIJNLRHU05ATHBAWWAG2+"
                         "ADDCXPWGGP1VTHJIYU13IIFQYSYMGKIMSFIEBM+HCAACVNGO8CX"
                         "XO%81%9F%F1m/", &encoded_url);
  // By default TestUrlNamer doesn't proxy but we need it to for this test.
  TestUrlNamer::SetProxyMode(true);
  EXPECT_FALSE(CanDecodeUrl(encoded_url));
}

// Test to make sure we do not put in extra things into the cache.
// This is using the CSS rewriter, which caches the output.
TEST_P(RewriteDriverTest, TestCacheUse) {
  AddFilter(RewriteOptions::kRewriteCss);

  const char kCss[] = "* { display: none; }";
  const char kMinCss[] = "*{display:none}";
  InitResponseHeaders("a.css", kContentTypeCss, kCss, 100);

  GoogleString css_minified_url =
      Encode(kTestDomain, RewriteOptions::kCssFilterId,
             hasher()->Hash(kMinCss), "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(css_minified_url));

  // We should have 3 things inserted:
  // 1) the source data
  // 2) the result
  // 3) the rname entry for the result
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(3, cold_num_inserts);

  // Warm load. This one should not change the number of inserts at all
  EXPECT_TRUE(TryFetchResource(css_minified_url));
  EXPECT_EQ(cold_num_inserts, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
}

// Extension of above with cache invalidation.
TEST_P(RewriteDriverTest, TestCacheUseWithInvalidation) {
  resource_manager()->set_store_outputs_in_file_system(false);
  AddFilter(RewriteOptions::kRewriteCss);

  const char kCss[] = "* { display: none; }";
  const char kMinCss[] = "*{display:none}";
  InitResponseHeaders("a.css", kContentTypeCss, kCss, 100);

  GoogleString css_minified_url =
      Encode(kTestDomain, RewriteOptions::kCssFilterId,
             hasher()->Hash(kMinCss), "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(css_minified_url));

  // We should have 3 things inserted:
  // 1) the source data
  // 2) the result
  // 3) the rname entry for the result.
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(3, cold_num_inserts);

  // Warm load. This one should not change the number of inserts at all
  EXPECT_TRUE(TryFetchResource(css_minified_url));
  EXPECT_EQ(cold_num_inserts, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

  // Set cache invalidation timestamp (to now, so that response date header is
  // in the "past") and load. Should get inserted again.
  ClearStats();
  int64 now_ms = mock_timer()->NowMs();
  options()->ClearSignatureForTesting();
  options()->set_cache_invalidation_timestamp(now_ms);
  options()->ComputeSignature(hasher());
  EXPECT_TRUE(TryFetchResource(css_minified_url));
  // We expect: identical input a new rname entry (its version # changed),
  // and the output which may not may not auto-advance due to MockTimer
  // black magic.
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(2, lru_cache()->num_identical_reinserts());
}

// Similar to TestCacheUse, but with cache-extender which reconstructs on the
// fly.
TEST_P(RewriteDriverTest, TestCacheUseOnTheFly) {
  AddFilter(RewriteOptions::kExtendCacheCss);

  const char kCss[] = "* { display: none; }";
  InitResponseHeaders("a.css", kContentTypeCss, kCss, 100);

  GoogleString cache_extended_url =
      Encode(kTestDomain, RewriteOptions::kCacheExtenderId,
             hasher()->Hash(kCss), "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(cache_extended_url));

  // We should have 2 things inserted:
  // 1) the source data
  // 2) the rname entry for the result (only in sync)
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(2, cold_num_inserts);

  // Warm load. This one re-inserts in the rname entry, without changing it.
  EXPECT_TRUE(TryFetchResource(cache_extended_url));
  EXPECT_EQ(cold_num_inserts, lru_cache()->num_inserts());
  EXPECT_EQ(1, lru_cache()->num_identical_reinserts());
}

// Extension of above with cache invalidation.
TEST_P(RewriteDriverTest, TestCacheUseOnTheFlyWithInvalidation) {
  resource_manager()->set_store_outputs_in_file_system(false);
  AddFilter(RewriteOptions::kExtendCacheCss);

  const char kCss[] = "* { display: none; }";
  InitResponseHeaders("a.css", kContentTypeCss, kCss, 100);

  GoogleString cache_extended_url =
      Encode(kTestDomain, RewriteOptions::kCacheExtenderId,
             hasher()->Hash(kCss), "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(cache_extended_url));

  // We should have 2 things inserted:
  // 1) the source data
  // 2) the rname entry for the result
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(2, cold_num_inserts);

  // Warm load. This one re-inserts in the rname entry, without changing it.
  EXPECT_TRUE(TryFetchResource(cache_extended_url));
  EXPECT_EQ(cold_num_inserts, lru_cache()->num_inserts());
  EXPECT_EQ(1, lru_cache()->num_identical_reinserts());

  // Set cache invalidation timestamp (to now, so that response date header is
  // in the "past") and load.
  ClearStats();
  int64 now_ms = mock_timer()->NowMs();
  options()->ClearSignatureForTesting();
  options()->set_cache_invalidation_timestamp(now_ms);
  options()->ComputeSignature(hasher());
  EXPECT_TRUE(TryFetchResource(cache_extended_url));
  // We expect: input re-insert, new metadata key
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(1, lru_cache()->num_identical_reinserts());
}

TEST_P(RewriteDriverTest, BaseTags) {
  // Starting the parse, the base-tag will be derived from the html url.
  ASSERT_TRUE(rewrite_driver()->StartParse("http://example.com/index.html"));
  rewrite_driver()->Flush();
  EXPECT_EQ("http://example.com/index.html", BaseUrlSpec());

  // If we then encounter a base tag, that will become the new base.
  rewrite_driver()->ParseText("<base href='http://new.example.com/subdir/'>");
  rewrite_driver()->Flush();
  EXPECT_EQ(0, message_handler()->TotalMessages());
  EXPECT_EQ("http://new.example.com/subdir/", BaseUrlSpec());

  // A second base tag will be ignored, and an info message will be printed.
  rewrite_driver()->ParseText("<base href=http://second.example.com/subdir2>");
  rewrite_driver()->Flush();
  EXPECT_EQ(1, message_handler()->TotalMessages());
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
  EXPECT_EQ(1, message_handler()->TotalMessages());
  EXPECT_EQ("http://new.example.com/subdir/", BaseUrlSpec());
}

TEST_P(RewriteDriverTest, RelativeBaseTag) {
  // Starting the parse, the base-tag will be derived from the html url.
  ASSERT_TRUE(rewrite_driver()->StartParse("http://example.com/index.html"));
  rewrite_driver()->ParseText("<base href='subdir/'>");
  rewrite_driver()->Flush();
  EXPECT_EQ(0, message_handler()->TotalMessages());
  EXPECT_EQ("http://example.com/subdir/", BaseUrlSpec());
}

TEST_P(RewriteDriverTest, InvalidBaseTag) {
  // Encountering an invalid base tag should be ignored (except info message).
  ASSERT_TRUE(rewrite_driver()->StartParse("slwly://example.com/index.html"));
  rewrite_driver()->ParseText("<base href='subdir_not_allowed_on_slwly/'>");
  rewrite_driver()->Flush();

  EXPECT_EQ(1, message_handler()->TotalMessages());
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
      // Short name should always succeed at creating new resource.
      resource.reset(rewrite_driver()->CreateOutputResourceWithPath(
          short_path, dummy_filter_id, short_name,
          content_types[t], resource_kinds[k]));
      EXPECT_TRUE(NULL != resource.get());

      // Long leaf-name should always fail at creating new resource.
      resource.reset(rewrite_driver()->CreateOutputResourceWithPath(
          short_path, dummy_filter_id, long_name,
          content_types[t], resource_kinds[k]));
      EXPECT_TRUE(NULL == resource.get());

      // Long total URL length should always fail at creating new resource.
      resource.reset(rewrite_driver()->CreateOutputResourceWithPath(
          long_path, dummy_filter_id, short_name,
          content_types[t], resource_kinds[k]));
      EXPECT_TRUE(NULL == resource.get());
    }
  }
}

TEST_P(RewriteDriverTest, MultipleDomains) {
  rewrite_driver()->AddFilters();

  // Make sure we authorize domains for resources properly. This is a regression
  // test for where loading things from a domain would prevent loads from an
  // another domain from the same RewriteDriver.

  const char kCss[] = "* { display: none; }";
  const char kAltDomain[] = "http://www.example.co.uk/";
  InitResponseHeaders(StrCat(kTestDomain, "a.css"), kContentTypeCss, kCss, 100);
  InitResponseHeaders(StrCat(kAltDomain, "b.css"), kContentTypeCss, kCss, 100);

  GoogleString rewritten1 = Encode(kTestDomain,
                                   RewriteOptions::kCacheExtenderId,
                                   hasher()->Hash(kCss), "a.css", "css");

  GoogleString rewritten2 = Encode(kAltDomain, RewriteOptions::kCacheExtenderId,
                                   hasher()->Hash(kCss), "b.css", "css");

  EXPECT_TRUE(TryFetchResource(rewritten1));
  rewrite_driver()->Clear();
  EXPECT_TRUE(TryFetchResource(rewritten2));
}

// Test caching behavior for normal UrlInputResources.
// This is the base case that LoadResourcesFromFiles below contrasts with.
TEST_P(RewriteDriverTest, LoadResourcesFromTheWeb) {
  rewrite_driver()->AddFilters();

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
  rewrite_driver()->AddFilters();

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
  rewrite_driver()->AddFilters();
  ASSERT_TRUE(rewrite_driver()->StartParse("http://example.com/index.html"));
  GoogleUrl resolved(rewrite_driver()->base_url(), "#anchor");
  EXPECT_EQ("http://example.com/index.html#anchor", resolved.Spec());
  rewrite_driver()->FinishParse();
}

namespace {

// A rewrite context that's not actually capable of rewriting -- we just need
// one to pass in to InfoAt in test below.
class MockRewriteContext : public SingleRewriteContext {
 public:
  explicit MockRewriteContext(RewriteDriver* driver) :
      SingleRewriteContext(driver, NULL, NULL) {}

  virtual void RewriteSingle(const ResourcePtr& input,
                             const OutputResourcePtr& output) {}
  virtual const char* id() const { return "mock"; }
  virtual OutputResourceKind kind() const { return kOnTheFlyResource; }
};

}  // namespace

TEST_P(RewriteDriverTest, DiagnosticsWithPercent) {
  // Regression test for crash in InfoAt where location has %stuff in it.
  // (make sure it actually shows up first, though).
  int prev_log_level = logging::GetMinLogLevel();
  logging::SetMinLogLevel(logging::LOG_INFO);
  rewrite_driver()->AddFilters();
  MockRewriteContext context(rewrite_driver());
  ResourcePtr resource(rewrite_driver()->CreateInputResourceAbsoluteUnchecked(
      "http://www.example.com/%s%s%s%d%f"));
  ResourceSlotPtr slot(new FetchResourceSlot(resource));
  context.AddSlot(slot);
  rewrite_driver()->InfoAt(&context, "Just a test");
  logging::SetMinLogLevel(prev_log_level);
}

// Tests that we reject https URLs quickly.
TEST_P(RewriteDriverTest, RejectHttpsQuickly) {
  // Need to expressly authorize https even though we don't support it.
  options()->domain_lawyer()->AddDomain("https://*/", message_handler());
  AddFilter(RewriteOptions::kRewriteJavascript);

  // When we don't support https then we fail quickly and cleanly.
  factory()->mock_url_async_fetcher()->set_fetcher_supports_https(false);
  ValidateNoChanges("reject_https_quickly",
                    "<script src='https://example.com/a.js'></script>");
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());

  // When we do support https the fetcher fails to find the resource.
  factory()->mock_url_async_fetcher()->set_fetcher_supports_https(true);
  SetFetchResponse404("https://example.com/a.js");
  ValidateNoChanges("reject_https_quickly",
                    "<script src='https://example.com/a.js'></script>");
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->failure_count());
}

INSTANTIATE_TEST_CASE_P(RewriteDriverTestInstance,
                        RewriteDriverTest,
                        ::testing::Bool());

INSTANTIATE_TEST_CASE_P(RewriteDriverTestUrlNamerInstance,
                        RewriteDriverTestUrlNamer,
                        ::testing::Bool());

class RewriteDriverInhibitTest : public RewriteDriverTest {
 protected:
  RewriteDriverInhibitTest() {}

  void SetUpDocument() {
    SetupWriter();
    ASSERT_TRUE(rewrite_driver()->StartParse("http://example.com/index.html"));

    // Set up a document: <html><body><p></p></body></html>.
    html_ = rewrite_driver()->NewElement(NULL, HtmlName::kHtml);
    body_ = rewrite_driver()->NewElement(html_, HtmlName::kBody);
    par_ = rewrite_driver()->NewElement(body_, HtmlName::kP);
    par_->set_close_style(HtmlElement::EXPLICIT_CLOSE);
    HtmlCharactersNode* start = rewrite_driver()->NewCharactersNode(NULL, "");
    HtmlTestingPeer::AddEvent(rewrite_driver(),
                              new HtmlCharactersEvent(start, -1));
    rewrite_driver()->InsertElementAfterElement(start, html_);
    rewrite_driver()->AppendChild(html_, body_);
    rewrite_driver()->AppendChild(body_, par_);
  }

  // Uninhibits the EndEvent for element, and waits for the necessary flush
  // to complete.
  void UninhibitEndElementAndWait(HtmlElement* element) {
    rewrite_driver()->UninhibitEndElementFlushless(element);
    ASSERT_TRUE(!rewrite_driver()->EndElementIsInhibited(element));
    rewrite_driver()->Flush();
  }

  // A callback for FinishParseAsync.
  void ParseFinished() {
  }

  HtmlElement* html_;
  HtmlElement* body_;
  HtmlElement* par_;

  DISALLOW_COPY_AND_ASSIGN(RewriteDriverInhibitTest);
};

// Tests that we stop the flush immediately before the EndElementEvent for an
// inhibited element, and resume it when that element is uninhibited.
TEST_P(RewriteDriverInhibitTest, InhibitEndElement) {
  SetUpDocument();

  // Inhibit </body>.
  rewrite_driver()->InhibitEndElement(body_);
  ASSERT_TRUE(rewrite_driver()->EndElementIsInhibited(body_));

  // Verify that we do not flush </body> or beyond, even on a second flush.
  rewrite_driver()->Flush();
  EXPECT_EQ("<html><body><p></p>", output_buffer_);
  rewrite_driver()->Flush();
  EXPECT_EQ("<html><body><p></p>", output_buffer_);

  // Verify that we flush the entire document once </body> is uninhibited.
  UninhibitEndElementAndWait(body_);
  EXPECT_EQ("<html><body><p></p></body></html>", output_buffer_);
}

// Tests that we can inhibit and uninhibit the flush in multiple places.
TEST_P(RewriteDriverInhibitTest, MultipleInhibitEndElement) {
  SetUpDocument();

  // Inhibit </body> and </html>.
  rewrite_driver()->InhibitEndElement(body_);
  ASSERT_TRUE(rewrite_driver()->EndElementIsInhibited(body_));
  rewrite_driver()->InhibitEndElement(html_);
  ASSERT_TRUE(rewrite_driver()->EndElementIsInhibited(html_));

  // Verify that we will not flush </body> or beyond.
  rewrite_driver()->Flush();
  EXPECT_EQ("<html><body><p></p>", output_buffer_);

  // Uninhibit </body> and verify that we flush it.
  UninhibitEndElementAndWait(body_);
  EXPECT_EQ("<html><body><p></p></body>", output_buffer_);

  // Verify that we will flush the entire document once </html> is uninhibited.
  UninhibitEndElementAndWait(html_);
  EXPECT_EQ("<html><body><p></p></body></html>", output_buffer_);
}

// Tests that FinishParseAsync respects inhibits.
TEST_P(RewriteDriverInhibitTest, InhibitWithFinishParse) {
  SetUpDocument();

  // Inhibit </body>.
  rewrite_driver()->InhibitEndElement(body_);
  ASSERT_TRUE(rewrite_driver()->EndElementIsInhibited(body_));

  // Start finishing the parse.
  SchedulerBlockingFunction wait(rewrite_driver()->scheduler());
  rewrite_driver()->FinishParseAsync(&wait);

  // Busy wait until the resulting async flush completes.
  mock_scheduler()->AwaitQuiescence();
  EXPECT_EQ("<html><body><p></p>", output_buffer_);

  // Uninhibit </body> and wait for FinishParseAsync to call back.
  rewrite_driver()->UninhibitEndElement(body_);
  ASSERT_TRUE(!rewrite_driver()->EndElementIsInhibited(body_));
  wait.Block();

  // Verify that we flush the entire document once </body> is uninhibited.
  EXPECT_EQ("<html><body><p></p></body></html>", output_buffer_);
}

INSTANTIATE_TEST_CASE_P(RewriteDriverInhibitTestInstance,
                        RewriteDriverInhibitTest,
                        ::testing::Bool());

}  // namespace net_instaweb
