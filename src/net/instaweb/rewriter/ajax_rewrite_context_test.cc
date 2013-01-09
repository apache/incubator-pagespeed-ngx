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

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/fake_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_scheduler.h"
#include "net/instaweb/util/public/scheduler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/worker_test_base.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class HtmlElement;
class MessageHandler;

namespace {

// A filter that that appends ':id' to the input contents and counts the number
// of rewrites it has performed. It also has the ability to simulate a long
// rewrite to test exceeding the rewrite deadline.
class FakeFilter : public RewriteFilter {
 public:
  class Context : public SingleRewriteContext {
   public:
    Context(FakeFilter* filter, RewriteDriver* driver,
            RewriteContext* parent)
        : SingleRewriteContext(driver, parent, NULL), filter_(filter) {}
    virtual ~Context() {}

    void RewriteSingle(const ResourcePtr& input,
                       const OutputResourcePtr& output) {
      if (filter_->exceed_deadline()) {
        int64 wakeup_us = Driver()->scheduler()->timer()->NowUs() +
            (1000 * GetRewriteDeadlineAlarmMs());
        Function* closure = MakeFunction(this, &Context::DoRewriteSingle,
                                         input, output);
        Driver()->scheduler()->AddAlarm(wakeup_us, closure);
      } else {
        DoRewriteSingle(input, output);
      }
    }

    void DoRewriteSingle(const ResourcePtr input,
                         OutputResourcePtr output) {
      RewriteResult result = kRewriteFailed;
      GoogleString rewritten;

      if (filter_->enabled()) {
        // TODO(jkarlin): Writing to the filter from a context is not thread
        // safe.
        filter_->IncRewrites();
        StrAppend(&rewritten, input->contents(), ":", filter_->id());

        // Set the output type here to make sure that the CachedResult url
        // field has the correct extension for the type.
        const ContentType* output_type = &kContentTypeText;
        if (filter_->output_content_type() != NULL) {
          output_type = filter_->output_content_type();
        } else if (input->type() != NULL) {
          output_type = input->type();
        }
        ResourceVector rv = ResourceVector(1, input);
        if (Driver()->Write(rv, rewritten, output_type,
                            input->charset(), output.get())) {
          result = kRewriteOk;
        }
      }

      RewriteDone(result, 0);
    }
    virtual const char* id() const { return filter_->id(); }
    virtual OutputResourceKind kind() const { return filter_->kind(); }

   private:
    FakeFilter* filter_;
  };

  FakeFilter(const char* id, RewriteDriver* rewrite_driver)
      : RewriteFilter(rewrite_driver), id_(id), exceed_deadline_(false),
        enabled_(true), num_rewrites_(0), output_content_type_(NULL) {}

  virtual ~FakeFilter() {}

  virtual void StartDocumentImpl() {}
  virtual void EndElementImpl(HtmlElement* element) {}
  virtual void StartElementImpl(HtmlElement* element) {}
  virtual RewriteContext* MakeRewriteContext() {
    return new FakeFilter::Context(this, driver_, NULL);
  }
  virtual RewriteContext* MakeNestedRewriteContext(
      RewriteContext* parent, const ResourceSlotPtr& slot) {
    RewriteContext* context = new FakeFilter::Context(this, NULL, parent);
    context->AddSlot(slot);
    return context;
  }
  int num_rewrites() const { return num_rewrites_; }
  void ClearStats() { num_rewrites_ = 0; }
  void set_enabled(bool x) { enabled_ = x; }
  bool enabled() { return enabled_; }
  bool exceed_deadline() { return exceed_deadline_; }
  void set_exceed_deadline(bool x) { exceed_deadline_ = x; }
  void IncRewrites() { ++num_rewrites_; }
  void set_output_content_type(const ContentType* type) {
    output_content_type_ = type;
  }
  const ContentType* output_content_type() { return output_content_type_; }

 protected:
  virtual const char* id() const { return id_; }
  virtual OutputResourceKind kind() const { return kRewrittenResource; }
  virtual const char* Name() const { return "MockFilter"; }
  virtual bool ComputeOnTheFly() const { return false; }

