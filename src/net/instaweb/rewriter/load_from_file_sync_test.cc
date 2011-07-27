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

// Author: sligocki@google.com (Shawn Ligocki)
//
// Adapted from rewrite_context_test, this tests loading from file in the
// sync flow (where it is significantly less effective).
//
// Branched from rewrite_context_test, because the sync flow is dying and we
// don't want to have to maintain that test for both flows.

#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

const char kTrimFilterPrefix[] = "tw";

// Simple filter which just trims whitespace from linked CSS files.
class TrimFilter : public RewriteSingleResourceFilter {
 public:
  TrimFilter(OutputResourceKind kind,
             RewriteDriver* driver, MessageHandler* handler)
      : RewriteSingleResourceFilter(driver, kTrimFilterPrefix),
        kind_(kind), driver_(driver), handler_(handler) {
    ClearStats();
  }
  virtual ~TrimFilter() {}

  virtual const char* Name() const { return "TrimFilter"; }

  virtual bool ComputeOnTheFly() const { return kind_ == kOnTheFlyResource; }

  // Stats
  int num_rewrites() const { return num_rewrites_; }
  void ClearStats() { num_rewrites_ = 0; }

 protected:
  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element) {
    if (element->name().keyword() == HtmlName::kLink) {
      // Original URL.
      HtmlElement::Attribute* attr = element->FindAttribute(HtmlName::kHref);
      EXPECT_TRUE(attr != NULL);
      // Rewrite resource.
      scoped_ptr<CachedResult> rewrite_info(
          RewriteWithCaching(attr->value(), NULL));
      // Update URL.
      EXPECT_TRUE(rewrite_info.get());
      attr->SetValue(rewrite_info->url());
    }
  }

 private:
  virtual RewriteResult RewriteLoadedResource(const ResourcePtr& input,
                                              const OutputResourcePtr& output) {
    ++num_rewrites_;
    // Trim input.
    GoogleString trimmed;
    TrimWhitespace(input->contents(), &trimmed);
    // Write output.
    driver_->resource_manager()->Write(
        HttpStatus::kOK, trimmed, output.get(),
        input->response_headers()->CacheExpirationTimeMs(), handler_);
    // Rewrite always succeeds.
    return kRewriteOk;
  }

  GoogleString filter_prefix_;
  UrlSegmentEncoder encoder_;

  int num_rewrites_;

  OutputResourceKind kind_;

  RewriteDriver* driver_;
  MessageHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(TrimFilter);
};

class LoadFromFileSyncTest : public ResourceManagerTestBase {
 protected:
  LoadFromFileSyncTest() : trim_filter_(NULL) {}
  virtual ~LoadFromFileSyncTest() {}

  void InitTrimFilters(OutputResourceKind kind) {
    // Owned by rewrite_driver().
    trim_filter_ = new TrimFilter(kind, rewrite_driver(), message_handler());
    rewrite_driver()->AddRewriteFilter(trim_filter_);
    rewrite_driver()->AddFilters();
    //other_rewrite_driver()->AddRewriteFilter(
    //    new TrimFilter(kind, other_rewrite_driver(), message_handler()));
    //other_rewrite_driver()->AddFilters();
  }

  GoogleString CssLinkHref(const StringPiece& url) {
    return StrCat("<link rel=stylesheet href=", url, ">");
  }

  virtual void ClearStats() {
    ResourceManagerTestBase::ClearStats();
    if (trim_filter_ != NULL) {
      trim_filter_->ClearStats();
    }
  }

  TrimFilter* trim_filter_;  // Just a ref, filter is owned by rewrite_driver().
};

TEST_F(LoadFromFileSyncTest, OnTheFly) {
  InitTrimFilters(kOnTheFlyResource);

  // Init file resources.
  InitResponseHeaders("http://test.com/a.css", kContentTypeCss,
                      " foo b ar ", 5 * Timer::kMinuteMs);

  // First time we load and rewrite the resource (blocking fetch).
  ClearStats();
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // Second time we just get a cache hit, no rewrites or fetches.
  ClearStats();
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
}

