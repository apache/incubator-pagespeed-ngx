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

// Author: nikhilmadan@google.com (Nikhil Madan)

#include "net/instaweb/rewriter/public/in_place_rewrite_context.h"

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/fake_filter.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/test_distributed_fetcher.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_scheduler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/worker_test_base.h"
#include "net/instaweb/util/public/timer.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/user_agent_matcher_test_base.h"

namespace net_instaweb {

class MessageHandler;

namespace {

class FakeFetch : public AsyncFetch {
 public:
  FakeFetch(const RequestContextPtr& request_context,
            RewriteOptions* options,
            const GoogleString& url,
            WorkerTestBase::SyncPoint* sync,
            ResponseHeaders* response_headers)
      : AsyncFetch(request_context),
        done_(false),
        success_(false),
        options_(options),
        url_(url),
        sync_(sync) {
    set_response_headers(response_headers);
  }

  virtual ~FakeFetch() {}

  virtual void HandleHeadersComplete() {}
  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    content.AppendToString(&content_);
    return true;
  }
  virtual bool HandleFlush(MessageHandler* handler) {
    return true;
  }
  virtual void HandleDone(bool success) {
    response_headers()->ComputeCaching();
    done_ = true;
    success_ = success;
    sync_->Notify();
  }
  StringPiece content() { return content_; }
  bool done() { return done_; }
  bool success() { return success_; }

  bool IsCachedResultValid(const ResponseHeaders& headers) {
    return OptionsAwareHTTPCacheCallback::IsCacheValid(
        url_, *options_, request_context(), headers);
  }

 private:
  GoogleString content_;
  bool done_;
  bool success_;
  const RewriteOptions* options_;
  GoogleString url_;
  WorkerTestBase::SyncPoint* sync_;

  DISALLOW_COPY_AND_ASSIGN(FakeFetch);
};

class FakeImageFilter : public FakeFilter {
 public:
  class Context : public FakeFilter::Context {
   public:
    Context(FakeImageFilter* filter, RewriteDriver* driver,
            RewriteContext* parent, ResourceContext* resource_context)
        : FakeFilter::Context(filter, driver, parent, resource_context),
          filter_(filter) { }

    virtual void DoRewriteSingle(const ResourcePtr input,
                                 OutputResourcePtr output) {
      CachedResult* cached = output->EnsureCachedResultCreated();
      cached->set_minimal_webp_support(filter_->minimal_webp_support());
      FakeFilter::Context::DoRewriteSingle(input, output);
    }

   private:
    FakeImageFilter* filter_;
    DISALLOW_COPY_AND_ASSIGN(Context);
  };

  explicit FakeImageFilter(RewriteDriver* rewrite_driver)
      : FakeFilter(RewriteOptions::kImageCompressionId,
                   rewrite_driver, semantic_type::kImage),
        minimal_webp_support_(ResourceContext::LIBWEBP_LOSSY_ONLY) { }

  void set_minimal_webp_support(ResourceContext::LibWebpLevel level) {
    minimal_webp_support_ = level;
  }
  ResourceContext::LibWebpLevel minimal_webp_support() {
    return minimal_webp_support_;
  }
  RewriteContext* MakeFakeContext(
      RewriteDriver* driver, RewriteContext* parent,
      ResourceContext* resource_context) {
    return new FakeImageFilter::Context(this, driver, parent, resource_context);
  }

 private:
  ResourceContext::LibWebpLevel minimal_webp_support_;
  DISALLOW_COPY_AND_ASSIGN(FakeImageFilter);
};

class InPlaceRewriteContextTest : public RewriteTestBase {
 protected:
  static const bool kWriteToCache = true;
  static const bool kNoWriteToCache = false;
  static const bool kNoTransform = true;
  static const bool kTransform = false;
  InPlaceRewriteContextTest()
      : img_filter_(NULL),
        other_img_filter_(NULL),
        js_filter_(NULL),
        css_filter_(NULL),
        cache_html_url_("http://www.example.com/cacheable.html"),
        cache_jpg_url_("http://www.example.com/cacheable.jpg"),
        cache_jpg_no_extension_url_("http://www.example.com/cacheable_jpg"),
        cache_jpg_notransform_url_("http://www.example.com/notransform.jpg"),
        cache_jpg_vary_star_url_("http://www.example.com/vary_star.jpg"),
        cache_jpg_vary_ua_url_("http://www.example.com/vary_ua.jpg"),
        cache_jpg_vary_origin_url_("http://www.example.com/vary_origin.jpg"),
        cache_png_url_("http://www.example.com/cacheable.png"),
        cache_gif_url_("http://www.example.com/cacheable.gif"),
        cache_webp_url_("http://www.example.com/cacheable.webp"),
        cache_js_url_("http://www.example.com/cacheable.js"),
        cache_js_jpg_extension_url_("http://www.example.com/cacheable_js.jpg"),
        cache_css_url_("http://www.example.com/cacheable.css"),
        nocache_html_url_("http://www.example.com/nocacheable.html"),
        nocache_js_url_("http://www.example.com/nocacheable.js"),
        private_cache_js_url_("http://www.example.com/privatecacheable.js"),
        cache_js_no_max_age_url_("http://www.example.com/cacheablemod.js"),
        bad_url_("http://www.example.com/bad.url"),
        redirect_url_("http://www.example.com/redir.url"),
        rewritten_jpg_url_(
            "http://www.example.com/cacheable.jpg.pagespeed.ic.0.jpg"),
        cache_body_("good"), nocache_body_("bad"),
        bad_body_("ugly"),
        redirect_body_("Location: http://www.example.com/final.url"),
        ttl_ms_(Timer::kHourMs), etag_("W/\"PSA-aj-0\""),
        original_etag_("original_etag"),
        exceed_deadline_(false), optimize_for_browser_(false),
        oversized_stream_(NULL), in_place_uncacheable_rewrites_(NULL),
        distributed_rewrite_failures_(NULL),
        distributed_rewrite_successes_(NULL) {}

  virtual void Init() {
    SetTimeMs(start_time_ms());
    mock_url_fetcher()->set_fail_on_unexpected(false);

    const StringPiece kNoVary("");

    // Set fetcher result and headers.
    AddResponse(cache_html_url_, kContentTypeHtml, cache_body_, start_time_ms(),
                ttl_ms_, original_etag_, kNoVary, kNoWriteToCache, kTransform);
    AddResponse(cache_jpg_url_, kContentTypeJpeg, cache_body_, start_time_ms(),
                ttl_ms_, "", kNoVary, kNoWriteToCache, kTransform);
    AddResponse(cache_jpg_no_extension_url_, kContentTypeJpeg, cache_body_,
                start_time_ms(), ttl_ms_, "", kNoVary,
                kNoWriteToCache, kTransform);
    AddResponse(cache_jpg_notransform_url_, kContentTypeJpeg, cache_body_,
                start_time_ms(), ttl_ms_, "", kNoVary,
                kNoWriteToCache, kNoTransform);
    AddResponse(cache_jpg_vary_star_url_, kContentTypeJpeg, cache_body_,
                start_time_ms(), ttl_ms_, "", /* Vary: */ "*",
                kNoWriteToCache, kTransform);
    AddResponse(cache_jpg_vary_ua_url_, kContentTypeJpeg, cache_body_,
                start_time_ms(), ttl_ms_, "", /* Vary: */ "User-Agent",
                kNoWriteToCache, kTransform);
    AddResponse(cache_jpg_vary_origin_url_, kContentTypeJpeg, cache_body_,
                start_time_ms(), ttl_ms_, "", /* Vary: */ "Origin",
                kNoWriteToCache, kTransform);
    AddResponse(cache_png_url_, kContentTypePng, cache_body_, start_time_ms(),
                ttl_ms_, original_etag_, kNoVary, kWriteToCache, kTransform);
    AddResponse(cache_gif_url_, kContentTypeGif, cache_body_, start_time_ms(),
                ttl_ms_, original_etag_, kNoVary, kWriteToCache, kTransform);
    AddResponse(cache_webp_url_, kContentTypeWebp, cache_body_, start_time_ms(),
                ttl_ms_, original_etag_, kNoVary, kWriteToCache, kTransform);
    AddResponse(cache_js_url_, kContentTypeJavascript, cache_body_,
                start_time_ms(), ttl_ms_, "", kNoVary,
                kNoWriteToCache, kTransform);
    AddResponse(cache_js_jpg_extension_url_, kContentTypeJavascript,
                cache_body_, start_time_ms(), ttl_ms_, "", kNoVary,
                kNoWriteToCache, kTransform);
    AddResponse(cache_css_url_, kContentTypeCss, cache_body_, start_time_ms(),
                ttl_ms_, "", kNoVary, kNoWriteToCache, kTransform);
    AddResponse(nocache_html_url_, kContentTypeHtml, nocache_body_,
                start_time_ms(), -1, "", kNoVary, kNoWriteToCache, kTransform);
    AddResponse(nocache_js_url_, kContentTypeJavascript, cache_body_,
                start_time_ms(), -1 /*ttl*/, "" /*etag*/, kNoVary,
                kNoWriteToCache, kTransform);
    AddResponse(cache_js_no_max_age_url_, kContentTypeJavascript, cache_body_,
                start_time_ms(), 0, "", kNoVary, kNoWriteToCache, kTransform);

    ResponseHeaders private_headers;
    SetDefaultHeaders(kContentTypeJavascript, &private_headers);
    private_headers.SetDateAndCaching(start_time_ms(), 1200 /*ttl*/,
                                      ",private");
    mock_url_fetcher()->SetResponse(
        private_cache_js_url_, private_headers, cache_body_);

    ResponseHeaders bad_headers;
    bad_headers.set_first_line(1, 1, 404, "Not Found");
    bad_headers.SetDate(start_time_ms());
    mock_url_fetcher()->SetResponse(bad_url_, bad_headers, bad_body_);

    // Add a response for permanent redirect.
    ResponseHeaders redirect_headers;
    redirect_headers.set_first_line(1, 1, 301, "Moved Permanently");
    redirect_headers.SetCacheControlMaxAge(36000);
    redirect_headers.Add(HttpAttributes::kCacheControl, "public");
    redirect_headers.Add(HttpAttributes::kContentType, "image/jpeg");
    mock_url_fetcher()->SetResponse(
        redirect_url_, redirect_headers, redirect_body_);

    img_filter_ = new FakeImageFilter(rewrite_driver());
    js_filter_ = new FakeFilter(RewriteOptions::kJavascriptMinId,
                                rewrite_driver(), semantic_type::kScript);
    css_filter_ = new FakeFilter(RewriteOptions::kCssFilterId, rewrite_driver(),
                                 semantic_type::kStylesheet);

    rewrite_driver()->AppendRewriteFilter(img_filter_);
    rewrite_driver()->AppendRewriteFilter(js_filter_);
    rewrite_driver()->AppendRewriteFilter(css_filter_);
    options()->ClearSignatureForTesting();
    AddRecompressImageFilters();
    options()->EnableFilter(RewriteOptions::kRewriteJavascript);
    options()->EnableFilter(RewriteOptions::kRewriteCss);
    if (optimize_for_browser()) {
      options()->EnableFilter(RewriteOptions::kInPlaceOptimizeForBrowser);
      options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
    }
    options()->set_in_place_rewriting_enabled(true);
    server_context()->ComputeSignature(options());
    // Clear stats since we may have added something to the cache.
    ClearStats();

    oversized_stream_ = statistics()->GetVariable(
        InPlaceRewriteContext::kInPlaceOversizedOptStream);
    in_place_uncacheable_rewrites_ = statistics()->GetVariable(
        InPlaceRewriteContext::kInPlaceUncacheableRewrites);
    distributed_rewrite_failures_ = statistics()->GetVariable(
        RewriteContext::kNumDistributedRewriteFailures);
    distributed_rewrite_successes_ = statistics()->GetVariable(
        RewriteContext::kNumDistributedRewriteSuccesses);
  }

