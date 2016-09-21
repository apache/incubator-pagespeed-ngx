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

// Unit-test the lru cache

#include "net/instaweb/http/public/http_cache.h"

#include <cstddef>                     // for size_t

#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/inflating_fetch.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_hasher.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/lru_cache.h"
#include "pagespeed/kernel/cache/write_through_cache.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/util/gzip_inflater.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/kernel/util/simple_stats.h"
#include "pagespeed/opt/logging/request_timing_info.h"

namespace {
// Set the cache size large enough so nothing gets evicted during this test.
const int kMaxSize = 10000;
const char kStartDate[] = "Sun, 16 Dec 1979 02:27:45 GMT";
const char kUrl[] = "http://www.test.com/";
const char kUrl2[] = "http://www.test.com/2";
const char kUrl3[] = "http://www.test.com/3";
const char kHttpsUrl[] = "https://www.test.com/";
const char kFragment[] = "www.test.com";
const char kFragment2[] = "www.other.com";
const char kCssText[] = ".a         {color:blue;}                    "
    "                                                                "
    "                                                                "
    "                                                                "
    "                                                                "
    "                                                                "
    "                                                                ";

const size_t kPayloadSizeWithoutHeaders = STATIC_STRLEN(kUrl) +
    STATIC_STRLEN(kFragment) + STATIC_STRLEN(kCssText);
}  // namespace

namespace net_instaweb {

class HTTPCacheTest : public testing::Test {
 protected:
  // Helper class for calling Get and Query methods on cache implementations
  // that are blocking in nature (e.g. in-memory LRU or blocking file-system).
  class Callback : public HTTPCache::Callback {
   public:
    explicit Callback(const RequestContextPtr& ctx) : HTTPCache::Callback(ctx) {
      called_ = false;
      cache_valid_ = true;
      fresh_ = true;
      override_cache_ttl_ms_= -1;
      http_value()->Clear();
      fallback_http_value()->Clear();
    }
    virtual void Done(HTTPCache::FindResult result) {
      called_ = true;
      result_ = result;
    }
    virtual bool IsCacheValid(const GoogleString& key,
                              const ResponseHeaders& headers) {
      // For unit testing, we are simply stubbing IsCacheValid.
      return cache_valid_;
    }
    virtual bool IsFresh(const ResponseHeaders& headers) {
      // For unit testing, we are simply stubbing IsFresh.
      return fresh_;
    }
    virtual int64 OverrideCacheTtlMs(const GoogleString& key) {
      return override_cache_ttl_ms_;
    }

    // Detailed Vary handling is tested in ResponseHeadersTest.
    virtual ResponseHeaders::VaryOption RespectVaryOnResources() const {
      return ResponseHeaders::kRespectVaryOnResources;
    }

    bool called_;
    HTTPCache::FindResult result_;
    bool cache_valid_;
    bool fresh_;
    int64 override_cache_ttl_ms_;
  };

  static int64 ParseDate(const char* start_date) {
    int64 time_ms;
    ResponseHeaders::ParseTime(start_date, &time_ms);
    return time_ms;
  }

  const HTTPCache::FindResult kFoundResult;
  const HTTPCache::FindResult kNotFoundResult;

  HTTPCacheTest()
      : kFoundResult(HTTPCache::kFound, kFetchStatusOK),
        kNotFoundResult(HTTPCache::kNotFound, kFetchStatusNotSet),
        thread_system_(Platform::CreateThreadSystem()),
        simple_stats_(thread_system_.get()),
        mock_timer_(thread_system_->NewMutex(), ParseDate(kStartDate)),
        lru_cache_(kMaxSize) {
    HTTPCache::InitStats(&simple_stats_);
    http_cache_.reset(new HTTPCache(&lru_cache_, &mock_timer_, &mock_hasher_,
                                    &simple_stats_));
  }

  void InitHeaders(ResponseHeaders* headers, const char* cache_control) {
    headers->Add("name", "value");
    headers->Add("Date", kStartDate);
    if (cache_control != NULL) {
      headers->Add("Cache-control", cache_control);
    }
    headers->SetStatusAndReason(HttpStatus::kOK);
    headers->ComputeCaching();
  }

  int GetStat(const char* name) { return simple_stats_.LookupValue(name); }

  HTTPCache::FindResult FindWithCallback(
      const GoogleString& key, const GoogleString& fragment, HTTPValue* value,
      ResponseHeaders* headers, Callback* callback) {
    http_cache_->Find(key, fragment, &message_handler_, callback);
    EXPECT_TRUE(callback->called_);
    if (callback->result_.status == HTTPCache::kFound) {
      value->Link(callback->http_value());
    }
    headers->CopyFrom(*callback->response_headers());
    return callback->result_;
  }

  HTTPCache::FindResult Find(
      const GoogleString& key, const GoogleString& fragment, HTTPValue* value,
      ResponseHeaders* headers) {
    scoped_ptr<Callback> callback(NewCallback());
    return FindWithCallback(
        key, fragment, value, headers, callback.get());
  }

  HTTPCache::FindResult FindAcceptGzip(
      const GoogleString& key, const GoogleString& fragment, HTTPValue* value,
      ResponseHeaders* headers) {
    scoped_ptr<Callback> callback(NewCallback());
    callback->request_context()->SetAcceptsGzip(true);
    return FindWithCallback(
        key, fragment, value, headers, callback.get());
  }

  HTTPCache::FindResult Find(
      const GoogleString& key, const GoogleString& fragment, HTTPValue* value,
      ResponseHeaders* headers, bool cache_valid) {
    scoped_ptr<Callback> callback(NewCallback());
    callback->cache_valid_ = cache_valid;
    return FindWithCallback(
        key, fragment, value, headers, callback.get());
  }

  Callback* NewCallback() {
    return new Callback(RequestContext::NewTestRequestContext(
        thread_system_.get()));
  }

  void Put(const GoogleString& key, const GoogleString& fragment,
           ResponseHeaders* headers, StringPiece content) {
    http_cache_->Put(key, fragment, RequestHeaders::Properties(),
                     ResponseHeaders::kRespectVaryOnResources,
                     headers, content, &message_handler_);
  }

  void PopulateGzippedEntry(const char* cache_control,
                            ResponseHeaders* response_headers) {
    PutGzippedEntry(cache_control, response_headers, false, 9);
  }

  void PutGzippedEntry(const char* cache_control,
                       ResponseHeaders* response_headers,
                       bool gzip_data_first,
                       int compression_level) {
    InitHeaders(response_headers, cache_control);
    response_headers->Add(HttpAttributes::kContentType, "text/css");
    GoogleString value;
    if (gzip_data_first) {
      StringWriter writer(&value);
      ASSERT_TRUE(GzipInflater::Deflate(
          kCssText, GzipInflater::InflateType::kGzip, &writer));
      response_headers->Add(HttpAttributes::kContentEncoding, "gzip");
    } else {
      value = kCssText;
    }
    http_cache_->SetCompressionLevel(compression_level);
    response_headers->ComputeCaching();
    Put(kUrl, kFragment, response_headers, value);
  }

  size_t CacheSizeAfterPut(int cache_compression_level,
                           bool compress_before_put) {
    ResponseHeaders response_headers;
    PutGzippedEntry("max-age:300", &response_headers, compress_before_put,
                    cache_compression_level);
    HTTPValue value;
    response_headers.Clear();
    HTTPCache::FindResult found = Find(kUrl, kFragment, &value,
                                       &response_headers);
    StringPiece contents;
    CHECK(value.ExtractContents(&contents));
    EXPECT_STREQ(kCssText, contents);
    EXPECT_EQ(kFoundResult, found);
    return lru_cache_.size_bytes();
  }

