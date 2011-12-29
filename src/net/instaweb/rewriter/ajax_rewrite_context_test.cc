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

#include "net/instaweb/rewriter/public/ajax_rewrite_context.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/simple_text_filter.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/worker_test_base.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class MessageHandler;

namespace {

// Mock rewriter that appends colon followed by rewriter id to the input
// string. These are used since we need to use specific image / js / css
// rewriters with a specific id, but don't want to test their entire
// functionality.
class FakeRewriter : public SimpleTextFilter::Rewriter {
 public:
  explicit FakeRewriter(const char* id)
      : id_(id), num_rewrites_(0), enabled_(true) {}

  // Stats.
  int num_rewrites() const { return num_rewrites_; }
  void ClearStats() { num_rewrites_ = 0; }
  void set_enabled(bool x) { enabled_ = x; }

 protected:
  REFCOUNT_FRIEND_DECLARATION(FakeRewriter);
  virtual ~FakeRewriter() {}

  virtual bool RewriteText(const StringPiece& url, const StringPiece& in,
                           GoogleString* out,
                           ResourceManager* resource_manager) {
    if (enabled_) {
      ++num_rewrites_;
      StrAppend(out, in, ":", id_);
      return true;
    }
    return false;
  }
  virtual HtmlElement::Attribute* FindResourceAttribute(HtmlElement* element) {
    return NULL;
  }
  virtual OutputResourceKind kind() const { return kRewrittenResource; }
  virtual const char* id() const { return id_; }
  virtual const char* name() const { return "MockFilter"; }

 private:
  OutputResourceKind kind_;
  const char* id_;
  int num_rewrites_;
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(FakeRewriter);
};

class FakeFetch : public AsyncFetch {
 public:
  FakeFetch(WorkerTestBase::SyncPoint* sync, ResponseHeaders* response_headers)
      : done_(false),
        success_(false),
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

 private:
  GoogleString content_;
  bool done_;
  bool success_;
  WorkerTestBase::SyncPoint* sync_;

  DISALLOW_COPY_AND_ASSIGN(FakeFetch);
};

class AjaxRewriteContextTest : public ResourceManagerTestBase {
 protected:
  AjaxRewriteContextTest()
      : cache_html_url_("http://www.example.com/cacheable.html"),
        cache_jpg_url_("http://www.example.com/cacheable.jpg"),
        cache_png_url_("http://www.example.com/cacheable.png"),
        cache_gif_url_("http://www.example.com/cacheable.gif"),
        cache_webp_url_("http://www.example.com/cacheable.webp"),
        cache_js_url_("http://www.example.com/cacheable.js"),
        cache_css_url_("http://www.example.com/cacheable.css"),
        nocache_html_url_("http://www.example.com/nocacheable.html"),
        bad_url_("http://www.example.com/bad.url"),
        cache_body_("good"), nocache_body_("bad"), bad_body_("ugly"),
        ttl_ms_(Timer::kHourMs), etag_("W/PSA-aj-0"),
        original_etag_("original_etag") {}

  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    mock_url_fetcher()->set_fail_on_unexpected(false);

    // Set fetcher result and headers.
    AddResponse(cache_html_url_, kContentTypeHtml, cache_body_, start_time_ms(),
                ttl_ms_, original_etag_, false);
    AddResponse(cache_jpg_url_, kContentTypeJpeg, cache_body_, start_time_ms(),
                ttl_ms_, "", false);
    AddResponse(cache_png_url_, kContentTypePng, cache_body_, start_time_ms(),
                ttl_ms_, original_etag_, true);
    AddResponse(cache_gif_url_, kContentTypeGif, cache_body_, start_time_ms(),
                ttl_ms_, original_etag_, true);
    AddResponse(cache_webp_url_, kContentTypeWebp, cache_body_, start_time_ms(),
                ttl_ms_, original_etag_, true);
    AddResponse(cache_js_url_, kContentTypeJavascript, cache_body_,
                start_time_ms(), ttl_ms_, "", false);
    AddResponse(cache_css_url_, kContentTypeCss, cache_body_, start_time_ms(),
                ttl_ms_, "", false);
    AddResponse(nocache_html_url_, kContentTypeHtml, nocache_body_,
                start_time_ms(), -1, "", false);

    ResponseHeaders bad_headers;
    bad_headers.set_first_line(1, 1, 404, "Not Found");
    bad_headers.SetDate(start_time_ms());
    mock_url_fetcher()->SetResponse(bad_url_, bad_headers, bad_body_);

    img_filter_ = new FakeRewriter(RewriteOptions::kImageCompressionId);
    js_filter_ = new FakeRewriter(RewriteOptions::kJavascriptMinId);
    css_filter_ = new FakeRewriter(RewriteOptions::kCssFilterId);