  void AddResponse(const GoogleString& url, const ContentType& content_type,
                   const GoogleString& body, int64 now_ms, int64 ttl_ms,
                   const GoogleString& etag, const StringPiece& vary,
                   bool write_to_cache, bool no_transform) {
    ResponseHeaders response_headers;
    SetDefaultHeaders(content_type, &response_headers);
    if (ttl_ms > 0) {
      response_headers.SetDateAndCaching(now_ms, ttl_ms);
    } else {
      response_headers.SetDate(now_ms);
      if (ttl_ms < 0) {
        response_headers.Replace(HttpAttributes::kCacheControl, "no-cache");
      } else {
        response_headers.Replace(HttpAttributes::kCacheControl, "public");
      }
    }
    if (!vary.empty()) {
      response_headers.Replace(HttpAttributes::kVary, vary);
    }
    if (no_transform) {
      response_headers.Replace(HttpAttributes::kCacheControl, "no-transform");
    }
    if (!etag.empty()) {
      response_headers.Add(HttpAttributes::kEtag, etag);
    }
    mock_url_fetcher()->SetResponse(url, response_headers, body);
    if (write_to_cache) {
      response_headers.ComputeCaching();
      http_cache()->Put(
          url, rewrite_driver_->CacheFragment(),
          request_headers_.GetProperties(),
          ResponseHeaders::GetVaryOption(options()->respect_vary()),
          &response_headers, body, message_handler());
    }
  }

  void SetDefaultHeaders(const ContentType& content_type,
                         ResponseHeaders* header) const {
    header->set_major_version(1);
    header->set_minor_version(1);
    header->SetStatusAndReason(HttpStatus::kOK);
    header->Replace(HttpAttributes::kContentType, content_type.mime_type());
  }

  void SetAcceptWebp() {
    request_headers_.Add(HttpAttributes::kAccept, "image/webp");
  }

  void FetchAndCheckResponse(const GoogleString& url,
                             const GoogleString& expected_body,
                             bool expected_success,
                             int64 expected_ttl,
                             const char* etag,
                             int64 date_ms) {
    js_filter_->set_exceed_deadline(exceed_deadline_);
    img_filter_->set_exceed_deadline(exceed_deadline_);
    if (other_img_filter_ != NULL) {
      other_img_filter_->set_exceed_deadline(exceed_deadline_);
    }
    css_filter_->set_exceed_deadline(exceed_deadline_);

    WorkerTestBase::SyncPoint sync(server_context()->thread_system());
    RequestContextPtr request_context(RequestContext::NewTestRequestContext(
        server_context()->thread_system()));
    FakeFetch mock_fetch(request_context, options(), url, &sync,
                         &response_headers_);
    mock_fetch.set_request_headers(&request_headers_);

    ClearRewriteDriver();
    if (!user_agent_.empty()) {
      rewrite_driver()->SetUserAgent(user_agent_);
    }
    rewrite_driver()->SetRequestHeaders(request_headers_);
    rewrite_driver()->FetchResource(url, &mock_fetch);
    // If we're testing if the rewrite takes too long, we need to push
    // time forward here.
    if (exceed_deadline()) {
      rewrite_driver()->BoundedWaitFor(
          RewriteDriver::kWaitForCompletion,
          rewrite_driver()->rewrite_deadline_ms());
    }

    sync.Wait();
    rewrite_driver()->WaitForShutDown();
    mock_scheduler()->AwaitQuiescence();  // needed for cache puts to finish.
    EXPECT_TRUE(mock_fetch.done());
    EXPECT_EQ(expected_success, mock_fetch.success()) << url;
    EXPECT_EQ(expected_body, mock_fetch.content()) << url;
    EXPECT_EQ(expected_ttl, response_headers_.cache_ttl_ms()) << url;
    EXPECT_STREQ(etag, response_headers_.Lookup1(HttpAttributes::kEtag)) << url;
    EXPECT_EQ(date_ms, response_headers_.date_ms()) << url;
  }

  void ResetHeadersAndStats() {
    request_headers_.Clear();
    response_headers_.Clear();
    img_filter_->ClearStats();
    if (other_img_filter_ != NULL) {
      other_img_filter_->ClearStats();
    }
    js_filter_->ClearStats();
    css_filter_->ClearStats();
    RewriteTestBase::ClearStats();
  }

  void CheckWarmCache(StringPiece id) {
    EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count()) << id;
    EXPECT_EQ(1, http_cache()->cache_hits()->Get()) << id;
    EXPECT_EQ(0, http_cache()->cache_misses()->Get()) << id;
    EXPECT_EQ(0, http_cache()->cache_inserts()->Get()) << id;
    EXPECT_EQ(2, lru_cache()->num_hits()) << id;
    EXPECT_EQ(0, lru_cache()->num_misses()) << id;
    EXPECT_EQ(0, lru_cache()->num_inserts()) << id;
    EXPECT_EQ(0, img_filter_->num_rewrites()) << id;
    EXPECT_EQ(0, js_filter_->num_rewrites()) << id;
    EXPECT_EQ(0, css_filter_->num_rewrites()) << id;
    EXPECT_EQ(0, oversized_stream_->Get()) << id;
  }

  void SetupDistributedTest(const StringPiece& distributed_filter) {
    SetupSharedCache();
    other_img_filter_ = new FakeImageFilter(other_rewrite_driver());
    other_rewrite_driver()->AppendRewriteFilter(other_img_filter_);
    options()->ClearSignatureForTesting();
    other_options()->ClearSignatureForTesting();
    AddRecompressImageFilters();
    options()->EnableFilter(RewriteOptions::kRewriteJavascript);
    options()->EnableFilter(RewriteOptions::kRewriteCss);
    options_->DistributeFilter(distributed_filter);
    options_->set_distributed_rewrite_servers("example.com:80");
    options_->set_distributed_rewrite_key("1234123");
    Init();
    other_options()->Merge(*options());
    other_server_context()->ComputeSignature(other_options());
  }

  void CheckDistributedFetch(int distributed_fetch_success_count,
                             int distributed_fetch_failure_count,
                             int local_fetch_required, int rewritten) {
    EXPECT_EQ(distributed_fetch_success_count + distributed_fetch_failure_count,
              counting_distributed_fetcher()->fetch_count());
    EXPECT_EQ(local_fetch_required,
              counting_url_async_fetcher()->fetch_count());
    EXPECT_EQ(
        0, other_factory_->counting_distributed_async_fetcher()->fetch_count());
    EXPECT_EQ(distributed_fetch_success_count,
              distributed_rewrite_successes_->Get());
    EXPECT_EQ(distributed_fetch_failure_count,
              distributed_rewrite_failures_->Get());
    EXPECT_EQ(rewritten,
              img_filter_->num_rewrites() + other_img_filter_->num_rewrites());
  }