  scoped_ptr<ThreadSystem> thread_system_;
  SimpleStats simple_stats_;
  MockTimer mock_timer_;
  MockHasher mock_hasher_;
  LRUCache lru_cache_;
  scoped_ptr<HTTPCache> http_cache_;
  GoogleMessageHandler message_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HTTPCacheTest);
};

// Simple flow of putting in an item, getting it.
TEST_F(HTTPCacheTest, PutGet) {
  simple_stats_.Clear();

  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl, kFragment, &meta_data_in, "content");
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  HTTPValue value;
  HTTPCache::FindResult found = Find(kUrl, kFragment, &value, &meta_data_out);
  ASSERT_EQ(kFoundResult, found);
  ASSERT_TRUE(meta_data_out.headers_complete());
  StringPiece contents;
  ASSERT_TRUE(value.ExtractContents(&contents));
  ConstStringStarVector values;
  ASSERT_TRUE(meta_data_out.Lookup("name", &values));
  ASSERT_EQ(static_cast<size_t>(1), values.size());
  EXPECT_EQ(GoogleString("value"), *(values[0]));
  EXPECT_EQ("content", contents);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));

  simple_stats_.Clear();
  scoped_ptr<Callback> callback(NewCallback());
  // Now advance time 301 seconds and the we should no longer
  // be able to fetch this resource out of the cache.
  mock_timer_.AdvanceMs(301 * 1000);
  found = FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                           callback.get());
  ASSERT_EQ(kNotFoundResult, found);
  ASSERT_FALSE(meta_data_out.headers_complete());
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheBackendHits));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheBackendMisses));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheExpirations));

  // However, the fallback value should be filled in.
  HTTPValue* fallback_value = callback->fallback_http_value();
  meta_data_out.Clear();
  contents = StringPiece();
  EXPECT_FALSE(fallback_value->Empty());
  ASSERT_TRUE(fallback_value->ExtractHeaders(&meta_data_out,
                                             &message_handler_));
  ASSERT_TRUE(meta_data_out.headers_complete());
  ASSERT_TRUE(fallback_value->ExtractContents(&contents));
  ASSERT_STREQ("value", meta_data_out.Lookup1("name"));
  EXPECT_EQ("content", contents);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheFallbacks));

  // Try again but with the cache invalidated.
  simple_stats_.Clear();
  scoped_ptr<Callback> callback2(NewCallback());
  callback2->cache_valid_ = false;
  found = FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                           callback2.get());
  ASSERT_EQ(kNotFoundResult, found);
  ASSERT_FALSE(meta_data_out.headers_complete());
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheBackendHits));
  // The fallback is empty since the entry has been invalidated.
  fallback_value = callback2->fallback_http_value();
  ASSERT_TRUE(fallback_value->Empty());
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));
}

TEST_F(HTTPCacheTest, PutGetCompressed) {
  // Check to see that when compression is on, data put into the cache is
  // properly compressed, and can be retrieved if the callback accepts gzipped
  // data.
  http_cache_->SetCompressionLevel(9); // Set cache to max compression.
  simple_stats_.Clear();
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  meta_data_in.Replace(HttpAttributes::kContentType,
                       kContentTypeCss.mime_type());
  meta_data_in.ComputeCaching();
  Put(kUrl, kFragment, &meta_data_in, "content");
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  HTTPValue value;
  HTTPCache::FindResult found = FindAcceptGzip(
      kUrl, kFragment, &value, &meta_data_out);
  ASSERT_EQ(kFoundResult, found);
  ASSERT_TRUE(meta_data_out.headers_complete());
  StringPiece contents;
  ASSERT_TRUE(value.ExtractContents(&contents));
  ConstStringStarVector values;
  ASSERT_TRUE(meta_data_out.Lookup("name", &values));
  ASSERT_EQ(static_cast<size_t>(1), values.size());
  EXPECT_EQ(GoogleString("value"), *(values[0]));
  EXPECT_NE("content", contents);
  GoogleString inflated;
  StringWriter inflate_writer(&inflated);
  GzipInflater::Inflate(contents, GzipInflater::kGzip, &inflate_writer);
  EXPECT_STREQ("content", inflated);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));
  http_cache_->SetCompressionLevel(0); // Return cache to uncompressed.
}

TEST_F(HTTPCacheTest, StaticInflatingFetch) {
  // Check to see that when compression is on, data put into the cache is
  // properly compressed, and can be retrieved if the callback accepts gzipped
  // data.
  http_cache_->SetCompressionLevel(9); // Set cache to max compression.
  simple_stats_.Clear();
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  meta_data_in.Replace(HttpAttributes::kContentType,
                       kContentTypeCss.mime_type());
  meta_data_in.ComputeCaching();
  Put(kUrl, kFragment, &meta_data_in, "content");
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  HTTPValue value;
  HTTPCache::FindResult found = FindAcceptGzip(
      kUrl, kFragment, &value, &meta_data_out);
  ASSERT_EQ(kFoundResult, found);
  ASSERT_TRUE(meta_data_out.headers_complete());
  StringPiece contents;
  ASSERT_TRUE(value.ExtractContents(&contents));
  ConstStringStarVector values;
  ASSERT_TRUE(meta_data_out.Lookup("name", &values));
  ASSERT_EQ(static_cast<size_t>(1), values.size());
  EXPECT_EQ(GoogleString("value"), *(values[0]));
  EXPECT_NE("content", contents);
  ResponseHeaders response_headers;
  EXPECT_TRUE(value.ExtractHeaders(&response_headers, NULL));
  // Check that the InflatingFetch gzip methods work properly when extracting
  // data.
  HTTPValue ungzipped;
  ASSERT_TRUE(InflatingFetch::UnGzipValueIfCompressed(
      value, &response_headers, &ungzipped, &message_handler_));
  ASSERT_TRUE(ungzipped.ExtractContents(&contents));
  ASSERT_TRUE(meta_data_out.Lookup("name", &values));
  ASSERT_EQ(static_cast<size_t>(1), values.size());
  EXPECT_EQ(GoogleString("value"), *(values[0]));
  EXPECT_STREQ("content", contents);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));
  http_cache_->SetCompressionLevel(0); // Return cache to uncompressed.
}

TEST_F(HTTPCacheTest, PutGetCompressedJpg) {
  // Check to see that when compression is on, data put into the cache is
  // properly compressed, and can be retrieved if the callback accepts gzipped
  // data. Jpeg data should not be compressed, as gzip doesn't handle non-text
  // data well.
  http_cache_->SetCompressionLevel(9); // Set cache to max compression.
  simple_stats_.Clear();
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  meta_data_in.Replace(HttpAttributes::kContentType,
                       kContentTypeJpeg.mime_type());
  meta_data_in.ComputeCaching();
  Put(kUrl, kFragment, &meta_data_in, "content");
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  HTTPValue value;
  HTTPCache::FindResult found = FindAcceptGzip(
      kUrl, kFragment, &value, &meta_data_out);
  ASSERT_EQ(kFoundResult, found);
  ASSERT_TRUE(meta_data_out.headers_complete());
  StringPiece contents;
  ASSERT_TRUE(value.ExtractContents(&contents));
  ConstStringStarVector values;
  ASSERT_TRUE(meta_data_out.Lookup("name", &values));
  ASSERT_EQ(static_cast<size_t>(1), values.size());
  EXPECT_EQ(GoogleString("value"), *(values[0]));
  EXPECT_EQ("content", contents);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));
  http_cache_->SetCompressionLevel(0); // Return cache to uncompressed.
}