    rewrite_driver()->AppendRewriteFilter(
        new SimpleTextFilter(img_filter_, rewrite_driver()));
    rewrite_driver()->AppendRewriteFilter(
        new SimpleTextFilter(js_filter_, rewrite_driver()));
    rewrite_driver()->AppendRewriteFilter(
        new SimpleTextFilter(css_filter_, rewrite_driver()));
    rewrite_driver()->AddFilters();

    options()->ClearSignatureForTesting();
    options()->EnableFilter(RewriteOptions::kRecompressImages);
    options()->EnableFilter(RewriteOptions::kRewriteJavascript);
    options()->EnableFilter(RewriteOptions::kRewriteCss);
    options()->set_ajax_rewriting_enabled(true);
    resource_manager()->ComputeSignature(options());
    // Clear stats since we may have added something to the cache.
    ClearStats();
  }

  void AddResponse(const GoogleString& url, const ContentType& content_type,
                   const GoogleString& body, int64 now_ms, int64 ttl_ms,
                   const GoogleString& etag, bool write_to_cache) {
    ResponseHeaders response_headers;
    SetDefaultHeaders(content_type, &response_headers);
    if (ttl_ms > 0) {
      response_headers.SetDateAndCaching(now_ms, ttl_ms);
    } else {
      response_headers.SetDate(now_ms);
      response_headers.Replace(HttpAttributes::kCacheControl, "no-cache");
    }
    if (!etag.empty()) {
      response_headers.Add(HttpAttributes::kEtag, etag);
    }
    mock_url_fetcher()->SetResponse(url, response_headers, body);
    if (write_to_cache) {
      response_headers.ComputeCaching();
      http_cache()->Put(url, &response_headers, body, message_handler());
    }
  }

  void SetDefaultHeaders(const ContentType& content_type,
                         ResponseHeaders* header) const {
    header->set_major_version(1);
    header->set_minor_version(1);
    header->SetStatusAndReason(HttpStatus::kOK);
    header->Replace(HttpAttributes::kContentType, content_type.mime_type());
  }

  void FetchAndCheckResponse(const GoogleString& url,
                             const GoogleString& expected_body,
                             bool expected_success,
                             int64 expected_ttl,
                             const char* etag,
                             int64 date_ms) {
    WorkerTestBase::SyncPoint sync(resource_manager()->thread_system());
    FakeFetch mock_fetch(&sync, &response_headers_);
    mock_fetch.set_request_headers(&request_headers_);

    rewrite_driver()->Clear();
    rewrite_driver()->set_async_fetcher(counting_url_async_fetcher());
    rewrite_driver()->FetchResource(url, &mock_fetch);
    sync.Wait();
    rewrite_driver()->WaitForShutDown();
    EXPECT_TRUE(mock_fetch.done());
    EXPECT_EQ(expected_success, mock_fetch.success());
    EXPECT_EQ(expected_body, mock_fetch.content());
    EXPECT_EQ(expected_ttl, response_headers_.cache_ttl_ms());
    EXPECT_STREQ(etag, response_headers_.Lookup1(HttpAttributes::kEtag));
    EXPECT_EQ(date_ms, response_headers_.date_ms());
  }

  void ResetTest() {
    request_headers_.Clear();
    response_headers_.Clear();
    img_filter_->ClearStats();
    js_filter_->ClearStats();
    css_filter_->ClearStats();
    ResourceManagerTestBase::ClearStats();
  }

  void ExpectAjaxImageSuccessFlow(const GoogleString& url) {
    FetchAndCheckResponse(url, cache_body_, true, ttl_ms_,
                          original_etag_, start_time_ms());

    // First fetch misses initial cache lookup, succeeds at fetch and inserts
    // result into cache. Also, the resource gets rewritten and the rewritten
    // resource gets inserted into cache.
    EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
    EXPECT_EQ(0, http_cache()->cache_hits()->Get());
    EXPECT_EQ(0, http_cache()->cache_misses()->Get());
    EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
    EXPECT_EQ(0, lru_cache()->num_hits());
    EXPECT_EQ(2, lru_cache()->num_misses());
    EXPECT_EQ(3, lru_cache()->num_inserts());
    EXPECT_EQ(1, img_filter_->num_rewrites());
    EXPECT_EQ(0, js_filter_->num_rewrites());
    EXPECT_EQ(0, css_filter_->num_rewrites());

    ResetTest();
    FetchAndCheckResponse(url, "good:ic", true, ttl_ms_, etag_,
                        start_time_ms());
    // Second fetch hits the metadata cache and the rewritten resource is
    // served out.
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

    mock_timer()->AdvanceMs(2 * ttl_ms_);
    ResetTest();
    FetchAndCheckResponse(url, cache_body_, true, ttl_ms_,
                          original_etag_, start_time_ms());
    // The metadata and cache entry is stale now. Fetch the content and serve
    // it out without rewriting. Don't attempt to rewrite the content as it
    // is stale.
    EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
    EXPECT_EQ(0, http_cache()->cache_hits()->Get());
    EXPECT_EQ(0, http_cache()->cache_misses()->Get());
    EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
    EXPECT_EQ(1, lru_cache()->num_hits());
    EXPECT_EQ(0, lru_cache()->num_misses());
    EXPECT_EQ(0, lru_cache()->num_inserts());
    EXPECT_EQ(0, img_filter_->num_rewrites());
    EXPECT_EQ(0, js_filter_->num_rewrites());
    EXPECT_EQ(0, css_filter_->num_rewrites());
  }