 private:
  const char* id_;
  bool exceed_deadline_;
  bool enabled_;
  int num_rewrites_;
  const ContentType* output_content_type_;
};

class FakeFetch : public AsyncFetch {
 public:
  FakeFetch(const RequestContextPtr& request_context,
            WorkerTestBase::SyncPoint* sync,
            ResponseHeaders* response_headers)
      : AsyncFetch(request_context),
        done_(false),
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

class AjaxRewriteContextTest : public RewriteTestBase {
 protected:
  static const bool kWriteToCache = true;
  static const bool kNoWriteToCache = false;
  static const bool kNoTransform = true;
  static const bool kTransform = false;
  AjaxRewriteContextTest()
      : img_filter_(NULL),
        js_filter_(NULL),
        css_filter_(NULL),
        cache_html_url_("http://www.example.com/cacheable.html"),
        cache_jpg_url_("http://www.example.com/cacheable.jpg"),
        cache_jpg_notransform_url_("http://www.example.com/notransform.jpg"),
        cache_png_url_("http://www.example.com/cacheable.png"),
        cache_gif_url_("http://www.example.com/cacheable.gif"),
        cache_webp_url_("http://www.example.com/cacheable.webp"),
        cache_js_url_("http://www.example.com/cacheable.js"),
        cache_css_url_("http://www.example.com/cacheable.css"),
        nocache_html_url_("http://www.example.com/nocacheable.html"),
        cache_js_no_max_age_url_("http://www.example.com/cacheablemod.js"),
        bad_url_("http://www.example.com/bad.url"),
        rewritten_jpg_url_(
            "http://www.example.com/cacheable.jpg.pagespeed.ic.0.jpg"),
        cache_body_("good"), nocache_body_("bad"), bad_body_("ugly"),
        ttl_ms_(Timer::kHourMs), etag_("W/\"PSA-aj-0\""),
        original_etag_("original_etag"),
        exceed_deadline_(false),
        oversized_stream_(NULL) {}

  virtual void Init() {
    SetTimeMs(start_time_ms());
    mock_url_fetcher()->set_fail_on_unexpected(false);

    // Set fetcher result and headers.
    AddResponse(cache_html_url_, kContentTypeHtml, cache_body_, start_time_ms(),
                ttl_ms_, original_etag_, kNoWriteToCache, kTransform);
    AddResponse(cache_jpg_url_, kContentTypeJpeg, cache_body_, start_time_ms(),
                ttl_ms_, "", kNoWriteToCache, kTransform);
    AddResponse(cache_jpg_notransform_url_, kContentTypeJpeg, cache_body_,
                start_time_ms(), ttl_ms_, "", kNoWriteToCache, kNoTransform);
    AddResponse(cache_png_url_, kContentTypePng, cache_body_, start_time_ms(),
                ttl_ms_, original_etag_, kWriteToCache, kTransform);
    AddResponse(cache_gif_url_, kContentTypeGif, cache_body_, start_time_ms(),
                ttl_ms_, original_etag_, kWriteToCache, kTransform);
    AddResponse(cache_webp_url_, kContentTypeWebp, cache_body_, start_time_ms(),
                ttl_ms_, original_etag_, kWriteToCache, kTransform);
    AddResponse(cache_js_url_, kContentTypeJavascript, cache_body_,
                start_time_ms(), ttl_ms_, "", kNoWriteToCache, kTransform);
    AddResponse(cache_css_url_, kContentTypeCss, cache_body_, start_time_ms(),
                ttl_ms_, "", kNoWriteToCache, kTransform);
    AddResponse(nocache_html_url_, kContentTypeHtml, nocache_body_,
                start_time_ms(), -1, "", kNoWriteToCache, kTransform);
    AddResponse(cache_js_no_max_age_url_, kContentTypeJavascript, cache_body_,
                start_time_ms(), 0, "", kNoWriteToCache, kTransform);


    ResponseHeaders bad_headers;
    bad_headers.set_first_line(1, 1, 404, "Not Found");
    bad_headers.SetDate(start_time_ms());
    mock_url_fetcher()->SetResponse(bad_url_, bad_headers, bad_body_);

    img_filter_ = new FakeFilter(RewriteOptions::kImageCompressionId,
                                 rewrite_driver());
    js_filter_ = new FakeFilter(RewriteOptions::kJavascriptMinId,
                                rewrite_driver());
    css_filter_ = new FakeFilter(RewriteOptions::kCssFilterId,
                                 rewrite_driver());

    rewrite_driver()->AppendRewriteFilter(img_filter_);
    rewrite_driver()->AppendRewriteFilter(js_filter_);
    rewrite_driver()->AppendRewriteFilter(css_filter_);

    options()->ClearSignatureForTesting();
    AddRecompressImageFilters();
    options()->EnableFilter(RewriteOptions::kRewriteJavascript);
    options()->EnableFilter(RewriteOptions::kRewriteCss);
    options()->set_ajax_rewriting_enabled(true);
    server_context()->ComputeSignature(options());
    // Clear stats since we may have added something to the cache.
    ClearStats();

    oversized_stream_ = statistics()->GetVariable(
        AjaxRewriteContext::kInPlaceOversizedOptStream);
  }