TEST_F(HTTPCacheTest, PutGetForInvalidUrl) {
  simple_stats_.Clear();
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  meta_data_in.Replace(HttpAttributes::kContentType,
                       kContentTypeHtml.mime_type());
  meta_data_in.ComputeCaching();
  // The response for the invalid url does not get cached.
  Put("blah", kFragment, &meta_data_in, "content");
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  HTTPValue value;
  HTTPCache::FindResult found = Find(
      "blah", kFragment, &value, &meta_data_out);
  ASSERT_EQ(kNotFoundResult, found);
}

TEST_F(HTTPCacheTest, PutGetForHttps) {
  simple_stats_.Clear();
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  meta_data_in.Replace(HttpAttributes::kContentType,
                       kContentTypeHtml.mime_type());
  meta_data_in.ComputeCaching();
  // Disable caching of html on https.
  http_cache_->set_disable_html_caching_on_https(true);
  // The html response does not get cached.
  Put(kHttpsUrl, kFragment, &meta_data_in, "content");
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  HTTPValue value;
  HTTPCache::FindResult found = Find(
      kHttpsUrl, kFragment, &value, &meta_data_out);
  ASSERT_EQ(kNotFoundResult, found);

  // However a css file is cached.
  meta_data_in.Replace(HttpAttributes::kContentType,
                       kContentTypeCss.mime_type());
  meta_data_in.ComputeCaching();
  Put(kHttpsUrl, kFragment, &meta_data_in, "content");
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  found = Find(kHttpsUrl, kFragment, &value, &meta_data_out);
  ASSERT_EQ(kFoundResult, found);
  ASSERT_TRUE(meta_data_out.headers_complete());
  StringPiece contents;
  ASSERT_TRUE(value.ExtractContents(&contents));
  ConstStringStarVector values;
  ASSERT_TRUE(meta_data_out.Lookup("name", &values));
  ASSERT_EQ(static_cast<size_t>(1), values.size());
  EXPECT_EQ(GoogleString("value"), *(values[0]));
  EXPECT_EQ("content", contents);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));
}

TEST_F(HTTPCacheTest, EtagsAddedIfAbsent) {
  simple_stats_.Clear();
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl, kFragment, &meta_data_in, "content");
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));

  HTTPValue value;
  HTTPCache::FindResult found = Find(
      kUrl, kFragment, &value, &meta_data_out);
  ASSERT_EQ(kFoundResult, found);
  ASSERT_TRUE(meta_data_out.headers_complete());

  StringPiece contents;
  ASSERT_TRUE(value.ExtractContents(&contents));
  ConstStringStarVector values;
  ASSERT_TRUE(meta_data_out.Lookup("name", &values));
  ASSERT_EQ(static_cast<size_t>(1), values.size());
  EXPECT_EQ(GoogleString("value"), *(values[0]));
  EXPECT_STREQ(HTTPCache::FormatEtag("0"),
               meta_data_out.Lookup1(HttpAttributes::kEtag));
  EXPECT_EQ("content", contents);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));  // The "query" counts as a hit.
}

TEST_F(HTTPCacheTest, EtagsNotAddedIfPresent) {
  simple_stats_.Clear();
  ResponseHeaders meta_data_in, meta_data_out;
  meta_data_in.Add(HttpAttributes::kEtag, "Etag!");
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl, kFragment, &meta_data_in, "content");
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));

  HTTPValue value;
  HTTPCache::FindResult found = Find(
      kUrl, kFragment, &value, &meta_data_out);
  ASSERT_EQ(kFoundResult, found);
  ASSERT_TRUE(meta_data_out.headers_complete());

  StringPiece contents;
  ASSERT_TRUE(value.ExtractContents(&contents));
  ConstStringStarVector values;
  ASSERT_TRUE(meta_data_out.Lookup("name", &values));
  ASSERT_EQ(static_cast<size_t>(1), values.size());
  EXPECT_EQ(GoogleString("value"), *(values[0]));
  EXPECT_STREQ("Etag!", meta_data_out.Lookup1(HttpAttributes::kEtag));
  EXPECT_EQ("content", contents);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));  // The "query" counts as a hit.
}

TEST_F(HTTPCacheTest, CookiesNotCached) {
  simple_stats_.Clear();
  ResponseHeaders meta_data_in, meta_data_out;
  meta_data_in.Add(HttpAttributes::kSetCookie, "cookies!");
  meta_data_in.Add(HttpAttributes::kSetCookie2, "more cookies!");
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl, kFragment, &meta_data_in, "content");
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  HTTPValue value;
  HTTPCache::FindResult found = Find(
      kUrl, kFragment, &value, &meta_data_out);
  ASSERT_EQ(kFoundResult, found);
  ASSERT_TRUE(meta_data_out.headers_complete());
  StringPiece contents;
  ASSERT_TRUE(value.ExtractContents(&contents));
  ConstStringStarVector values;
  ASSERT_TRUE(meta_data_out.Lookup("name", &values));
  ASSERT_EQ(static_cast<size_t>(1), values.size());
  EXPECT_EQ(GoogleString("value"), *(values[0]));
  EXPECT_FALSE(meta_data_out.Lookup(HttpAttributes::kSetCookie, &values));
  EXPECT_FALSE(meta_data_out.Lookup(HttpAttributes::kSetCookie2, &values));
  EXPECT_EQ("content", contents);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));  // The "query" counts as a hit.
}

// Verifies that the cache will 'remember' that a fetch failed according to
// the configured policy.
TEST_F(HTTPCacheTest, RememberFetchFailed) {
  ResponseHeaders meta_data_out;
  http_cache_->RememberFailure(kUrl, kFragment, kFetchStatusOtherError,
                               &message_handler_);
  HTTPValue value;
  EXPECT_EQ(HTTPCache::FindResult(HTTPCache::kRecentFailure,
                                  kFetchStatusOtherError),
            Find(kUrl, kFragment, &value, &meta_data_out));

  // Now advance time 301 seconds; the cache should allow us to try fetching
  // again.
  mock_timer_.AdvanceMs(301 * 1000);
  EXPECT_EQ(kNotFoundResult,
            Find(kUrl, kFragment, &value, &meta_data_out));

  http_cache_->set_failure_caching_ttl_sec(kFetchStatusOtherError, 600);
  http_cache_->RememberFailure(kUrl, kFragment, kFetchStatusOtherError,
                               &message_handler_);

  // Now advance time 301 seconds; the cache should remember that the fetch
  // failed previously.
  mock_timer_.AdvanceMs(301 * 1000);
  EXPECT_EQ(HTTPCache::FindResult(HTTPCache::kRecentFailure,
                                  kFetchStatusOtherError),
            Find(kUrl, kFragment, &value, &meta_data_out));
}

// Verifies that the cache will 'remember' 'non-cacheable' according to the
// appropriate policy.
TEST_F(HTTPCacheTest, RememberNotCacheable200) {
  ResponseHeaders meta_data_out;
  http_cache_->RememberFailure(kUrl, kFragment, kFetchStatusUncacheable200,
                               &message_handler_);
  HTTPValue value;
  EXPECT_EQ(HTTPCache::FindResult(HTTPCache::kRecentFailure,
                                  kFetchStatusUncacheable200),
            Find(kUrl, kFragment, &value, &meta_data_out));

  // Now advance time 301 seconds; the cache should allow us to try fetching
  // again.
  mock_timer_.AdvanceMs(301 * 1000);
  EXPECT_EQ(kNotFoundResult,
            Find(kUrl, kFragment, &value, &meta_data_out));

  http_cache_->set_failure_caching_ttl_sec(kFetchStatusUncacheable200, 600);
  http_cache_->RememberFailure(kUrl, kFragment, kFetchStatusUncacheable200,
                               &message_handler_);

  // Now advance time 301 seconds; the cache should remember that the fetch
  // failed previously.
  mock_timer_.AdvanceMs(301 * 1000);
  EXPECT_EQ(HTTPCache::FindResult(HTTPCache::kRecentFailure,
                                  kFetchStatusUncacheable200),
            Find(kUrl, kFragment, &value, &meta_data_out));
}