  void ExpectInPlaceImageSuccessFlow(const GoogleString& url) {
    FetchAndCheckResponse(url, cache_body_, true, ttl_ms_,
                          original_etag_, start_time_ms());

    // First fetch misses initial metadata cache lookup, finds original in
    // cache; the resource gets rewritten and the rewritten
    // resource gets inserted into cache.
    EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
    EXPECT_EQ(1, http_cache()->cache_hits()->Get());
    EXPECT_EQ(0, http_cache()->cache_misses()->Get());
    EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
    EXPECT_EQ(1, lru_cache()->num_hits());
    EXPECT_EQ(2, lru_cache()->num_misses());
    EXPECT_EQ(3, lru_cache()->num_inserts());
    EXPECT_EQ(1, img_filter_->num_rewrites());
    EXPECT_EQ(0, js_filter_->num_rewrites());
    EXPECT_EQ(0, css_filter_->num_rewrites());

    ResetHeadersAndStats();
    SetTimeMs(start_time_ms() + ttl_ms_/2);
    FetchAndCheckResponse(url, "good:ic", true, ttl_ms_/2, etag_,
                          start_time_ms() + ttl_ms_/2);
    // Second fetch hits the metadata cache and the rewritten resource is
    // served out.
    CheckWarmCache("second_fetch_1");

    AdvanceTimeMs(2 * ttl_ms_);
    ResetHeadersAndStats();
    FetchAndCheckResponse(url, cache_body_, true, ttl_ms_,
                          original_etag_, timer()->NowMs());
    // The metadata and cache entry is stale now. Fetch the content and serve
    // out the original. The background rewrite work then revalidates
    // the response and updates metadata.
    EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
    EXPECT_EQ(0, http_cache()->cache_hits()->Get());
    EXPECT_EQ(1, http_cache()->cache_misses()->Get());
    EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
    EXPECT_EQ(3, lru_cache()->num_hits());  // (expired) orig., aj, ic metadata
    EXPECT_EQ(0, lru_cache()->num_misses());
    EXPECT_EQ(3, lru_cache()->num_inserts());
    EXPECT_EQ(0, img_filter_->num_rewrites());
    EXPECT_EQ(0, js_filter_->num_rewrites());
    EXPECT_EQ(0, css_filter_->num_rewrites());
  }

  bool exceed_deadline() { return exceed_deadline_; }
  void set_exceed_deadline(bool x) { exceed_deadline_ = x; }

  bool optimize_for_browser() { return optimize_for_browser_; }
  void set_optimize_for_browser(bool x) { optimize_for_browser_ = x; }

  FakeImageFilter* img_filter_;
  FakeImageFilter* other_img_filter_;
  FakeFilter* js_filter_;
  FakeFilter* css_filter_;

  RequestHeaders request_headers_;
  ResponseHeaders response_headers_;

  const GoogleString cache_html_url_;
  const GoogleString cache_jpg_url_;
  const GoogleString cache_jpg_no_extension_url_;
  const GoogleString cache_jpg_notransform_url_;
  const GoogleString cache_jpg_vary_star_url_;
  const GoogleString cache_jpg_vary_ua_url_;
  const GoogleString cache_jpg_vary_origin_url_;
  const GoogleString cache_png_url_;
  const GoogleString cache_gif_url_;
  const GoogleString cache_webp_url_;
  const GoogleString cache_js_url_;
  const GoogleString cache_js_jpg_extension_url_;
  const GoogleString cache_css_url_;
  const GoogleString nocache_html_url_;
  const GoogleString nocache_js_url_;
  const GoogleString private_cache_js_url_;
  const GoogleString cache_js_no_max_age_url_;
  const GoogleString bad_url_;
  const GoogleString redirect_url_;
  const GoogleString rewritten_jpg_url_;

  const GoogleString cache_body_;
  const GoogleString nocache_body_;
  const GoogleString bad_body_;
  const GoogleString redirect_body_;

  GoogleString user_agent_;

  const int ttl_ms_;
  const char* etag_;
  const char* original_etag_;
  bool exceed_deadline_;
  bool optimize_for_browser_;

  Variable* oversized_stream_;
  Variable* in_place_uncacheable_rewrites_;
  Variable* distributed_rewrite_failures_;
  Variable* distributed_rewrite_successes_;
};

TEST_F(InPlaceRewriteContextTest, IngressDistributedRewrite) {
  // Distribute an image rewrite (the response of the rewrite task is mocked).
  SetupDistributedTest(RewriteOptions::kInPlaceRewriteId);

  FetchAndCheckResponse(cache_jpg_url_, "good", true, ttl_ms_, NULL,
                        start_time_ms());
  CheckDistributedFetch(1,      // successful distributed fetches
                        0,      // unsuccessful distributed fetches
                        0,      // number of ingress fetches
                        1);     // number of rewrites

  // Ingress task distributes and streams back the result.
  // Rewrite task misses metadata, misses http, fetches, writes original,
  // misses ic metadata, writes optimized, and writes metadata for ipro and ic.
  EXPECT_EQ(1, counting_distributed_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(4, lru_cache()->num_inserts());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());

  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_, etag_,
                        start_time_ms());

  // Ingress task distributes, distributed task hits and returns.
  EXPECT_EQ(1, counting_distributed_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
}

TEST_F(InPlaceRewriteContextTest, IngressDistributedRewriteImage) {
  // Distribute the nested image task instead of the in_place_rewrite filter.
  SetupDistributedTest(RewriteOptions::kImageCompressionId);

  FetchAndCheckResponse(cache_jpg_url_, "good", true, ttl_ms_, NULL,
                        start_time_ms());
  CheckDistributedFetch(1,      // successful distributed fetches
                        0,      // unsuccessful distributed fetches
                        1,      // number of ingress fetches
                        1);     // number of rewrites

  // Ingress task: misses IPRO metadata, fetches the resource (miss and insert)
  // misses ic metadata, and distributes, eventually writing IPRO metadata.
  // Rewrite task: misses ic metadata, hits http, writes optimized, and ic.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(4, lru_cache()->num_misses());
  EXPECT_EQ(4, lru_cache()->num_inserts());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());

  ResetHeadersAndStats();
  SetTimeMs((start_time_ms() + ttl_ms_/2));
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_/2, etag_,
                        start_time_ms() + ttl_ms_ / 2);

  // Ingress task hits ipro metadata and associated http resource.
  EXPECT_EQ(0, counting_distributed_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
}

TEST_F(InPlaceRewriteContextTest, IngressDistributedNestedWaitForOptimized) {
  // Like IngressDistributedNested but this time we want to wait for the
  // optimized result, which causes a distributed GET request and the returned
  // content is fed into the nested context's output_ for IPRO's Harvest.
  options_->set_in_place_wait_for_optimized(true);
  SetupDistributedTest(RewriteOptions::kImageCompressionId);

  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_, etag_,
                        start_time_ms());

  // Ingress task fetches in IPRO and then distributes the nested image rewrite.
  CheckDistributedFetch(1,      // successful distributed fetches
                        0,      // unsuccessful distributed fetches
                        1,      // number of ingress fetches
                        1);     // number of rewrites

  // Ingress task: IPRO misses metadata and http original resource then fetches
  // and inserts http original resource. Ingress then starts nested image
  // rewriter which skips metadata check (force_rewrite) and distributes. IPRO
  // writes its metadata.
  // Distributed task: Misses image metadata, hits the original resource, and
  // stores the new metadata and optimized resource.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(4, lru_cache()->num_inserts());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());

  ResetHeadersAndStats();
  SetTimeMs((start_time_ms() + ttl_ms_/2));
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_/2, etag_,
                        start_time_ms() + ttl_ms_ / 2);
  CheckDistributedFetch(0,      // successful distributed fetches
                        0,      // unsuccessful distributed fetches
                        0,      // number of ingress fetches
                        0);     // number of rewrites
  // Ingress task hits ipro metadata and associated http resource.
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
}

TEST_F(InPlaceRewriteContextTest,
       IngressDistributedNestedWaitForOptimizedFail) {
  // Wait for an optimized result but the fetcher breaks after the headers are
  // written. Fall back to the original resource.
  options_->set_in_place_wait_for_optimized(true);
  SetupDistributedTest(RewriteOptions::kImageCompressionId);
  test_distributed_fetcher()->set_fail_after_headers(true);

  FetchAndCheckResponse(cache_jpg_url_, "good", true, ttl_ms_, NULL,
                        start_time_ms());

  // Ingress task fetches in IPRO and then distributes the nested image rewrite.
  CheckDistributedFetch(0,      // successful distributed fetches
                        1,      // unsuccessful distributed fetches
                        1,      // number of ingress fetches
                        1);     // number of rewrites

  // Ingress task: IPRO misses metadata and http original resource,
  // fetches and inserts http original resource. Then starts nested image
  // rewriter which skips the metadata lookup (force_rewrite) and distributes.
  // Distributed task: Misses image metadata, hits the original resource, and
  // stores the new metadata and optimized resource.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(4, lru_cache()->num_inserts());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());

  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_jpg_url_, "good", true, ttl_ms_, kEtag0.c_str(),
                        start_time_ms());
  CheckDistributedFetch(0,      // successful distributed fetches
                        0,      // unsuccessful distributed fetches
                        0,      // number of ingress fetches
                        0);     // number of rewrites
  // Ingress task hits ipro metadata and associated http resource.
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
}