  FakeRewriter* img_filter_;
  FakeRewriter* js_filter_;
  FakeRewriter* css_filter_;

  RequestHeaders request_headers_;
  ResponseHeaders response_headers_;

  const GoogleString cache_html_url_;
  const GoogleString cache_jpg_url_;
  const GoogleString cache_png_url_;
  const GoogleString cache_gif_url_;
  const GoogleString cache_webp_url_;
  const GoogleString cache_js_url_;
  const GoogleString cache_css_url_;

  const GoogleString nocache_html_url_;
  const GoogleString bad_url_;

  const GoogleString cache_body_;
  const GoogleString nocache_body_;
  const GoogleString bad_body_;

  const int ttl_ms_;
  const char* etag_;
  const char* original_etag_;
};

TEST_F(AjaxRewriteContextTest, CacheableHtmlUrlNoRewriting) {
  // All these entries find no ajax rewrite metadata and no rewriting happens.
  FetchAndCheckResponse(cache_html_url_, cache_body_, true, ttl_ms_,
                        original_etag_, start_time_ms());
  // First fetch misses initial cache lookup, succeeds at fetch and inserts
  // result into cache.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  ResetTest();
  FetchAndCheckResponse(cache_html_url_, cache_body_, true, ttl_ms_,
                        original_etag_, start_time_ms());
  // Second fetch hits initial cache lookup and no extra fetches are needed.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  mock_timer()->AdvanceMs(2 * ttl_ms_);
  ResetTest();
  FetchAndCheckResponse(cache_html_url_, cache_body_, true, ttl_ms_,
                        original_etag_, start_time_ms());
  // Cache entry is stale, so we must fetch again.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
}

TEST_F(AjaxRewriteContextTest, CacheableJpgUrlRewritingSucceeds) {
  FetchAndCheckResponse(cache_jpg_url_, cache_body_, true, ttl_ms_, NULL,
                        start_time_ms());

  // First fetch misses initial cache lookup, succeeds at fetch and inserts
  // result into cache. Also, the resource gets rewritten and the rewritten
  // resource gets inserted into cache.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());
  EXPECT_EQ(1, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  ResetTest();
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_, etag_,
                        start_time_ms());
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

  ResetTest();
  // We get a 304 if we send a request with an If-None-Match matching the hash
  // of the rewritten resource.
  request_headers_.Add(HttpAttributes::kIfNoneMatch, etag_);
  FetchAndCheckResponse(cache_jpg_url_, "", true, ttl_ms_, NULL, 0);
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

  ResetTest();
  // The etag doesn't match and hence we serve the full response.
  request_headers_.Add(HttpAttributes::kIfNoneMatch, "no-match");
  FetchAndCheckResponse(cache_jpg_url_, "good:ic", true, ttl_ms_, etag_,
                        start_time_ms());
  EXPECT_EQ(HttpStatus::kOK, response_headers_.status_code());
  // We hit the metadata cache, but the etag doesn't match so we fetch the
  // rewritten resource from the HTTPCache and serve it out.
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

  mock_timer()->AdvanceMs(2 * ttl_ms_);
  ResetTest();
  FetchAndCheckResponse(cache_jpg_url_, cache_body_, true, ttl_ms_, NULL,
                        start_time_ms());
  // The metadata and cache entry is stale now. Fetch the content and serve it
  // out without rewriting. Don't attempt to rewrite the content as it is stale.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
}

TEST_F(AjaxRewriteContextTest, CacheablePngUrlRewritingSucceeds) {
  ExpectAjaxImageSuccessFlow(cache_png_url_);
}

TEST_F(AjaxRewriteContextTest, CacheableiGifUrlRewritingSucceeds) {
  ExpectAjaxImageSuccessFlow(cache_gif_url_);
}