// Make sure we don't remember 'non-cacheable' once we've put it into
// non-recording of failures mode (but do before that), and that we
// remember successful results even when in SetIgnoreFailurePuts() mode.
TEST_F(HTTPCacheTest, IgnoreFailurePuts) {
  http_cache_->RememberFailure(kUrl, kFragment,
                               kFetchStatusUncacheableError, &message_handler_);
  http_cache_->SetIgnoreFailurePuts();
  http_cache_->RememberFailure(kUrl2, kFragment,
                               kFetchStatusUncacheableError, &message_handler_);

  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl3, kFragment, &meta_data_in, "content");

  HTTPValue value_out;
  EXPECT_EQ(
      HTTPCache::FindResult(HTTPCache::kRecentFailure,
                            kFetchStatusUncacheableError),
      Find(kUrl, kFragment, &value_out, &meta_data_out));
  EXPECT_EQ(
      kNotFoundResult,
      Find(kUrl2, kFragment, &value_out, &meta_data_out));
  EXPECT_EQ(
      kFoundResult,
      Find(kUrl3, kFragment, &value_out, &meta_data_out));
}

TEST_F(HTTPCacheTest, Uncacheable) {
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, NULL);
  Put(kUrl, kFragment, &meta_data_in, "content");
  HTTPValue value;
  HTTPCache::FindResult found = Find(
      kUrl, kFragment, &value, &meta_data_out);
  ASSERT_EQ(kNotFoundResult, found);
  ASSERT_FALSE(meta_data_out.headers_complete());
}

TEST_F(HTTPCacheTest, UncacheablePrivate) {
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "private, max-age=300");
  Put(kUrl, kFragment, &meta_data_in, "content");
  HTTPValue value;
  HTTPCache::FindResult found = Find(
      kUrl, kFragment, &value, &meta_data_out);
  ASSERT_EQ(kNotFoundResult, found);
  ASSERT_FALSE(meta_data_out.headers_complete());
}

// Unit testing cache invalidation.
TEST_F(HTTPCacheTest, CacheInvalidation) {
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl, kFragment, &meta_data_in, "content");
  HTTPValue value;
  // Check with cache valid.
  EXPECT_EQ(
      kFoundResult,
      Find(kUrl, kFragment, &value, &meta_data_out, true));
  // Check with cache invalidated.
  EXPECT_EQ(
      kNotFoundResult,
      Find(kUrl, kFragment, &value, &meta_data_out, false));
}

TEST_F(HTTPCacheTest, IsFresh) {
  const char kDataIn[] = "content";
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl, kFragment, &meta_data_in, kDataIn);
  HTTPValue value;
  scoped_ptr<Callback> callback(NewCallback());
  callback->fresh_ = true;
  // Check with IsFresh set to true.
  EXPECT_EQ(kFoundResult,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             callback.get()));
  StringPiece contents;
  EXPECT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ(kDataIn, contents);
  EXPECT_TRUE(callback->fallback_http_value()->Empty());
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));

  callback.reset(NewCallback());
  value.Clear();
  callback->fresh_ = false;
  // Check with IsFresh set to false.
  EXPECT_EQ(kNotFoundResult,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             callback.get()));
  EXPECT_TRUE(value.Empty());
  EXPECT_TRUE(callback->fallback_http_value()->ExtractContents(&contents));
  EXPECT_STREQ(kDataIn, contents);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheFallbacks));
}

TEST_F(HTTPCacheTest, OverrideCacheTtlMs) {
  simple_stats_.Clear();

  // First test overriding works for a publicly cacheable response if the
  // override TTL is larger than the original one.
  const char kDataIn[] = "content";
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl, kFragment, &meta_data_in, kDataIn);
  HTTPValue value;
  scoped_ptr<Callback> callback(NewCallback());
  callback->override_cache_ttl_ms_ = 400 * 1000;
  EXPECT_EQ(kFoundResult,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             callback.get()));
  StringPiece contents;
  EXPECT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ(kDataIn, contents);
  EXPECT_TRUE(callback->fallback_http_value()->Empty());
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_STREQ("max-age=400",
               meta_data_out.Lookup1(HttpAttributes::kCacheControl));

  // Now, test that overriding has no effect if the override TTL is less than
  // the original one.
  simple_stats_.Clear();
  callback.reset(NewCallback());
  value.Clear();
  callback->override_cache_ttl_ms_ = 200 * 1000;
  EXPECT_EQ(kFoundResult,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             callback.get()));
  EXPECT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ(kDataIn, contents);
  EXPECT_TRUE(callback->fallback_http_value()->Empty());
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_STREQ("max-age=300",
               meta_data_out.Lookup1(HttpAttributes::kCacheControl));

  // Now, test that overriding works for Cache-Control: private responses.
  simple_stats_.Clear();
  callback.reset(NewCallback());
  value.Clear();
  meta_data_in.Clear();
  InitHeaders(&meta_data_in, "private");
  Put(kUrl, kFragment, &meta_data_in, kDataIn);
  callback->override_cache_ttl_ms_ = 400 * 1000;
  EXPECT_EQ(kFoundResult,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             callback.get()));
  EXPECT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ(kDataIn, contents);
  EXPECT_TRUE(callback->fallback_http_value()->Empty());
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_STREQ("max-age=400",
               meta_data_out.Lookup1(HttpAttributes::kCacheControl));

  // Now advance the time by 310 seconds and set override cache TTL to 300
  // seconds. The lookup fails.
  simple_stats_.Clear();
  mock_timer_.AdvanceMs(310 * Timer::kSecondMs);
  callback.reset(NewCallback());
  value.Clear();
  meta_data_in.Clear();
  callback->override_cache_ttl_ms_ = 300 * 1000;
  EXPECT_EQ(kNotFoundResult,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             callback.get()));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheFallbacks));

  // Set the override cache TTL to 400 seconds. The lookup succeeds and the
  // Cache-Control header is updated.
  simple_stats_.Clear();
  callback.reset(NewCallback());
  value.Clear();
  meta_data_in.Clear();
  callback->override_cache_ttl_ms_ = 400 * 1000;
  EXPECT_EQ(kFoundResult,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             callback.get()));
  EXPECT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ(kDataIn, contents);
  EXPECT_TRUE(callback->fallback_http_value()->Empty());
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_STREQ("max-age=400",
               meta_data_out.Lookup1(HttpAttributes::kCacheControl));
}

TEST_F(HTTPCacheTest, OverrideCacheTtlMsForOriginallyNotCacheable200) {
  ResponseHeaders meta_data_out;
  http_cache_->RememberFailure(kUrl, kFragment, kFetchStatusUncacheable200,
                               &message_handler_);
  HTTPValue value;
  scoped_ptr<Callback> callback(NewCallback());
  EXPECT_EQ(HTTPCache::FindResult(HTTPCache::kRecentFailure,
                                  kFetchStatusUncacheable200),
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             callback.get()));

  // Now change the value of override_cache_ttl_ms_. The lookup returns
  // kNotFound now.
  callback.reset(NewCallback());
  value.Clear();
  callback->override_cache_ttl_ms_ = 200 * 1000;
  EXPECT_EQ(kNotFoundResult,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             callback.get()));
}