TEST_F(InPlaceRewriteContextTest, IngressDistributedRewriteNotFound) {
  // If the distributed fetcher returns a 404 then that's what should be
  // returned.
  SetupDistributedTest(RewriteOptions::kInPlaceRewriteId);

  GoogleString orig_url = StrCat(kTestDomain, "fourofour.png");
  SetFetchResponse404(orig_url);

  FetchAndCheckResponse(orig_url, "", true, ServerContext::kGeneratedMaxAgeMs,
                        ServerContext::kResourceEtagValue, start_time_ms());
  EXPECT_EQ(HttpStatus::kNotFound, response_headers_.status_code());

  // The distributed fetcher should have run once on the ingress task and the
  // url fetcher should have run once on the rewrite task.  The result goes to
  // shared cache.
  CheckDistributedFetch(1,      // successful distributed fetches
                        0,      // unsuccessful distributed fetches
                        0,      // number of ingress fetches
                        0);     // number of rewrites

  // Ingress task distributes and returns the 404 it gets back.
  // Rewrite task misses metadata and http, writes 404 http and returns.
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());

  // Ingress task distributes.
  // Rewrite task misses ipro metadata but hits http, and returns that.
  ResetHeadersAndStats();
  FetchAndCheckResponse(orig_url, "", true, ServerContext::kGeneratedMaxAgeMs,
                        ServerContext::kResourceEtagValue, start_time_ms());
  CheckDistributedFetch(1,      // successful distributed fetches
                        0,      // unsuccessful distributed fetches
                        0,      // number of ingress fetches
                        0);     // number of rewrites
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
}

TEST_F(InPlaceRewriteContextTest, IngressDistributedRewriteFailFallback) {
  // If the distributed fetch fails mid-stream then we fail.
  SetupDistributedTest(RewriteOptions::kInPlaceRewriteId);
  test_distributed_fetcher()->set_fail_after_headers(true);

  FetchAndCheckResponse(cache_jpg_url_, "", false, ttl_ms_, NULL,
                        start_time_ms());
  // Note that we didn't need to fetch the original resource at the ingress task
  // because the distributed task already fetched it and put it in shared cache.
  CheckDistributedFetch(0,      // successful distributed fetches
                        1,      // unsuccessful distributed fetches
                        0,      // number of ingress fetches
                        1);     // number of rewrites
  // Ingress task: Distributes and streams back a failed stream.
  // Rewrite task: Misses ipro metadata, misses http, fetches and inserts http,
  // misses image compression metadata, writes optimized http and 2 metadata,
  // returns.
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(4, lru_cache()->num_inserts());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());

  // Fetch again but we'll hit the cache this time.
  ResetHeadersAndStats();
  SetTimeMs((start_time_ms() + ttl_ms_ / 2));
  FetchAndCheckResponse(cache_jpg_url_, "", false, ttl_ms_ / 2, etag_,
                        start_time_ms() + ttl_ms_ / 2);
  CheckDistributedFetch(0,      // successful distributed fetches
                        1,      // unsuccessful distributed fetches
                        0,      // number of ingress fetches
                        0);     // number of rewrites
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
}

TEST_F(InPlaceRewriteContextTest, IngressDistributedRewriteFailFallbackImage) {
  // If the distributed fetch fails mid-stream then the unoptimized resource
  // should be returned. This time image compression is distributed but not IPRO
  // itself.
  SetupDistributedTest(RewriteOptions::kImageCompressionId);

  // Simulate distributed fetch failure and ensure that we fall back to the
  // original.
  test_distributed_fetcher()->set_fail_after_headers(true);

  FetchAndCheckResponse(cache_jpg_url_, "good", true, ttl_ms_, NULL,
                        start_time_ms());
  // Note that we didn't need to fetch the original resource at the ingress task
  // because the distributed task already fetched it and put it in shared cache.
  CheckDistributedFetch(0,      // successful distributed fetches
                        1,      // unsuccessful distributed fetches
                        1,      // number of ingress fetches
                        1);     // number of rewrites
  // Ingress task: Misses ipro metadata, fetches and inserts input resource,
  // misses ic metadata, distributes. Upon distributed failure, aborts nested
  // rewrite and IPRO records the failure.
  // Rewrite task: Misses ic metadata, hits input resources, writes optimized
  // resource and ic metadata, then returns.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(4, lru_cache()->num_misses());
  EXPECT_EQ(4, lru_cache()->num_inserts());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());

  // On the first attempt IPRO failed to rewrite the image due to its nested
  // task's distribution failure.  On the second attempt, IPRO remembers that
  // the first attempt failed and returns the original resource.

  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_jpg_url_, "good", true, ttl_ms_, kEtag0.c_str(),
                        start_time_ms());
  CheckDistributedFetch(0,      // successful distributed fetches
                        0,      // unsuccessful distributed fetches
                        0,      // number of ingress fetches
                        0);     // number of rewrites
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
}

TEST_F(InPlaceRewriteContextTest, CacheableHtmlUrlNoRewriting) {
  // All these entries find no in-place rewrite metadata and no rewriting
  // happens.
  Init();
  FetchAndCheckResponse(cache_html_url_, cache_body_, true, ttl_ms_,
                        original_etag_, start_time_ms());
  // First fetch misses initial cache lookup, succeeds at fetch and inserts
  // result into cache.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());  // metadata + html
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_html_url_, cache_body_, true, ttl_ms_,
                        original_etag_, start_time_ms());
  // Second fetch hits initial cache lookup and no extra fetches are needed.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());  // metadata
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  AdvanceTimeMs(2 * ttl_ms_);
  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_html_url_, cache_body_, true, ttl_ms_,
                        original_etag_, start_time_ms() + 2 * ttl_ms_);
  // Cache entry is stale, so we must fetch again.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());  // HTML is in LRU cache, just expired.
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
}

TEST_F(InPlaceRewriteContextTest, WaitForOptimizedFirstRequest) {
  // By setting this flag we should get an optimized response on the first
  // request unless we hit a rewrite timeout but in this test it will complete
  // in time.
  options()->set_in_place_wait_for_optimized(true);
  Init();

  // The optimized content from the fake rewriter has ":ic" appended to original
  // content.
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_, etag_,
                        start_time_ms());

  // First fetch misses initial cache lookup, succeeds at fetch and inserts
  // result into cache. Also, the resource gets rewritten and the rewritten
  // resource gets inserted into cache. The optimized version should be
  // returned.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());  // rewritten + original
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(4, lru_cache()->num_inserts());
  EXPECT_EQ(1, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
  EXPECT_EQ(0, oversized_stream_->Get());

  ResetHeadersAndStats();
  SetTimeMs((start_time_ms() + ttl_ms_/2));
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_/2, etag_,
                        start_time_ms() + ttl_ms_/2);
  // Second fetch hits the metadata cache and the rewritten resource is
  // served out.
  CheckWarmCache("second_fetch_2");
}

TEST_F(InPlaceRewriteContextTest, WaitForOptimizeWithDisabledFilter) {
  // Wait for optimized but if the resource fails to optimize we should get
  // back the original resource.
  options()->set_in_place_wait_for_optimized(true);
  // We'll also test that the hash values we get are legitimate and not
  // hard-coded 0s
  UseMd5Hasher();

  Init();

  // Turn off optimization. The filter will still run but return false in
  // rewrite.
  img_filter_->set_enabled(false);
  FetchAndCheckResponse(cache_jpg_url_, cache_body_, true, ttl_ms_,
                        NULL, start_time_ms());

  // First fetch misses initial cache lookup, succeeds at fetch and inserts
  // result into cache. Failure to rewrite means original should be returned.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());  // original only
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
  EXPECT_EQ(0, oversized_stream_->Get());

  ResetHeadersAndStats();
  // The second time we get the cached original, which should have an md5'd
  // etag.

  // TODO(jkarlin): Note that if we advance time here, we'd expect the TTL of
  // the cached resource to decrease on the second fetch, but that doesn't
  // happen. That should be fixed.
  GoogleString expected_etag = StrCat("W/\"PSA-", hasher()->Hash(cache_body_),
                                      "\"");
  FetchAndCheckResponse(cache_jpg_url_, cache_body_, true, ttl_ms_,
                        expected_etag.c_str(), start_time_ms());
  // Second fetch hits the metadata cache, sees that the rewrite failed and
  // fetches and serves the original resource from cache.
  CheckWarmCache("second_fetch_3");
}

TEST_F(InPlaceRewriteContextTest, WaitForOptimizeNoTransform) {
  // Confirm that when cache-control:no-transform is present in the response
  // headers that the in-place optimizer does not optimize the resource.
  options()->set_in_place_wait_for_optimized(true);
  Init();

  // Don't rewrite since it's no-transform.
  FetchAndCheckResponse(cache_jpg_notransform_url_, cache_body_, true,
                        ttl_ms_, NULL, start_time_ms());
  // First fetch misses initial cache lookup, succeeds at fetch and inserts
  // into cache.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());  // original + ipro metadata
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  EXPECT_TRUE(response_headers_.HasValue(HttpAttributes::kCacheControl,
                                         "no-transform"));

  ResetHeadersAndStats();

  // Don't rewrite since it's no-transform.
  FetchAndCheckResponse(cache_jpg_notransform_url_, cache_body_, true,
                        ttl_ms_, kEtag0.c_str(), start_time_ms());
  // The second fetch should return the cached original after seeing that it
  // can't be rewritten.
  CheckWarmCache("second_fetch_4");
}