TEST_F(AjaxRewriteContextTest, CacheableWebpUrlRewritingSucceeds) {
  ExpectAjaxImageSuccessFlow(cache_webp_url_);
}

TEST_F(AjaxRewriteContextTest, CacheablePngUrlRewritingFails) {
  // Setup the image filter to fail at rewriting.
  img_filter_->set_enabled(false);
  FetchAndCheckResponse(cache_png_url_, cache_body_, true, ttl_ms_,
                        original_etag_, start_time_ms());

  // First fetch misses initial cache lookup, succeeds at fetch and inserts
  // result into cache. The rewrite fails and metadata is inserted into the
  // cache indicating that the rewriting didn't succeed.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  ResetTest();
  FetchAndCheckResponse(cache_png_url_, cache_body_, true, ttl_ms_,
                        original_etag_, start_time_ms());
  // Second fetch hits the metadata cache, sees that the rewrite failed and
  // fetches and serves the original resource from cache.
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
}

TEST_F(AjaxRewriteContextTest, CacheableJsUrlRewritingSucceeds) {
  FetchAndCheckResponse(cache_js_url_, cache_body_, true, ttl_ms_, NULL,
                        start_time_ms());

  // First fetch misses initial cache lookup, succeeds at fetch and inserts
  // result into cache. Also, the resource gets rewritten and the rewritten
  // resource gets inserted into cache.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(1, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  ResetTest();
  FetchAndCheckResponse(cache_js_url_, "good:jm", true, ttl_ms_, etag_,
                        start_time_ms());
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

  mock_timer()->AdvanceMs(2 * ttl_ms_);
  ResetTest();
  FetchAndCheckResponse(cache_js_url_, cache_body_, true, ttl_ms_, NULL,
                        start_time_ms());
  // The metadata and cache entry is stale now. Fetch the content and serve it
  // out without rewriting. Don't attempt to rewrite the content as it is stale.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
}

TEST_F(AjaxRewriteContextTest, CacheableCssUrlIfCssRewritingDisabled) {
  options()->ClearSignatureForTesting();
  options()->DisableFilter(RewriteOptions::kRewriteCss);
  resource_manager()->ComputeSignature(options());
  FetchAndCheckResponse(cache_css_url_, cache_body_, true, ttl_ms_, NULL,
                        start_time_ms());

  // First fetch succeeds at the fetcher, no rewriting happens since the css
  // filter is disabled, and metadata indicating a rewriting failure gets
  // inserted into cache.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  ResetTest();
  FetchAndCheckResponse(cache_css_url_, cache_body_, true, ttl_ms_, NULL,
                        start_time_ms());

  // Second fetch hits the metadata cache, finds that the result is not
  // optimizable. It then looks up cache for the original, does not find it and
  // succeeds at the fetcher.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
}

TEST_F(AjaxRewriteContextTest, CacheableCssUrlRewritingSucceeds) {
  FetchAndCheckResponse(cache_css_url_, cache_body_, true, ttl_ms_, NULL,
                        start_time_ms());

  // First fetch misses initial cache lookup, succeeds at fetch and inserts
  // result into cache. Also, the resource gets rewritten and the rewritten
  // resource gets inserted into cache.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(1, css_filter_->num_rewrites());

  ResetTest();
  FetchAndCheckResponse(cache_css_url_, "good:cf", true, ttl_ms_, etag_,
                        start_time_ms());
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

  mock_timer()->AdvanceMs(2 * ttl_ms_);
  ResetTest();
  FetchAndCheckResponse(cache_css_url_, cache_body_, true, ttl_ms_, NULL,
                        start_time_ms());
  // The metadata and cache entry is stale now. Fetch the content and serve it
  // out without rewriting. Don't attempt to rewrite the content as it is stale.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
}

TEST_F(AjaxRewriteContextTest, NonCacheableUrlNoRewriting) {
  FetchAndCheckResponse(nocache_html_url_, nocache_body_, true, 0, NULL,
                        start_time_ms());
  // First fetch misses initial cache lookup, succeeds at fetch and we don't
  // insert into cache because it's not cacheable. Don't attempt to rewrite
  // this since its not cacheable.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
}

TEST_F(AjaxRewriteContextTest, BadUrlNoRewriting) {
  FetchAndCheckResponse(bad_url_, bad_body_, true, 0, NULL, start_time_ms());
  // First fetch misses initial cache lookup, succeeds at fetch and we don't
  // insert into cache because it's not cacheable. Don't attempt to rewrite
  // this since its not cacheable.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
}

TEST_F(AjaxRewriteContextTest, FetchFailedNoRewriting) {
  FetchAndCheckResponse("http://www.notincache.com", "", false, 0, NULL, 0);
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
}

}  // namespace

}  // namespace net_instaweb