TEST_F(LoadFromFileSyncTest, Rewritten) {
  InitTrimFilters(kRewrittenResource);

  // Init file resources.
  InitResponseHeaders("http://test.com/a.css", kContentTypeCss,
                      " foo b ar ", 5 * Timer::kMinuteMs);

  // First time we load and rewrite the resource (blocking fetch).
  ClearStats();
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // Second time we just get a cache hit, no rewrites or fetches.
  ClearStats();
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
}

TEST_F(LoadFromFileSyncTest, LoadFromFileOnTheFly) {
  options()->file_load_policy()->Associate("http://test.com/", "/test/");
  InitTrimFilters(kOnTheFlyResource);

  // Init file resources.
  WriteFile("/test/a.css", " foo b ar ");

  // First time we load and rewrite the resource (blocking filesystem load).
  ClearStats();
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());

  // Second time we just get a cache hit, no rewrites or loads.
  ClearStats();
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
}

TEST_F(LoadFromFileSyncTest, LoadFromFileRewritten) {
  options()->file_load_policy()->Associate("http://test.com/", "/test/");
  InitTrimFilters(kRewrittenResource);

  // Init file resources.
  WriteFile("/test/a.css", " foo b ar ");

  // First time we load and rewrite the resource (blocking filesystem load).
  ClearStats();
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());

  // Second time we just get a cache hit, no rewrites or loads.
  ClearStats();
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref("http://test.com/a.css.pagespeed.tw.0.css"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
}

// Test resource update behavior.
class LoadFromFileResourceUpdateTest : public LoadFromFileSyncTest {
 protected:
  static const char kOriginalUrl[];
  static const char kRewrittenUrlFormat[];

  LoadFromFileResourceUpdateTest() {
    FetcherUpdateDateHeaders();
  }

  // Simulates requesting HTML doc and then loading resource.
  GoogleString RewriteSingleResource(const StringPiece& id) {
    return RewriteSingleResource(id, true);
  }

  GoogleString RewriteSingleResource(const StringPiece& id, bool check_hash) {
    const GoogleString html_input = CssLinkHref(kOriginalUrl);

    // We use MD5 hasher instead of mock hasher so that different resources
    // are assigned different URLs.
    UseMd5Hasher();

    // Rewrite HTML.
    Parse(id, html_input);

    // Find rewritten resource URL.
    StringVector css_urls;
    CollectCssLinks(StrCat(id, "-collect"), output_buffer_, &css_urls);
    EXPECT_EQ(1UL, css_urls.size());
    const GoogleString& rewritten_url = css_urls[0];

    // Fetch rewritten resource
    GoogleString contents;
    EXPECT_TRUE(ServeResourceUrl(rewritten_url, &contents));

    // Check that hash code is correct.
    if (check_hash) {
      ResourceNamer namer;
      namer.Decode(rewritten_url);
      EXPECT_EQ(hasher()->Hash(contents), namer.hash());
    }

    return contents;
  }
};

const char LoadFromFileResourceUpdateTest::kOriginalUrl[] = "a.css";
const char LoadFromFileResourceUpdateTest::kRewrittenUrlFormat[] =
    "http://test.com/a.css.pagespeed.tw.%s.css";