TEST_F(InPlaceRewriteContextTest, OptimizeOnNoTransformIfOptionFalse) {
  options()->set_disable_rewrite_on_no_transform(false);
  Init();
  FetchAndCheckResponse(cache_jpg_notransform_url_, cache_body_, true,
                        ttl_ms_, NULL, start_time_ms());
  // First fetch misses initial cache lookup, succeeds at fetch and inserts
  // into cache. Also the resource gets rewritten and the rewritten resource
  // gets inserted into cache.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());  // rewritten + original
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(4, lru_cache()->num_inserts());
  EXPECT_EQ(1, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  ResetHeadersAndStats();
  SetTimeMs((start_time_ms() + ttl_ms_/2));
  FetchAndCheckResponse(cache_jpg_notransform_url_, "good:ic", true,
                        ttl_ms_/2, etag_, start_time_ms() + ttl_ms_/2);
  // Second fetch hits the metadata cache and the rewritten resource is served
  // out.
  CheckWarmCache("second_fetch_notransform");
}

TEST_F(InPlaceRewriteContextTest, WaitForOptimizeTimeout) {
  // Confirm that rewrite deadlines cause the original resource to be returned
  // (but caches the optimized) even if in_place_wait_for_optimize is on.
  options()->set_in_place_wait_for_optimized(true);
  Init();

  // Tells the optimizing filter to slow down.
  exceed_deadline_ = true;

  FetchAndCheckResponse(cache_jpg_url_, cache_body_, true, ttl_ms_,
                        NULL, start_time_ms());
  // First fetch misses initial cache lookup, succeeds at fetch and inserts
  // result into cache. Rewrite succeeds but is slow so original returned.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());  // rewritten + original
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(4, lru_cache()->num_inserts());
  EXPECT_EQ(1, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
  EXPECT_EQ(0, oversized_stream_->Get());

  ResetHeadersAndStats();
  SetTimeMs((start_time_ms() + ttl_ms_/2));

  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_/2, etag_,
                        start_time_ms() + ttl_ms_/2);
  // Second fetch hits the metadata cache and the rewritten resource is served
  // out.
  CheckWarmCache("second_fetch_5");
}

TEST_F(InPlaceRewriteContextTest, WaitForOptimizeResourceTooBig) {
  // Wait for optimized but if it's larger than the RecordingFetch can handle
  // make sure we piece together the original resource properly.
  options()->set_in_place_wait_for_optimized(true);

  Init();

  // To make this more interesting there should be something in the cache to
  // recover when we fail.  Let's split the url_fetch from 'good' into 'go' and
  // 'od' writes.
  mock_url_fetcher()->set_split_writes(true);

  // By setting cache max to 2, the second write ('od') will cause an overflow.
  // Test that we recover.
  http_cache()->set_max_cacheable_response_content_length(2);

  FetchAndCheckResponse(cache_jpg_url_, cache_body_, true, ttl_ms_,
                        NULL, start_time_ms());

  // First fetch misses initial cache lookup, succeeds at fetch but resource too
  // big for cache.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());  // rewritten + original
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
  EXPECT_EQ(1, oversized_stream_->Get());

  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_jpg_url_, cache_body_, true, ttl_ms_,
                        NULL, start_time_ms());
  // Second fetch should also completely miss because the first fetch was too
  // big to stuff in the cache.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());  // rewritten + original
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
  EXPECT_EQ(1, oversized_stream_->Get());
}

TEST_F(InPlaceRewriteContextTest, CacheableJpgUrlRewritingSucceeds) {
  Init();
  FetchAndCheckResponse(cache_jpg_url_, cache_body_, true, ttl_ms_, NULL,
                        start_time_ms());

  // First fetch misses initial cache lookup, succeeds at fetch and inserts
  // result into cache. Also, the resource gets rewritten and the rewritten
  // resource gets inserted into cache.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());  // rewritten + original
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(4, lru_cache()->num_inserts());
  EXPECT_EQ(1, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  ResetHeadersAndStats();
  SetTimeMs((start_time_ms() + ttl_ms_/2));
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_/2, etag_,
                        start_time_ms() + ttl_ms_/2);
  // Second fetch hits the metadata cache and the rewritten resource is served
  // out.
  CheckWarmCache("second_fetch_6");

  ResetHeadersAndStats();
  // We get a 304 if we send a request with an If-None-Match matching the hash
  // of the rewritten resource.
  request_headers_.Add(HttpAttributes::kIfNoneMatch, etag_);
  FetchAndCheckResponse(cache_jpg_url_, "", true, ttl_ms_/2, NULL, 0);
  EXPECT_EQ(HttpStatus::kNotModified, response_headers_.status_code());
  // We hit the metadata cache and find that the etag matches the hash of the
  // rewritten resource.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  ResetHeadersAndStats();
  // The etag doesn't match and hence we serve the full response.
  request_headers_.Add(HttpAttributes::kIfNoneMatch, "no-match");
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_/2, etag_,
                        start_time_ms() + ttl_ms_/2);
  EXPECT_EQ(HttpStatus::kOK, response_headers_.status_code());
  // We hit the metadata cache, but the etag doesn't match so we fetch the
  // rewritten resource from the HTTPCache and serve it out.
  CheckWarmCache("etag_mismatch");

  // Delete the rewritten resource from cache to check if reconstruction works.
  lru_cache()->Delete(HttpCacheKey(rewritten_jpg_url_));

  ResetHeadersAndStats();
  // Original resource is served with the date set to start time.
  // The ETag we check for here is the ETag HTTPCache synthesized for
  // the original resource.
  FetchAndCheckResponse(cache_jpg_url_, "good", true, ttl_ms_, kEtag0.c_str(),
                        start_time_ms());
  // We find the metadata in cache, but don't find the rewritten resource.
  // Hence, we reconstruct the resource and insert it into cache. We see 2
  // identical reinserts - one for the image rewrite filter metadata and one for
  // the in-place metadata.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(2, lru_cache()->num_identical_reinserts());
  EXPECT_EQ(1, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  // For only the next request, update the date header so that freshening
  // succeeds.
  FetcherUpdateDateHeaders();
  ResetHeadersAndStats();
  int64 time_ms = start_time_ms() + ttl_ms_ - 2 * Timer::kMinuteMs;
  SetTimeMs(time_ms);
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, 2 * Timer::kMinuteMs,
                        etag_, time_ms);
  // This fetch hits the metadata cache and the rewritten resource is served
  // out. Freshening is triggered here and we insert the freshened response and
  // metadata into the cache.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(3, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  mock_url_fetcher()->set_update_date_headers(false);

  ResetHeadersAndStats();
  SetTimeMs((start_time_ms() + ttl_ms_ * 5/4));
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true,
                        ttl_ms_ * 3/4 - 2 * Timer::kMinuteMs, etag_,
                        start_time_ms() + ttl_ms_ * 5/4);
  // Since the previous request freshened the metadata, this fetch hits the
  // metadata cache and the rewritten resource is served out. Note that no
  // freshening needs to be triggered here.
  CheckWarmCache("freshened_metadata");

  AdvanceTimeMs(2 * ttl_ms_);
  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_jpg_url_, cache_body_, true, ttl_ms_, NULL,
                        timer()->NowMs());
  // The metadata and cache entry is stale now. Fetch the content and serve out
  // the original. We will however notice that the contents did not
  // actually change and update the metadata cache promptly, without
  // rewriting.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(3, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
}

TEST_F(InPlaceRewriteContextTest, CacheablePngUrlRewritingSucceeds) {
  Init();
  ExpectInPlaceImageSuccessFlow(cache_png_url_);
}

TEST_F(InPlaceRewriteContextTest, CacheablePngUrlRewritingSucceedsWithShards) {
  Init();
  const char kShard1[] = "http://s1.example.com/";
  const char kShard2[] = "http://s2.example.com/";
  AddShard("http://www.example.com", StrCat(kShard1, ",", kShard2));
  ExpectInPlaceImageSuccessFlow(cache_png_url_);
}

TEST_F(InPlaceRewriteContextTest, CacheableiGifUrlRewritingSucceeds) {
  Init();
  ExpectInPlaceImageSuccessFlow(cache_gif_url_);
}

TEST_F(InPlaceRewriteContextTest, CacheableWebpUrlRewritingSucceeds) {
  Init();
  ExpectInPlaceImageSuccessFlow(cache_webp_url_);
}

TEST_F(InPlaceRewriteContextTest, CacheablePngUrlRewritingFails) {
  // Setup the image filter to fail at rewriting.
  Init();
  img_filter_->set_enabled(false);
  FetchAndCheckResponse(cache_png_url_, cache_body_, true, ttl_ms_,
                        original_etag_, start_time_ms());

  // First fetch misses initial metadata lookup, finds original in cache.
  // The rewrite fails and metadata is inserted into the
  // cache indicating that the rewriting didn't succeed.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_png_url_, cache_body_, true, ttl_ms_,
                        original_etag_, start_time_ms());
  // Second fetch hits the metadata cache, sees that the rewrite failed and
  // fetches and serves the original resource from cache.
  CheckWarmCache("second_fetch_7");
}