  void AddResponse(const GoogleString& url, const ContentType& content_type,
                   const GoogleString& body, int64 now_ms, int64 ttl_ms,
                   const GoogleString& etag, bool write_to_cache,
                   bool no_transform) {
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
    if (no_transform) {
      response_headers.Replace(HttpAttributes::kCacheControl, "no-transform");
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
    js_filter_->set_exceed_deadline(exceed_deadline_);
    img_filter_->set_exceed_deadline(exceed_deadline_);
    css_filter_->set_exceed_deadline(exceed_deadline_);

    WorkerTestBase::SyncPoint sync(server_context()->thread_system());
    FakeFetch mock_fetch(RequestContext::NewTestRequestContext(
        server_context()->thread_system()), &sync, &response_headers_);
    mock_fetch.set_request_headers(&request_headers_);

    ClearRewriteDriver();
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
    EXPECT_EQ(expected_success, mock_fetch.success());
    EXPECT_EQ(expected_body, mock_fetch.content());
    EXPECT_EQ(expected_ttl, response_headers_.cache_ttl_ms());
    EXPECT_STREQ(etag, response_headers_.Lookup1(HttpAttributes::kEtag));
    EXPECT_EQ(date_ms, response_headers_.date_ms());
  }

  void ResetHeadersAndStats() {
    request_headers_.Clear();
    response_headers_.Clear();
    img_filter_->ClearStats();
    js_filter_->ClearStats();
    css_filter_->ClearStats();
    RewriteTestBase::ClearStats();
  }

  void ExpectAjaxImageSuccessFlow(const GoogleString& url) {
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

  FakeFilter* img_filter_;
  FakeFilter* js_filter_;
  FakeFilter* css_filter_;

  RequestHeaders request_headers_;
  ResponseHeaders response_headers_;

  const GoogleString cache_html_url_;
  const GoogleString cache_jpg_url_;
  const GoogleString cache_jpg_notransform_url_;
  const GoogleString cache_png_url_;
  const GoogleString cache_gif_url_;
  const GoogleString cache_webp_url_;
  const GoogleString cache_js_url_;
  const GoogleString cache_css_url_;
  const GoogleString nocache_html_url_;
  const GoogleString cache_js_no_max_age_url_;
  const GoogleString bad_url_;
  const GoogleString rewritten_jpg_url_;

  const GoogleString cache_body_;
  const GoogleString nocache_body_;
  const GoogleString bad_body_;

  const int ttl_ms_;
  const char* etag_;
  const char* original_etag_;
  bool exceed_deadline_;

  Variable* oversized_stream_;
};

TEST_F(AjaxRewriteContextTest, CacheableHtmlUrlNoRewriting) {
  // All these entries find no ajax rewrite metadata and no rewriting happens.
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

TEST_F(AjaxRewriteContextTest, WaitForOptimizedFirstRequest) {
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
  EXPECT_EQ(0, oversized_stream_->Get());
}

TEST_F(AjaxRewriteContextTest, WaitForOptimizeWithDisabledFilter) {
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
  GoogleString expected_etag = StrCat("W/\"PSA-", hasher()->Hash(cache_body_),
                                      "\"");
  FetchAndCheckResponse(cache_jpg_url_, cache_body_, true, ttl_ms_,
                        expected_etag.c_str(), start_time_ms());
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
  EXPECT_EQ(0, oversized_stream_->Get());
}

TEST_F(AjaxRewriteContextTest, WaitForOptimizeNoTransform) {
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
  EXPECT_EQ(2, lru_cache()->num_inserts());  // original resource + aj metadata
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  EXPECT_TRUE(response_headers_.HasValue(HttpAttributes::kCacheControl,
                                         "no-transform"));

  ResetHeadersAndStats();

  // Don't rewrite since it's no-transform.
  FetchAndCheckResponse(cache_jpg_notransform_url_, cache_body_, true,
                        ttl_ms_, "W/\"PSA-0\"", start_time_ms());
  // The second fetch should return the cached original after seeing that it
  // can't be rewritten.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());
}

TEST_F(AjaxRewriteContextTest, WaitForOptimizeTimeout) {
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
  EXPECT_EQ(0, oversized_stream_->Get());
}

TEST_F(AjaxRewriteContextTest, WaitForOptimizeResourceTooBig) {
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

TEST_F(AjaxRewriteContextTest, CacheableJpgUrlRewritingSucceeds) {
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

  // Delete the rewritten resource from cache to check if reconstruction works.
  lru_cache()->Delete(rewritten_jpg_url_);

  ResetHeadersAndStats();
  // Original resource is served with the date set to start time.
  // The ETag we check for here is the ETag HTTPCache synthesized for
  // the original resource.
  FetchAndCheckResponse(
      cache_jpg_url_, "good", true, ttl_ms_,
      StringPrintf(HTTPCache::kEtagFormat, "0").c_str(), start_time_ms());
  // We find the metadata in cache, but don't find the rewritten resource.
  // Hence, we reconstruct the resource and insert it into cache. We see 2
  // identical reinserts - one for the image rewrite filter metadata and one for
  // the ajax metadata.
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

TEST_F(AjaxRewriteContextTest, CacheablePngUrlRewritingSucceeds) {
  Init();
  ExpectAjaxImageSuccessFlow(cache_png_url_);
}

TEST_F(AjaxRewriteContextTest, CacheablePngUrlRewritingSucceedsWithShards) {
  Init();
  const char kShard1[] = "http://s1.example.com/";
  const char kShard2[] = "http://s2.example.com/";

  DomainLawyer* lawyer = options()->domain_lawyer();
  lawyer->AddShard("http://www.example.com", StrCat(kShard1, ",", kShard2),
                   message_handler());
  ExpectAjaxImageSuccessFlow(cache_png_url_);
}

TEST_F(AjaxRewriteContextTest, CacheableiGifUrlRewritingSucceeds) {
  Init();
  ExpectAjaxImageSuccessFlow(cache_gif_url_);
}

TEST_F(AjaxRewriteContextTest, CacheableWebpUrlRewritingSucceeds) {
  Init();
  ExpectAjaxImageSuccessFlow(cache_webp_url_);
}

TEST_F(AjaxRewriteContextTest, CacheablePngUrlRewritingFails) {
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

TEST_F(AjaxRewriteContextTest, CacheableJsUrlRewritingWithStaleServing) {
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
  // Second fetch hits the metadata cache and the rewritten resource is served
  // out.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  // Two cache hits, one for the ajax metadata and one for the rewritten
  // resource.
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, img_filter_->num_rewrites());
  EXPECT_EQ(0, js_filter_->num_rewrites());
  EXPECT_EQ(0, css_filter_->num_rewrites());

  SetTimeMs(start_time_ms() + (3 * ttl_ms_) / 2);
  ResetHeadersAndStats();
  FetchAndCheckResponse(cache_js_url_, "good:jm", true,
                        ResponseHeaders::kImplicitCacheTtlMs, etag_,
                        start_time_ms() + (3 * ttl_ms_) / 2);
  // The metadata and cache entry is stale now. However, since stale rewriting
  // is enabled, we serve the rewritten resource with a cache ttl of 5 minutes.
  // We also trigger an asynchronous fetch for the original resource and insert
  // it into cache and update the metadata.
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

TEST_F(AjaxRewriteContextTest, CacheableJsUrlModifiedImplicitCacheTtl) {
  Init();
  response_headers_.set_implicit_cache_ttl_ms(500 * Timer::kSecondMs);
  FetchAndCheckResponse(cache_js_no_max_age_url_, cache_body_, true,
                        500 * Timer::kSecondMs, NULL, start_time_ms());
}

TEST_F(AjaxRewriteContextTest, CacheableCssUrlIfCssRewritingDisabled) {
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
                        StringPrintf(HTTPCache::kEtagFormat, "0").c_str(),
                        start_time_ms());

  // Second fetch hits the metadata cache, finds that the result is not
  // optimizable. It then looks up cache for the original and finds it.
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

TEST_F(AjaxRewriteContextTest, CacheableCssUrlRewritingSucceeds) {
  Init();
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
  FetchAndCheckResponse(cache_css_url_, cache_body_, true, ttl_ms_, NULL,
                        timer()->NowMs());
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
}

TEST_F(AjaxRewriteContextTest, NonCacheableUrlNoRewriting) {
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

TEST_F(AjaxRewriteContextTest, BadUrlNoRewriting) {
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

TEST_F(AjaxRewriteContextTest, FetchFailedNoRewriting) {
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

TEST_F(AjaxRewriteContextTest, HandleResourceCreationFailure) {
  // Regression test. Trying to in-place optimize https resources with
  // a fetcher that didn't support https would fail to invoke the callbacks
  // and leak the rewrite driver.
  Init();
  factory()->mock_url_async_fetcher()->set_fetcher_supports_https(false);
  FetchAndCheckResponse("https://www.example.com", "", false, 0, NULL, 0);
}

TEST_F(AjaxRewriteContextTest, ResponseHeaderMimeTypeUpdate) {
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

}  // namespace

}  // namespace net_instaweb