TEST_F(HTTPCacheTest, OverrideCacheTtlMsForOriginallyNotCacheableNon200) {
  ResponseHeaders meta_data_out;
  http_cache_->RememberFailure(kUrl, kFragment, kFetchStatusUncacheableError,
                               &message_handler_);
  HTTPValue value;
  scoped_ptr<Callback> callback(NewCallback());
  EXPECT_EQ(HTTPCache::FindResult(HTTPCache::kRecentFailure,
                                  kFetchStatusUncacheableError),
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             callback.get()));

  // Now change the value of override_cache_ttl_ms_. The lookup returns
  // kNotFound now.
  callback.reset(NewCallback());
  value.Clear();
  callback->override_cache_ttl_ms_ = 200 * 1000;
  EXPECT_EQ(HTTPCache::FindResult(HTTPCache::kRecentFailure,
                                  kFetchStatusUncacheableError),
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             callback.get()));
}

TEST_F(HTTPCacheTest, OverrideCacheTtlMsForOriginallyFetchFailed) {
  ResponseHeaders meta_data_out;
  http_cache_->RememberFailure(kUrl, kFragment, kFetchStatusOtherError,
                               &message_handler_);
  HTTPValue value;
  scoped_ptr<Callback> callback(NewCallback());
  EXPECT_EQ(HTTPCache::FindResult(HTTPCache::kRecentFailure,
                                  kFetchStatusOtherError),
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             callback.get()));

  // Now change the value of override_cache_ttl_ms_. The lookup continues to
  // return recent failure with kFetchStatusOtherError.
  callback.reset(NewCallback());
  value.Clear();
  callback->override_cache_ttl_ms_ = 200 * 1000;
  EXPECT_EQ(HTTPCache::FindResult(HTTPCache::kRecentFailure,
                                  kFetchStatusOtherError),
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             callback.get()));
}

TEST_F(HTTPCacheTest, FragmentsIndependent) {
  HTTPValue value;
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl, kFragment, &meta_data_in, "content");
  ASSERT_EQ(kFoundResult,
            Find(kUrl, kFragment, &value, &meta_data_out));
  ASSERT_EQ(kNotFoundResult,
            Find(kUrl, kFragment2, &value, &meta_data_out));
  Put(kUrl, kFragment2, &meta_data_in, "content");
  ASSERT_EQ(kFoundResult,
            Find(kUrl, kFragment2, &value, &meta_data_out));
}

TEST_F(HTTPCacheTest, UpdateVersion) {
  HTTPValue value;
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  StringPiece contents;

  // Equivalent to pre-versioned caching.
  http_cache_->set_version_prefix("");
  Put(kUrl, "", &meta_data_in, "v1: No fragment");
  Put(kUrl, kFragment, &meta_data_in, "v1: Fragment");

  EXPECT_EQ(kFoundResult,
            Find(kUrl, "", &value, &meta_data_out));
  ASSERT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ("v1: No fragment", contents);
  EXPECT_EQ(kFoundResult,
            Find(kUrl, kFragment, &value, &meta_data_out));
  ASSERT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ("v1: Fragment", contents);

  // Setting version invalidates old data.
  http_cache_->SetVersion(2);
  EXPECT_EQ(kNotFoundResult,
            Find(kUrl, "", &value, &meta_data_out));
  EXPECT_EQ(kNotFoundResult,
            Find(kUrl, kFragment, &value, &meta_data_out));

  Put(kUrl, "", &meta_data_in, "v2: No fragment");
  Put(kUrl, kFragment, &meta_data_in, "v2: Fragment");

  EXPECT_EQ(kFoundResult,
            Find(kUrl, "", &value, &meta_data_out));
  ASSERT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ("v2: No fragment", contents);
  EXPECT_EQ(kFoundResult,
            Find(kUrl, kFragment, &value, &meta_data_out));
  ASSERT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ("v2: Fragment", contents);

  // Updating version invalidates old data.
  http_cache_->SetVersion(3);
  EXPECT_EQ(kNotFoundResult,
            Find(kUrl, "", &value, &meta_data_out));
  EXPECT_EQ(kNotFoundResult,
            Find(kUrl, kFragment, &value, &meta_data_out));
}

TEST_F(HTTPCacheTest, NoMutateOnPut) {
  ResponseHeaders response_headers;
  PopulateGzippedEntry("max-age=300", &response_headers);

  // The value that we have in the cache is compressed, but that should
  // not cause a mutation in the response headers passed into Put.
  EXPECT_FALSE(response_headers.HasValue(
      HttpAttributes::kContentEncoding, "gzip"));
}

TEST_F(HTTPCacheTest, DecompressFallbackValue) {
  ResponseHeaders response_headers;
  PopulateGzippedEntry("max-age=300", &response_headers);

  mock_timer_.AdvanceMs(310 * Timer::kSecondMs);  // Makes entry stale.
  response_headers.Clear();

  // If we don't have gzip in the request, it should not be in the fallback
  // value.
  scoped_ptr<Callback> callback(NewCallback());
  HTTPValue value;
  EXPECT_EQ(kNotFoundResult, FindWithCallback(  // Not found because it's stale.
      kUrl, kFragment, &value, &response_headers,
      callback.get()));
  EXPECT_TRUE(callback->fallback_http_value()->ExtractHeaders(
      &response_headers, &message_handler_));
  EXPECT_FALSE(response_headers.HasValue(
      HttpAttributes::kContentEncoding, "gzip"));
}

TEST_F(HTTPCacheTest, LeaveFallbackCompressed) {
  ResponseHeaders response_headers;
  PopulateGzippedEntry("max-age=300", &response_headers);

  mock_timer_.AdvanceMs(310 * Timer::kSecondMs);  // Makes entry stale.
  response_headers.Clear();

  // When we enable gzip in the request, though, we will get it because the
  // value stored in the cache is gzipped.
  RequestContextPtr request_context(RequestContext::NewTestRequestContext(
      thread_system_.get()));
  request_context->SetAcceptsGzip(true);
  Callback callback(request_context);
  HTTPValue value;
  EXPECT_EQ(kNotFoundResult, FindWithCallback(
      kUrl, kFragment, &value, &response_headers,
      &callback));
  EXPECT_TRUE(callback.fallback_http_value()->ExtractHeaders(
      &response_headers, &message_handler_));
  EXPECT_TRUE(response_headers.HasValue(
      HttpAttributes::kContentEncoding, "gzip"));
}

TEST_F(HTTPCacheTest, PutAlreadyCompressedWithCompressionOff) {
  size_t cache_size = CacheSizeAfterPut(0, true);
  // The physical cache entry includes the key, fragment, and value, which
  // we know exactly.  It also includes the serialized headers, and we
  // don't know the exact size of that, as HTTPValue does not expose it.
  // But what we can say safely is that even though we passed a compressed
  // value to HTTPCache, by uncompressing it and adding the header size,
  // it will be larger than kPayloadSizeWithoutHeaders.
  EXPECT_LT(kPayloadSizeWithoutHeaders, cache_size);
}