TEST_F(InPlaceRewriteContextTest, CacheableJsUrlRewritingSucceeds) {
  Init();
  FetchAndCheckResponse(cache_js_url_, cache_body_, true, ttl_ms_, NULL,
                        start_time_ms());

  // First fetch misses initial cache lookup, succeeds at fetch and inserts
  // result into cache. Also, the resource gets rewritten and the rewritten
  // resource gets inserted into cache.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(4, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(1, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  ResetHeadersAndStats();
  SetTimeMs(start_time_ms() + ttl_ms_/2);
  FetchAndCheckResponse(cache_js_url_, "good:jm", true, ttl_ms_/2, etag_,
                        start_time_ms() + ttl_ms_/2);
  // Second fetch hits the metadata cache and the rewritten resource is served
  // out.
  CheckWarmCache("second_fetch_8");

  AdvanceTimeMs(2 * ttl_ms_);
  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_js_url_, cache_body_, true, ttl_ms_, NULL,
                        timer()->NowMs());
  // The metadata and cache entry is stale now. Fetch the content and serve it
  // out without rewriting. The background rewrite will then revalidate
  // a previous rewrite's result and reuse it.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(3, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
}

TEST_F(InPlaceRewriteContextTest, CacheableJsUrlRewritingWithStaleServing) {
  Init();
  options()->ClearSignatureForTesting();
  options()->set_metadata_cache_staleness_threshold_ms(ttl_ms_);
  server_context()->ComputeSignature(options());

  FetchAndCheckResponse(cache_js_url_, cache_body_, true, ttl_ms_, NULL,
                        start_time_ms());

  // First fetch misses initial cache lookup, succeeds at fetch and inserts
  // result into cache. Also, the resource gets rewritten and the rewritten
  // resource gets inserted into cache.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(4, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(1, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  ResetHeadersAndStats();
  SetTimeMs(start_time_ms() + ttl_ms_/2);
  FetchAndCheckResponse(cache_js_url_, "good:jm", true, ttl_ms_/2, etag_,
                        start_time_ms() + ttl_ms_/2);
  CheckWarmCache("second_fetch_9");

  SetTimeMs(start_time_ms() + (3 * ttl_ms_) / 2);
  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_js_url_, "good:jm", true,
                        ResponseHeaders::kDefaultImplicitCacheTtlMs, etag_,
                        start_time_ms() + (3 * ttl_ms_) / 2);
  // The metadata and cache entry is stale now. We serve the rewritten resource
  // here, but trigger a fetch and rewrite to update the metadata.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(3, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
}

TEST_F(InPlaceRewriteContextTest, CacheableJsUrlModifiedImplicitCacheTtl) {
  Init();
  response_headers_.set_implicit_cache_ttl_ms(500 * Timer::kSecondMs);
  FetchAndCheckResponse(cache_js_no_max_age_url_, cache_body_,
                        /* expected_success= */ true,
                        /* expected_ttl= */ 500 * Timer::kSecondMs,
                        /* etag= */ NULL,
                        /* date_ms= */ start_time_ms());
}

TEST_F(InPlaceRewriteContextTest, CacheableCssUrlIfCssRewritingDisabled) {
  Init();
  options()->ClearSignatureForTesting();
  options()->DisableFilter(RewriteOptions::kRewriteCss);
  server_context()->ComputeSignature(options());
  FetchAndCheckResponse(cache_css_url_, cache_body_, true, ttl_ms_, NULL,
                        start_time_ms());

  // First fetch succeeds at the fetcher, no rewriting happens since the css
  // filter is disabled, and metadata indicating a rewriting failure gets
  // inserted into cache.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  ResetHeadersAndStats();

  // The ETag we check for here is the ETag HTTPCache synthesized for
  // the original resource.
  FetchAndCheckResponse(cache_css_url_, cache_body_, true, ttl_ms_,
                        kEtag0.c_str(), start_time_ms());

  // Second fetch hits the metadata cache, finds that the result is not
  // optimizable. It then looks up cache for the original and finds it.
  CheckWarmCache("second_fetch_10");
}

TEST_F(InPlaceRewriteContextTest, CacheableCssUrlRewritingSucceeds) {
  Init();
  EnableCachePurge();
  FetchAndCheckResponse(cache_css_url_, cache_body_, true, ttl_ms_, NULL,
                        start_time_ms());

  // First fetch misses initial cache lookup, succeeds at fetch and inserts
  // result into cache. Also, the resource gets rewritten and the rewritten
  // resource gets inserted into cache.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(4, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(1, css_filter_->num_rewrites());

  ResetHeadersAndStats();
  SetTimeMs(start_time_ms() + ttl_ms_/2);
  FetchAndCheckResponse(cache_css_url_, "good:cf", true, ttl_ms_/2, etag_,
                        start_time_ms() + ttl_ms_/2);
  // Second fetch hits the metadata cache and the rewritten resource is served
  // out.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  AdvanceTimeMs(2 * ttl_ms_);
  ResetHeadersAndStats();
  int64 date_of_css_ms = timer()->NowMs();
  FetchAndCheckResponse(cache_css_url_, cache_body_, true, ttl_ms_, NULL,
                        date_of_css_ms);
  // The metadata and cache entry is stale now. Fetch the content and serve it
  // out without rewriting. The background rewrite attempt will end up reusing
  // the old result due to revalidation, however.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(3, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  mock_url_fetcher()->set_timer(timer());
  mock_url_fetcher()->set_update_date_headers(true);
  SetCacheInvalidationTimestamp();
  date_of_css_ms = timer()->NowMs();

  // Having flushed cache, we are now back to serving the origin content.
  FetchAndCheckResponse(cache_css_url_, cache_body_, true, ttl_ms_,
                        NULL, date_of_css_ms);

  // Next time we'll serve optimized content.
  AdvanceTimeMs(ttl_ms_/2);
  ResetHeadersAndStats();
  int64 expected_ttl_ms = ttl_ms_ - (timer()->NowMs() - date_of_css_ms);
  FetchAndCheckResponse(cache_css_url_, "good:cf", true, expected_ttl_ms,
                        etag_, timer()->NowMs());
}

TEST_F(InPlaceRewriteContextTest, NonCacheableUrlNoRewriting) {
  Init();
  FetchAndCheckResponse(nocache_html_url_, nocache_body_, true, 0, NULL,
                        timer()->NowMs());
  // First fetch misses initial cache lookup, succeeds at fetch and we don't
  // insert into cache because it's not cacheable. Don't attempt to rewrite
  // this since its not cacheable.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
}