TEST_F(LoadFromFileResourceUpdateTest, OnTheFly) {
  InitTrimFilters(kOnTheFlyResource);

  int64 ttl_ms = 5 * Timer::kMinuteMs;

  // 1) Set first version of resource.
  InitResponseHeaders(kOriginalUrl, kContentTypeCss, " init ", ttl_ms / 1000);
  ClearStats();
  EXPECT_EQ("init", RewriteSingleResource("first_load"));
  // TODO(sligocki): Why are we rewriting twice here?
  //EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 2) Advance time, but not so far that resources have expired.
  mock_timer()->AdvanceMs(ttl_ms / 2);
  ClearStats();
  // Rewrite should be the same.
  EXPECT_EQ("init", RewriteSingleResource("advance_time"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 3) Change resource.
  InitResponseHeaders(kOriginalUrl, kContentTypeCss, " new ", ttl_ms / 1000);
  ClearStats();
  // Rewrite should still be the same, because it's found in cache.
  EXPECT_EQ("init", RewriteSingleResource("stale_content"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 4) Advance time so that old cached input resource expires.
  mock_timer()->AdvanceMs(ttl_ms);
  ClearStats();
  // Rewrite should now use new resource.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
  //EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
}

TEST_F(LoadFromFileResourceUpdateTest, Rewritten) {
  InitTrimFilters(kRewrittenResource);

  int64 ttl_ms = 5 * Timer::kMinuteMs;

  // 1) Set first version of resource.
  InitResponseHeaders(kOriginalUrl, kContentTypeCss, " init ", ttl_ms / 1000);
  ClearStats();
  EXPECT_EQ("init", RewriteSingleResource("first_load"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 2) Advance time, but not so far that resources have expired.
  mock_timer()->AdvanceMs(ttl_ms / 2);
  ClearStats();
  // Rewrite should be the same.
  EXPECT_EQ("init", RewriteSingleResource("advance_time"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 3) Change resource.
  InitResponseHeaders(kOriginalUrl, kContentTypeCss, " new ", ttl_ms / 1000);
  ClearStats();
  // Rewrite should still be the same, because it's found in cache.
  EXPECT_EQ("init", RewriteSingleResource("stale_content"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 4) Advance time so that old cached input resource expires.
  mock_timer()->AdvanceMs(ttl_ms);
  ClearStats();
  // Rewrite should now use new resource.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
}

TEST_F(LoadFromFileResourceUpdateTest, LoadFromFileOnTheFly) {
  options()->file_load_policy()->Associate("http://test.com/", "/test/");
  InitTrimFilters(kOnTheFlyResource);

  int64 ttl_ms = 5 * Timer::kMinuteMs;

  // 1) Set first version of resource.
  WriteFile("/test/a.css", " init ");
  ClearStats();
  EXPECT_EQ("init", RewriteSingleResource("first_load"));
  //EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  //EXPECT_EQ(1, file_system()->num_input_file_opens());
  EXPECT_EQ(2, file_system()->num_input_file_opens());

  // 2) Advance time, but not so far that resources would have expired if
  // they were loaded by UrlFetch.
  mock_timer()->AdvanceMs(ttl_ms / 2);
  ClearStats();
  // Rewrite should be the same.
  EXPECT_EQ("init", RewriteSingleResource("advance_time"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());

  // 3) Change resource.
  WriteFile("/test/a.css", " new ");
  ClearStats();
  // Rewrite should immediately update.
  // Note: We serve a hash code for "init" here, but compute "new" correctly
  // as the contents.
  bool check_hash_code = false;
  EXPECT_EQ("new", RewriteSingleResource("updated_content", check_hash_code));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());

  // 4) Advance time so that old cached input resource expires.
  mock_timer()->AdvanceMs(ttl_ms);
  ClearStats();
  // Rewrite should now use new resource.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
  //EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  //EXPECT_EQ(1, file_system()->num_input_file_opens());
  EXPECT_EQ(2, file_system()->num_input_file_opens());
}

TEST_F(LoadFromFileResourceUpdateTest, LoadFromFileRewritten) {
  options()->file_load_policy()->Associate("http://test.com/", "/test/");
  InitTrimFilters(kRewrittenResource);

  int64 ttl_ms = 5 * Timer::kMinuteMs;

  // 1) Set first version of resource.
  WriteFile("/test/a.css", " init ");
  ClearStats();
  EXPECT_EQ("init", RewriteSingleResource("first_load"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());

  // 2) Advance time, but not so far that resources would have expired if
  // they were loaded by UrlFetch.
  mock_timer()->AdvanceMs(ttl_ms / 2);
  ClearStats();
  // Rewrite should be the same.
  EXPECT_EQ("init", RewriteSingleResource("advance_time"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 3) Change resource.
  WriteFile("/test/a.css", " new ");
  ClearStats();
  // Rewrite does not immediately update, because we are caching it for 5min.
  EXPECT_EQ("init", RewriteSingleResource("updated_content"));
  //EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  //EXPECT_EQ(1, file_system()->num_input_file_opens());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 4) Advance time so that old cached input resource expires.
  mock_timer()->AdvanceMs(ttl_ms);
  ClearStats();
  // Rewrite now happens because implicit cache lifetime is done.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
  //EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  //EXPECT_EQ(0, file_system()->num_input_file_opens());
  EXPECT_EQ(1, file_system()->num_input_file_opens());
}

}  // namespace

}  // namespace net_instaweb