TEST_F(HTTPCacheTest, PutAlreadyCompressedWithCompressionOn) {
  size_t cache_size = CacheSizeAfterPut(9, true);
  // Here we are storing compressed values into the cache, and kCssText is
  // highly compressible.  So the stored size will be less than the
  // kPayloadSizeWithoutHeaders, despite the fact that we are also storing
  // the headers.
  EXPECT_GT(kPayloadSizeWithoutHeaders, cache_size);
}

TEST_F(HTTPCacheTest, PutUncompressedWithCompressionOff) {
  size_t cache_size = CacheSizeAfterPut(0, false);
  // The physical cache entry includes the key, fragment, and value, which
  // we know exactly.  It also includes the serialized headers, and we
  // don't know the exact size of that, as HTTPValue does not expose it.
  // But what we can say safely is that even though we passed a compressed
  // value to HTTPCache, by uncompressing it and adding the header size,
  // it will be larger than kPayloadSizeWithoutHeaders.
  EXPECT_LT(kPayloadSizeWithoutHeaders, cache_size);
}

TEST_F(HTTPCacheTest, PutUncompressedWithCompressionOn) {
  size_t cache_size = CacheSizeAfterPut(9, false);
  // Here we are storing compressed values into the cache, and kCssText is
  // highly compressible.  So the stored size will be less than the
  // kPayloadSizeWithoutHeaders, despite the fact that we are also storing
  // the headers.
  EXPECT_GT(kPayloadSizeWithoutHeaders, cache_size);
}

class HTTPCacheWriteThroughTest : public HTTPCacheTest {
 protected:
  // Unlike HTTPCacheTest::Callback this can produce different validity for
  // L1/L2 to help testing.
  class FakeHttpCacheCallback : public HTTPCache::Callback {
   public:
    explicit FakeHttpCacheCallback(ThreadSystem* thread_system)
        : HTTPCache::Callback(
              RequestContext::NewTestRequestContext(thread_system)),
          called_(false),
          first_call_cache_valid_(true),
          first_cache_valid_(true),
          second_cache_valid_(true),
          first_call_cache_fresh_(true),
          first_cache_fresh_(true),
          second_cache_fresh_(true) {}

    virtual void Done(HTTPCache::FindResult result) {
      called_ = true;
      result_ = result;
    }
    virtual bool IsCacheValid(const GoogleString& key,
                              const ResponseHeaders& headers) {
      bool result = first_call_cache_valid_ ?
          first_cache_valid_ : second_cache_valid_;
      first_call_cache_valid_ = false;
      return result;
    }

    virtual bool IsFresh(const ResponseHeaders& headers) {
      bool result = first_call_cache_fresh_ ?
          first_cache_fresh_ : second_cache_fresh_;
      first_call_cache_fresh_ = false;
      return result;
    }

    virtual ResponseHeaders::VaryOption RespectVaryOnResources() const {
      return ResponseHeaders::kRespectVaryOnResources;
    }

    bool called_;
    HTTPCache::FindResult result_;
    bool first_call_cache_valid_;
    bool first_cache_valid_;
    bool second_cache_valid_;
    bool first_call_cache_fresh_;
    bool first_cache_fresh_;
    bool second_cache_fresh_;
  };

  HTTPCacheWriteThroughTest()
      : cache1_(kMaxSize), cache2_(kMaxSize),
        write_through_cache_(&cache1_, &cache2_),
        key_("http://www.test.com/1"),
        key2_("http://www.test.com/2"),
        fragment_("www.test.com"),
        content_("content"),
        cache1_ms_(-1),
        cache2_ms_(-1) {
    http_cache_.reset(new HTTPCache(&write_through_cache_, &mock_timer_,
                                    &mock_hasher_, &simple_stats_));
    http_cache_->set_cache_levels(2);
  }

  void CheckCachedValueValid() {
    HTTPValue value;
    ResponseHeaders headers;
    HTTPCache::FindResult found = Find(
        key_, fragment_, &value, &headers);
    EXPECT_EQ(kFoundResult, found);
    EXPECT_TRUE(headers.headers_complete());
    StringPiece contents;
    EXPECT_TRUE(value.ExtractContents(&contents));
    EXPECT_EQ(content_, contents);
    EXPECT_STREQ("value", headers.Lookup1("name"));
  }

  void CheckCachedValueExpired() {
    HTTPValue value;
    ResponseHeaders headers;
    HTTPCache::FindResult found = Find(key_, fragment_, &value, &headers);
    EXPECT_EQ(kNotFoundResult, found);
    EXPECT_FALSE(headers.headers_complete());
  }

  void ClearStats() {
    cache1_.ClearStats();
    cache2_.ClearStats();
    simple_stats_.Clear();
  }

  HTTPCache::FindResult Find(const GoogleString& key,
                             const GoogleString& fragment,
                             HTTPValue* value,
                             ResponseHeaders* headers) {
    FakeHttpCacheCallback callback(thread_system_.get());
    http_cache_->Find(key, fragment, &message_handler_, &callback);
    EXPECT_TRUE(callback.called_);
    if (callback.result_.status == HTTPCache::kFound) {
      value->Link(callback.http_value());
    }
    headers->CopyFrom(*callback.response_headers());
    callback.request_context()->timing_info().GetHTTPCacheLatencyMs(
        &cache1_ms_);
    callback.request_context()->timing_info().GetL2HTTPCacheLatencyMs(
        &cache2_ms_);

    return callback.result_;
  }

  LRUCache cache1_;
  LRUCache cache2_;
  WriteThroughCache write_through_cache_;

  const GoogleString key_;
  const GoogleString key2_;
  const GoogleString fragment_;
  const GoogleString content_;
  int64 cache1_ms_;
  int64 cache2_ms_;
};