// Tests that with correct flags set, the uncacheable resource will be
// rewritten. Also checks, that resource will not be inserted.
TEST_F(InPlaceRewriteContextTest, NonCacheableUrlRewriting) {
  Init();

  // Modify options for our test.
  options()->ClearSignatureForTesting();
  options()->set_in_place_wait_for_optimized(true);
  options()->set_rewrite_uncacheable_resources(true);
  server_context()->ComputeSignature(options());

  // The ttl is just a value in proto, actual cacheable values will be checked
  // below.
  FetchAndCheckResponse(nocache_js_url_, StrCat(cache_body_, ":", "jm"),
                        true /* success */,
                        Timer::kYearMs /* ttl (ms) */,
                        etag_ /* etag */,
                        timer()->NowMs());

  // Shouldn't be cacheable at all.
  EXPECT_FALSE(response_headers_.IsBrowserCacheable());
  EXPECT_FALSE(response_headers_.IsProxyCacheable());

  // First fetch misses initial cache lookup, succeeds at fetch and we don't
  // insert into cache because it's not cacheable. But since flags are set to
  // rewrite uncacheable resources, JS rewriting should occur.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  // Should have been rewritten.
  EXPECT_EQ(1, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
  EXPECT_EQ(1, in_place_uncacheable_rewrites_->Get());
}

// Tests, that with correct flags set the private cacheable resource will be
// rewritten. Also checks, that the resource will not be cached.
TEST_F(InPlaceRewriteContextTest, PrivateCacheableUrlRewriting) {
  Init();

  // Modify options for our test.
  options()->ClearSignatureForTesting();
  options()->set_in_place_wait_for_optimized(true);
  options()->set_rewrite_uncacheable_resources(true);
  server_context()->ComputeSignature(options());

  // The ttl is just a value in proto, actual cacheable values will be checked
  // below.
  FetchAndCheckResponse(private_cache_js_url_, StrCat(cache_body_, ":", "jm"),
                        true /* success */,
                        1000 /* ttl (s) */,
                        etag_ /* etag */,
                        timer()->NowMs());
  // Should be cacheable.
  EXPECT_TRUE(response_headers_.IsBrowserCacheable());

  // But only in a private way.
  EXPECT_FALSE(response_headers_.IsProxyCacheable());

  // First fetch misses initial cache lookup, succeeds at fetch and we don't
  // insert into cache because it's not cacheable. But since flags are set to
  // rewrite uncacheable resources, JS rewriting should occur.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  // Should have been rewritten.
  EXPECT_EQ(1, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
  EXPECT_EQ(1, in_place_uncacheable_rewrites_->Get());
}


TEST_F(InPlaceRewriteContextTest, BadUrlNoRewriting) {
  Init();
  FetchAndCheckResponse(bad_url_, bad_body_, true, 0, NULL, start_time_ms());
  // First fetch misses initial cache lookup, succeeds at fetch and we don't
  // insert into cache because it's not cacheable. Don't attempt to rewrite
  // this since its not cacheable.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
}

TEST_F(InPlaceRewriteContextTest, PermanentRedirectNoRewriting) {
  options()->set_in_place_wait_for_optimized(true);
  Init();
  FetchAndCheckResponse(
      redirect_url_, redirect_body_, true /* expected_success */,
      36000 /* ttl (s) */, NULL /* etag */, start_time_ms());

  // Don't attempt to rewrite this since it's not a 200 response.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
}

TEST_F(InPlaceRewriteContextTest, FetchFailedNoRewriting) {
  Init();
  FetchAndCheckResponse("http://www.notincache.com", "", false, 0, NULL,
                        start_time_ms());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
}

TEST_F(InPlaceRewriteContextTest, HandleResourceCreationFailure) {
  // Regression test. Trying to in-place optimize https resources with
  // a fetcher that didn't support https would fail to invoke the callbacks
  // and leak the rewrite driver.
  Init();
  factory()->mock_url_async_fetcher()->set_fetcher_supports_https(false);
  FetchAndCheckResponse("https://www.example.com", "", false, 0, NULL, 0);
}

TEST_F(InPlaceRewriteContextTest, ResponseHeaderMimeTypeUpdate) {
  options()->set_in_place_wait_for_optimized(true);
  Init();
  // We are going to rewrite a PNG image below. Assume it will be converted
  // to a JPEG.
  img_filter_->set_output_content_type(&kContentTypeJpeg);
  FetchAndCheckResponse(cache_png_url_, "good:ic", true, ttl_ms_, etag_,
                        start_time_ms());
  EXPECT_STREQ(kContentTypeJpeg.mime_type(),
               response_headers_.Lookup1(HttpAttributes::kContentType));
}

TEST_F(InPlaceRewriteContextTest, OptimizeForBrowserEncodingAndHeader) {
  options()->set_in_place_wait_for_optimized(true);
  set_optimize_for_browser(true);
  Init();

  // Image with correct extension in URL.
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_, etag_,
                        start_time_ms());
  EXPECT_EQ(0, css_filter_->num_encode_user_agent());
  EXPECT_EQ(1, img_filter_->num_encode_user_agent());
  EXPECT_EQ(0, js_filter_->num_encode_user_agent());
  EXPECT_STREQ(HttpAttributes::kAccept,
               response_headers_.Lookup1(HttpAttributes::kVary));

  // Image with no extension in URL.
  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_jpg_no_extension_url_, "good:ic", true, ttl_ms_,
                        etag_, start_time_ms());
  EXPECT_EQ(1, css_filter_->num_encode_user_agent());
  EXPECT_EQ(1, img_filter_->num_encode_user_agent());
  EXPECT_EQ(0, js_filter_->num_encode_user_agent());
  EXPECT_STREQ(HttpAttributes::kAccept,
               response_headers_.Lookup1(HttpAttributes::kVary));

  // CSS with correct extension in URL.
  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_css_url_, "good:cf", true, ttl_ms_, etag_,
                        start_time_ms());
  EXPECT_EQ(1, css_filter_->num_encode_user_agent());
  EXPECT_EQ(0, img_filter_->num_encode_user_agent());
  EXPECT_EQ(0, js_filter_->num_encode_user_agent());
  EXPECT_STREQ(HttpAttributes::kUserAgent,
               response_headers_.Lookup1(HttpAttributes::kVary));

  // HTML with correct extension in URL.
  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_html_url_, "good", true, ttl_ms_,
                        original_etag_, start_time_ms());
  EXPECT_EQ(0, css_filter_->num_encode_user_agent());
  EXPECT_EQ(0, img_filter_->num_encode_user_agent());
  EXPECT_EQ(0, js_filter_->num_encode_user_agent());
  EXPECT_EQ(NULL, response_headers_.Lookup1(HttpAttributes::kVary));

  // Javascript with correct extension in URL.
  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_js_url_, "good:jm", true, ttl_ms_, etag_,
                        start_time_ms());
  EXPECT_EQ(0, css_filter_->num_encode_user_agent());
  EXPECT_EQ(0, img_filter_->num_encode_user_agent());
  EXPECT_EQ(0, js_filter_->num_encode_user_agent());
  EXPECT_EQ(NULL, response_headers_.Lookup1(HttpAttributes::kVary));

  // Javascript with jpeg extension in URL.
  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_js_jpg_extension_url_, "good:jm", true, ttl_ms_,
                        etag_, start_time_ms());
  EXPECT_EQ(0, css_filter_->num_encode_user_agent());
  EXPECT_EQ(1, img_filter_->num_encode_user_agent());
  EXPECT_EQ(0, js_filter_->num_encode_user_agent());
  EXPECT_EQ(NULL, response_headers_.Lookup1(HttpAttributes::kVary));

  // Bad content with unknown extension.
  ResetHeadersAndStats();
  FetchAndCheckResponse(bad_url_, bad_body_, true, 0, NULL, start_time_ms());
  EXPECT_EQ(1, css_filter_->num_encode_user_agent());
  EXPECT_EQ(1, img_filter_->num_encode_user_agent());
  EXPECT_EQ(0, js_filter_->num_encode_user_agent());
  EXPECT_EQ(NULL, response_headers_.Lookup1(HttpAttributes::kVary));
}

TEST_F(InPlaceRewriteContextTest, OptimizeForBrowserRewriting) {
  // When in_place_wait_for_optimized is true, force_rewrite is set to true and
  // the nested RewriteContext will not check for rewritten content if input
  // is ready. Keep that in mind when checking lru_cache hits/misses.
  options()->set_in_place_wait_for_optimized(true);
  options()->set_private_not_vary_for_ie(true);
  set_optimize_for_browser(true);
  Init();

  // First fetch with kTestUserAgentWebP. This will miss everything (metadata
  // lookup, original content, and rewritten content).
  // Vary: Accept header should be added.
  user_agent_ = UserAgentMatcher::kTestUserAgentWebP;
  SetAcceptWebp();
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_, etag_,
                        start_time_ms());

  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());  // original
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());  // rewritten + original
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());  // + ipro-md
  EXPECT_EQ(4, lru_cache()->num_inserts());  // + ipro-md + md
  EXPECT_EQ(1, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
  EXPECT_EQ(0, oversized_stream_->Get());
  EXPECT_STREQ(HttpAttributes::kAccept,
               response_headers_.Lookup1(HttpAttributes::kVary));

  // The second fetch uses a different user agent, kTestUserAgentNoWebP.
  // This will miss the metadata cache so it will start fetch input (cache hit)
  // and rewrite content (cache miss).
  // Vary: Accept header should be be added.
  ResetHeadersAndStats();
  SetTimeMs(start_time_ms() + ttl_ms_/2);
  user_agent_ = UserAgentMatcher::kTestUserAgentNoWebP;
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_/2, etag_,
                        start_time_ms() + ttl_ms_/2);
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());  // original
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());  // rewritten
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());  // rewritten
  EXPECT_EQ(1, lru_cache()->num_hits());  // original
  EXPECT_EQ(1, lru_cache()->num_misses());  // ipro-md
  EXPECT_EQ(3, lru_cache()->num_inserts());  // + ipro-md + md
  EXPECT_EQ(1, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
  EXPECT_EQ(0, oversized_stream_->Get());
  EXPECT_STREQ(HttpAttributes::kAccept,
               response_headers_.Lookup1(HttpAttributes::kVary));

  // The third fetch uses an IE 9 user agent string, which should result in a
  // Cache-Control: private resource and no Vary header.
  ResetHeadersAndStats();
  SetTimeMs(start_time_ms() + ttl_ms_/2);
  user_agent_ = UserAgentMatcherTestBase::kIe9UserAgent;
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_/2, etag_,
                        start_time_ms() + ttl_ms_/2);
  CheckWarmCache("no_webp_to_ie");
  EXPECT_FALSE(response_headers_.Has(HttpAttributes::kVary));
  ConstStringStarVector cache_controls;
  EXPECT_TRUE(response_headers_.Lookup(
      HttpAttributes::kCacheControl, &cache_controls));
  ASSERT_EQ(2, cache_controls.size());
  EXPECT_STREQ(HttpAttributes::kPrivate, *cache_controls[1]);

  // Fetch again still with kTestUserAgentWebP, but omits the Accept:webp
  // header.  Metadata cache hits.  No input fetch and rewriting.
  // Vary: Accept header should be be added.
  ResetHeadersAndStats();
  SetTimeMs(start_time_ms() + ttl_ms_/2);
  user_agent_ = UserAgentMatcher::kTestUserAgentWebP;
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_/2, etag_,
                        start_time_ms() + ttl_ms_/2);
  CheckWarmCache("no_webp_without_accept");
  EXPECT_STREQ(HttpAttributes::kAccept,
               response_headers_.Lookup1(HttpAttributes::kVary));

  // Fetch another time, switching to just sending Accept: webp and using
  // kTestUserAgentNoWebP.  Metadata cache hits. No input fetch and rewriting.
  // Vary: User-Agent header should be added.
  ResetHeadersAndStats();
  SetTimeMs((start_time_ms() + ttl_ms_/2));
  user_agent_ = UserAgentMatcher::kTestUserAgentNoWebP;
  SetAcceptWebp();
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_/2, etag_,
                        start_time_ms() + ttl_ms_/2);
  CheckWarmCache("back_to_webp");
  EXPECT_STREQ(HttpAttributes::kAccept,
               response_headers_.Lookup1(HttpAttributes::kVary));
}

TEST_F(InPlaceRewriteContextTest, OptimizeForBrowserNoPrivateForIE) {
  // Similar to test above, but set private_not_vary_for_ie to false and omit
  // detailed checking of cache hit statistics, focusing just on a behavioral
  // test.
  options()->set_in_place_wait_for_optimized(true);
  options()->set_private_not_vary_for_ie(false);
  set_optimize_for_browser(true);
  Init();

  // First fetch with kTestUserAgentWebP.
  // Vary: Accept header should be added.
  user_agent_ = UserAgentMatcher::kTestUserAgentWebP;
  SetAcceptWebp();
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_, etag_,
                        start_time_ms());
  EXPECT_STREQ(HttpAttributes::kAccept,
               response_headers_.Lookup1(HttpAttributes::kVary));

  // The second fetch uses a different user agent, kTestUserAgentNoWebP.
  // Vary: Accept header should be be added.
  ResetHeadersAndStats();
  SetTimeMs(start_time_ms() + ttl_ms_/2);
  user_agent_ = UserAgentMatcher::kTestUserAgentNoWebP;
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_/2, etag_,
                        start_time_ms() + ttl_ms_/2);
  EXPECT_STREQ(HttpAttributes::kAccept,
               response_headers_.Lookup1(HttpAttributes::kVary));

  // The third fetch uses an IE 9 user agent string, which should *also* have a
  // Vary: Accept header since private_not_vary_for_ie == false.
  ResetHeadersAndStats();
  SetTimeMs(start_time_ms() + ttl_ms_/2);
  user_agent_ = UserAgentMatcherTestBase::kIe9UserAgent;
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_/2, etag_,
                        start_time_ms() + ttl_ms_/2);
  EXPECT_STREQ(HttpAttributes::kAccept,
               response_headers_.Lookup1(HttpAttributes::kVary));
}

TEST_F(InPlaceRewriteContextTest, AcceptHeaderMerging) {
  options()->set_in_place_wait_for_optimized(true);
  set_optimize_for_browser(true);
  Init();
  SetAcceptWebp();

  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_, etag_,
                        start_time_ms());
  EXPECT_STREQ(HttpAttributes::kAccept,
               response_headers_.Lookup1(HttpAttributes::kVary));

  // We don't actually optimize the Vary: * resource.  See
  // CachingHeaders::HasExplicitNoCacheDirective().  Inexplicably (?), we also
  // change its ttl to 0 in spite of incoming ttl headers.
  FetchAndCheckResponse(cache_jpg_vary_star_url_, "good", true, 0,
                        NULL, start_time_ms());
  EXPECT_STREQ("*", response_headers_.Lookup1(HttpAttributes::kVary));

  // TODO(jmaessen): Right now we're not properly passing through Vary: headers
  // from the fetched resource.  When jmarantz's pending change lands, we will
  // do so, and these tests should be re-enabled accordingly.  Note that I've
  // verified in gdb that we're actually handling pre-existing headers properly
  // (due to a duplicate call; luckily we're idempotent!).

  // FetchAndCheckResponse(cache_jpg_vary_ua_url_, "good:ic", true, ttl_ms_,
  //                       etag_, start_time_ms());
  // EXPECT_STREQ(HttpAttributes::kUserAgent,
  //              response_headers_.Lookup1(HttpAttributes::kVary));

  // FetchAndCheckResponse(cache_jpg_vary_origin_url_, "good:ic", true, ttl_ms_,
  //                       etag_, start_time_ms());
  // ConstStringStarVector accepts;
  // EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kVary, &accepts));
  // ASSERT_EQ(2, accepts.size());
  // EXPECT_STREQ("Origin", *accepts[0]);
  // EXPECT_STREQ(HttpAttributes::kAccept, *accepts[1]);
}

TEST_F(InPlaceRewriteContextTest, NoAcceptHeaderForLossless) {
  // If the image filters says we can only convert to webp lossless + alpha, or
  // can't convert to webp at all, we should not see a Vary: header.
  options()->set_in_place_wait_for_optimized(true);
  set_optimize_for_browser(true);
  Init();
  SetAcceptWebp();

  // First check lossless case.
  img_filter_->set_minimal_webp_support(
      ResourceContext::LIBWEBP_LOSSY_LOSSLESS_ALPHA);
  FetchAndCheckResponse(cache_png_url_, "good:ic", true, ttl_ms_, etag_,
                        start_time_ms());
  EXPECT_FALSE(response_headers_.Has(HttpAttributes::kVary));

  // Then check lossy case where conversion failed (but jpeg was still
  // optimized).
  img_filter_->set_minimal_webp_support(ResourceContext::LIBWEBP_NONE);
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_, etag_,
                        start_time_ms());
  EXPECT_FALSE(response_headers_.Has(HttpAttributes::kVary));
}

TEST_F(InPlaceRewriteContextTest, OptimizeForBrowserNegative) {
  options()->set_in_place_wait_for_optimized(true);
  set_optimize_for_browser(false);
  Init();

  // Vary: User-Agent header should not be added no matter the user-agent.
  user_agent_ = UserAgentMatcher::kTestUserAgentWebP;
  SetAcceptWebp();
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_, etag_,
                        start_time_ms());
  EXPECT_EQ(NULL, response_headers_.Lookup1(HttpAttributes::kVary));

  ResetHeadersAndStats();
  SetTimeMs((start_time_ms() + ttl_ms_/2));
  user_agent_ = UserAgentMatcher::kTestUserAgentNoWebP;
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_/2, etag_,
                        start_time_ms() + ttl_ms_/2);
  EXPECT_EQ(NULL, response_headers_.Lookup1(HttpAttributes::kVary));
}

TEST_F(InPlaceRewriteContextTest, LoadFromFile) {
  options()->file_load_policy()->Associate("http://www.example.com", "/test/");
  WriteFile("/test/cacheable.js", cache_body_ /*"   alert ( 'foo ')   "*/);

  Init();

  // TODO(jmarantz): currently we will not have caching headers on
  // file-input-resources so we default to the implicit cache TTL.
  // We should probably have a new config options for file-input
  // TTL for use with in-place.
  const int64 kIproFileTtl = ResponseHeaders::kDefaultImplicitCacheTtlMs;
  FetchAndCheckResponse(cache_js_url_, cache_body_, true,
                        kIproFileTtl, NULL, start_time_ms());

  // First fetch misses initial cache lookup, succeeds at fetch and inserts
  // result into cache. Also, the resource gets rewritten and the rewritten
  // resource gets inserted into cache.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(1, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  // Note that without file-input resources, we would expect that our
  // TTL would be reduced to ttl_ms_/2.  But it doesn't work like that
  // for files.  The TTL stays the same.
  ResetHeadersAndStats();
  SetTimeMs(start_time_ms() + ttl_ms_/2);
  FetchAndCheckResponse(cache_js_url_, "good:jm", true,
                        kIproFileTtl, etag_,
                        start_time_ms() + ttl_ms_/2);
  // Second fetch hits the metadata cache and the rewritten resource is served
  // out.
  CheckWarmCache("second_fetch_11");
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  // Third fetch is the same exact deal.  The file hasn't actually
  // changed and the existing rewrite still is valid.  The metadata
  // cache does not go stale until the file is actually touched.
  AdvanceTimeMs(2 * ttl_ms_);
  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_js_url_, "good:jm", true,
                        kIproFileTtl, etag_, timer()->NowMs());
  CheckWarmCache("third_fetch");

  // OK let's now move time forward a little and touch the file
  // without changing it.  This results in a total reset back to
  // the original state.  It seems like we could read the file
  // and see if it's changed, but we wind up queuing up the asynchronous
  // rewrite.
  AdvanceTimeMs(1 * Timer::kSecondMs);
  WriteFile("/test/cacheable.js", cache_body_ /*"   alert ( 'foo ')   "*/);
  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_js_url_, cache_body_, true,
                        kIproFileTtl, NULL, timer()->NowMs());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(2, lru_cache()->num_hits());  // ipro-metadata, metadata
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());  // http, metadata, ipro-metadata
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(1, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  AdvanceTimeMs(1 * Timer::kSecondMs);
  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_js_url_, "good:jm", true,
                        kIproFileTtl, etag_, timer()->NowMs());
  CheckWarmCache("second_fetch_after_touch");

  // Now change the content.
  AdvanceTimeMs(1 * Timer::kSecondMs);
  WriteFile("/test/cacheable.js", "new_content");
  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_js_url_, "new_content", true,
                        kIproFileTtl, NULL, timer()->NowMs());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(2, lru_cache()->num_hits());  // ipro-metadata, metadata
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());  // http, metadata, ipro-metadata
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(1, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  AdvanceTimeMs(1 * Timer::kSecondMs);
  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_js_url_, "new_content:jm", true,
                        kIproFileTtl, etag_, timer()->NowMs());
  CheckWarmCache("second_fetch_after_mutation");
}

}  // namespace

}  // namespace net_instaweb