// Simple flow of putting in an item, getting it.
TEST_F(HTTPCacheWriteThroughTest, PutGet) {
  ClearStats();
  ResponseHeaders headers_in;
  InitHeaders(&headers_in, "max-age=300");
  Put(key_, fragment_, &headers_in, content_);
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_EQ(0, cache1_.num_hits());
  EXPECT_EQ(0, cache1_.num_misses());
  EXPECT_EQ(1, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(0, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(1, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());

  CheckCachedValueValid();
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_EQ(1, cache1_.num_hits());
  EXPECT_EQ(0, cache1_.num_misses());
  EXPECT_EQ(1, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(0, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(1, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());
  EXPECT_EQ(0, cache1_ms_);
  EXPECT_EQ(-1, cache2_ms_);

  // Remove the entry from cache1. We find it in cache2. The value is also now
  // inserted into cache1.
  cache1_.Clear();
  CheckCachedValueValid();
  EXPECT_EQ(2, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_EQ(1, cache1_.num_hits());
  EXPECT_EQ(1, cache1_.num_misses());
  EXPECT_EQ(2, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(1, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(1, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());
  EXPECT_EQ(0, cache1_ms_);
  EXPECT_EQ(0, cache2_ms_);

  // Now advance time 301 seconds and the we should no longer
  // be able to fetch this resource out of the cache. Note that we check both
  // the local and remote cache in this case.
  mock_timer_.AdvanceMs(301 * 1000);
  CheckCachedValueExpired();
  EXPECT_EQ(2, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_EQ(2, cache1_.num_hits());
  EXPECT_EQ(1, cache1_.num_misses());
  EXPECT_EQ(2, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(2, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(1, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());
  EXPECT_EQ(0, cache1_ms_);
  EXPECT_EQ(0, cache2_ms_);

  ClearStats();
  // Test that fallback_http_value() is set correctly.
  FakeHttpCacheCallback callback(thread_system_.get());
  http_cache_->Find(key_, fragment_, &message_handler_, &callback);
  EXPECT_EQ(kNotFoundResult, callback.result_);
  EXPECT_FALSE(callback.fallback_http_value()->Empty());
  EXPECT_TRUE(callback.http_value()->Empty());
  StringPiece content;
  EXPECT_TRUE(callback.fallback_http_value()->ExtractContents(&content));
  EXPECT_STREQ(content_, content);
  // We find a stale response in the L1 cache, clear it and use the stale
  // response in the L2 cache instead.
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(1, cache1_.num_hits());
  EXPECT_EQ(0, cache1_.num_misses());
  EXPECT_EQ(0, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(1, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(0, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());

  // Create a temporary HTTPCache with just cache1 and insert a stale response
  // into it. We use the fallback from cache2.
  HTTPCache temp_l1_cache(&cache1_, &mock_timer_, &mock_hasher_,
                          &simple_stats_);
  // Force caching so that the stale response is inserted.
  temp_l1_cache.set_force_caching(true);
  temp_l1_cache.Put(key_, fragment_, RequestHeaders::Properties(),
                    ResponseHeaders::kRespectVaryOnResources,
                    &headers_in, "new", &message_handler_);
  ClearStats();
  FakeHttpCacheCallback callback2(thread_system_.get());
  http_cache_->Find(key_, fragment_, &message_handler_, &callback2);
  EXPECT_EQ(kNotFoundResult, callback2.result_);
  EXPECT_FALSE(callback2.fallback_http_value()->Empty());
  EXPECT_TRUE(callback2.http_value()->Empty());
  StringPiece content2;
  EXPECT_TRUE(callback2.fallback_http_value()->ExtractContents(&content2));
  EXPECT_STREQ(content_, content2);
  // We find a stale response in the L1 cache, clear it and use the stale
  // response in the L2 cache instead.
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(1, cache1_.num_hits());
  EXPECT_EQ(0, cache1_.num_misses());
  EXPECT_EQ(0, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(1, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(0, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());

  ClearStats();
  // Clear cache2. We now use the fallback from cache1.
  cache2_.Clear();
  FakeHttpCacheCallback callback3(thread_system_.get());
  http_cache_->Find(key_, fragment_, &message_handler_, &callback3);
  EXPECT_EQ(kNotFoundResult, callback3.result_);
  EXPECT_FALSE(callback3.fallback_http_value()->Empty());
  EXPECT_TRUE(callback3.http_value()->Empty());
  StringPiece content3;
  EXPECT_TRUE(callback3.fallback_http_value()->ExtractContents(&content3));
  EXPECT_STREQ("new", content3);
  // We find a stale response in cache1. Since we don't find anything in cache2,
  // we use the stale response from cache1
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(1, cache1_.num_hits());
  EXPECT_EQ(0, cache1_.num_misses());
  EXPECT_EQ(0, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(0, cache2_.num_hits());
  EXPECT_EQ(1, cache2_.num_misses());
  EXPECT_EQ(0, cache2_.num_inserts());
}

// Check size-limits for the small cache
TEST_F(HTTPCacheWriteThroughTest, SizeLimit) {
  ClearStats();
  write_through_cache_.set_cache1_limit(183);  // See below.
  ResponseHeaders headers_in;
  InitHeaders(&headers_in, "max-age=300");

  // This one will fit. Size:
  // Key: v2/www.test.com/http://www.test.com/1 --- 37 bytes.
  // Value: 145 bytes
  // 145 + 37 = 182.
  Put(key_, fragment_, &headers_in, "Name");
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, cache1_.num_hits());
  EXPECT_EQ(0, cache1_.num_misses());
  EXPECT_EQ(1, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(0, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(1, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());

  // This one will not, as the value is a bit bigger and the key has same
  // length.
  Put(key2_, fragment_, &headers_in, "TooBigForCache1");
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(2, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, cache1_.num_hits());
  EXPECT_EQ(0, cache1_.num_misses());
  EXPECT_EQ(1, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(0, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(2, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());
}

TEST_F(HTTPCacheWriteThroughTest, PutGetForHttps) {
  ClearStats();
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  meta_data_in.Replace(HttpAttributes::kContentType,
                       kContentTypeHtml.mime_type());
  meta_data_in.ComputeCaching();
  // Disable caching of html on https.
  http_cache_->set_disable_html_caching_on_https(true);
  // The html response does not get cached.
  Put(kHttpsUrl, fragment_, &meta_data_in, "content");
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  HTTPValue value;
  HTTPCache::FindResult found = Find(
      kHttpsUrl, fragment_, &value, &meta_data_out);
  ASSERT_EQ(kNotFoundResult, found);

  // However a css file is cached.
  meta_data_in.Replace(HttpAttributes::kContentType,
                       kContentTypeCss.mime_type());
  meta_data_in.ComputeCaching();
  Put(kHttpsUrl, fragment_, &meta_data_in, "content");
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  found = Find(kHttpsUrl, fragment_, &value, &meta_data_out);
  ASSERT_EQ(kFoundResult, found);
  ASSERT_TRUE(meta_data_out.headers_complete());
  StringPiece contents;
  ASSERT_TRUE(value.ExtractContents(&contents));
  ConstStringStarVector values;
  ASSERT_TRUE(meta_data_out.Lookup("name", &values));
  ASSERT_EQ(static_cast<size_t>(1), values.size());
  EXPECT_EQ(GoogleString("value"), *(values[0]));
  EXPECT_EQ("content", contents);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));
}

// Verifies that the cache will 'remember' that a fetch should not be
// cached for 5 minutes.
TEST_F(HTTPCacheWriteThroughTest, RememberFetchFailedOrNotCacheable) {
  ClearStats();
  ResponseHeaders headers_out;
  http_cache_->RememberFailure(key_, fragment_, kFetchStatusOtherError,
                               &message_handler_);
  HTTPValue value;
  EXPECT_EQ(HTTPCache::FindResult(HTTPCache::kRecentFailure,
                                  kFetchStatusOtherError),
            Find(key_, fragment_, &value, &headers_out));

  // Now advance time 301 seconds; the cache should allow us to try fetching
  // again.
  mock_timer_.AdvanceMs(301 * 1000);
  EXPECT_EQ(kNotFoundResult,
            Find(key_, fragment_, &value, &headers_out));
}

TEST_F(HTTPCacheWriteThroughTest, RememberFetchDropped) {
  ClearStats();
  ResponseHeaders headers_out;
  http_cache_->RememberFailure(key_, fragment_, kFetchStatusDropped,
                               &message_handler_);
  HTTPValue value;
  EXPECT_EQ(HTTPCache::FindResult(HTTPCache::kRecentFailure,
                                  kFetchStatusDropped),
            Find(key_, fragment_, &value, &headers_out));

  // Now advance time 11 seconds; the cache should allow us to try fetching
  // again.
  mock_timer_.AdvanceMs(11 * Timer::kSecondMs);
  EXPECT_EQ(kNotFoundResult,
            Find(key_, fragment_, &value, &headers_out));
}

// Make sure we don't remember 'non-cacheable' once we've put it into
// SetIgnoreFailurePuts() mode (but do before)
TEST_F(HTTPCacheWriteThroughTest, SetIgnoreFailurePuts) {
  ClearStats();
  http_cache_->RememberFailure(key_, fragment_, kFetchStatusUncacheableError,
                               &message_handler_);
  http_cache_->SetIgnoreFailurePuts();
  http_cache_->RememberFailure(key2_, fragment_, kFetchStatusUncacheableError,
                               &message_handler_);
  ResponseHeaders headers_out;
  HTTPValue value_out;
  EXPECT_EQ(
      HTTPCache::FindResult(HTTPCache::kRecentFailure,
                            kFetchStatusUncacheableError),
      Find(key_, fragment_, &value_out, &headers_out));
  EXPECT_EQ(
      kNotFoundResult,
      Find(key2_, fragment_, &value_out, &headers_out));
}

TEST_F(HTTPCacheWriteThroughTest, Uncacheable) {
  ClearStats();
  ResponseHeaders headers_in, headers_out;
  InitHeaders(&headers_in, NULL);
  Put(key_, fragment_, &headers_in, content_);
  HTTPValue value;
  HTTPCache::FindResult found = Find(key_, fragment_, &value, &headers_out);
  ASSERT_EQ(kNotFoundResult, found);
  ASSERT_FALSE(headers_out.headers_complete());
}

TEST_F(HTTPCacheWriteThroughTest, UncacheablePrivate) {
  ClearStats();
  ResponseHeaders headers_in, headers_out;
  InitHeaders(&headers_in, "private, max-age=300");
  Put(key_, fragment_, &headers_in, content_);
  HTTPValue value;
  HTTPCache::FindResult found = Find(key_, fragment_, &value, &headers_out);
  ASSERT_EQ(kNotFoundResult, found);
  ASSERT_FALSE(headers_out.headers_complete());
}

// Unit testing cache invalidation.
TEST_F(HTTPCacheWriteThroughTest, CacheInvalidation) {
  ClearStats();
  ResponseHeaders meta_data_in;
  InitHeaders(&meta_data_in, "max-age=300");
  Put(key_, fragment_, &meta_data_in, content_);
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, cache1_.num_hits());
  EXPECT_EQ(0, cache1_.num_misses());
  EXPECT_EQ(1, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(0, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(1, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());

  // Check with both caches valid...
  ClearStats();
  FakeHttpCacheCallback callback1(thread_system_.get());
  http_cache_->Find(key_, fragment_, &message_handler_, &callback1);
  EXPECT_TRUE(callback1.called_);
  // ... only goes to cache1_ and hits.
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(1, cache1_.num_hits());
  EXPECT_EQ(0, cache1_.num_misses());
  EXPECT_EQ(0, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(0, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(0, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());
  EXPECT_EQ(kFoundResult, callback1.result_);

  // Check with local cache invalid and remote cache valid...
  ClearStats();
  FakeHttpCacheCallback callback2(thread_system_.get());
  callback2.first_cache_valid_ = false;
  http_cache_->Find(key_, fragment_, &message_handler_, &callback2);
  EXPECT_TRUE(callback2.called_);
  // ... hits both cache1_ (invalidated later by callback2) and cache_2.
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(1, cache1_.num_hits());
  EXPECT_EQ(0, cache1_.num_misses());
  EXPECT_EQ(0, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(1, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(0, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());
  // The insert in cache1_ is a reinsert.
  EXPECT_EQ(1, cache1_.num_identical_reinserts());
  EXPECT_EQ(kFoundResult, callback2.result_);

  // Check with both caches invalid...
  ClearStats();
  FakeHttpCacheCallback callback3(thread_system_.get());
  callback3.first_cache_valid_ = false;
  callback3.second_cache_valid_ = false;
  http_cache_->Find(key_, fragment_, &message_handler_, &callback3);
  EXPECT_TRUE(callback3.called_);
  // ... hits both cache1_ and cache_2. Both invalidated by callback3. So
  // http_cache_ misses.
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(1, cache1_.num_hits());
  EXPECT_EQ(0, cache1_.num_misses());
  EXPECT_EQ(0, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(1, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(0, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());
  EXPECT_EQ(kNotFoundResult, callback3.result_);

  // Check with local cache valid and remote cache invalid...
  ClearStats();
  FakeHttpCacheCallback callback4(thread_system_.get());
  callback4.second_cache_valid_ = false;
  http_cache_->Find(key_, fragment_, &message_handler_, &callback4);
  EXPECT_TRUE(callback4.called_);
  // ... only goes to cache1_ and hits.
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(1, cache1_.num_hits());
  EXPECT_EQ(0, cache1_.num_misses());
  EXPECT_EQ(0, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(0, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(0, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());
  EXPECT_EQ(kFoundResult, callback4.result_);
}

// Unit testing cache freshness.
TEST_F(HTTPCacheWriteThroughTest, CacheFreshness) {
  ClearStats();
  ResponseHeaders meta_data_in;
  InitHeaders(&meta_data_in, "max-age=300");
  Put(key_, fragment_, &meta_data_in, content_);
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, cache1_.num_hits());
  EXPECT_EQ(0, cache1_.num_misses());
  EXPECT_EQ(1, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(0, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(1, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());

  // Check with both caches freshe...
  ClearStats();
  FakeHttpCacheCallback callback1(thread_system_.get());
  http_cache_->Find(key_, fragment_, &message_handler_, &callback1);
  EXPECT_TRUE(callback1.called_);
  // ... only goes to cache1_ and hits.
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(1, cache1_.num_hits());
  EXPECT_EQ(0, cache1_.num_misses());
  EXPECT_EQ(0, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(0, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(0, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());
  EXPECT_EQ(kFoundResult, callback1.result_);

  // Check with local cache not fresh and remote cache fresh...
  ClearStats();
  FakeHttpCacheCallback callback2(thread_system_.get());
  callback2.first_cache_fresh_ = false;
  http_cache_->Find(key_, fragment_, &message_handler_, &callback2);
  EXPECT_TRUE(callback2.called_);
  // ... hits both cache1_ and cache_2.
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(1, cache1_.num_hits());
  EXPECT_EQ(0, cache1_.num_misses());
  EXPECT_EQ(0, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(1, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(0, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());
  // The insert in cache1_ is a reinsert.
  EXPECT_EQ(1, cache1_.num_identical_reinserts());
  EXPECT_EQ(kFoundResult, callback2.result_);

  // Check with both caches not fresh...
  ClearStats();
  FakeHttpCacheCallback callback3(thread_system_.get());
  callback3.first_cache_fresh_ = false;
  callback3.second_cache_fresh_ = false;
  http_cache_->Find(key_, fragment_, &message_handler_, &callback3);
  EXPECT_TRUE(callback3.called_);
  // ... hits both cache1_ and cache_2. Both aren't fresh. So http_cache_
  // misses.
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(1, cache1_.num_hits());
  EXPECT_EQ(0, cache1_.num_misses());
  EXPECT_EQ(0, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(1, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(0, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());
  EXPECT_EQ(kNotFoundResult, callback3.result_);
  EXPECT_FALSE(callback3.fallback_http_value()->Empty());

  // Check with local cache fresh and remote cache not fresh...
  ClearStats();
  FakeHttpCacheCallback callback4(thread_system_.get());
  callback4.second_cache_fresh_ = false;
  http_cache_->Find(key_, fragment_, &message_handler_, &callback4);
  EXPECT_TRUE(callback4.called_);
  // ... only goes to cache1_ and hits.
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheExpirations));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(1, cache1_.num_hits());
  EXPECT_EQ(0, cache1_.num_misses());
  EXPECT_EQ(0, cache1_.num_inserts());
  EXPECT_EQ(0, cache1_.num_deletes());
  EXPECT_EQ(0, cache2_.num_hits());
  EXPECT_EQ(0, cache2_.num_misses());
  EXPECT_EQ(0, cache2_.num_inserts());
  EXPECT_EQ(0, cache2_.num_deletes());
  EXPECT_EQ(kFoundResult, callback4.result_);
}

}  // namespace net_instaweb
