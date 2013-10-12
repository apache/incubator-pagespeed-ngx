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

#include "net/instaweb/http/public/http_cache.h"

#include <algorithm>

#include "base/logging.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

// Remember that a Fetch failed for 5 minutes by default.
//
// TODO(jmarantz): We could handle cc-private a little differently:
// in this case we could arguably remember it using the original cc-private ttl.
const int kRememberNotCacheableTtlSec = 300;
const int kRememberFetchFailedTtlSec = 300;

// We use an extremely low TTL for load-shed resources since we don't
// want this to get in the way of debugging, or letting a page with
// large numbers of refresh converge towards being fully optimized.
//
// Note if you bump this number too high, then
// RewriteContextTest.DropFetchesAndRecover cannot pass because we
// won't try fetches for dropped resources until after the rewrites
// for the successful fetches will expire.  In system terms, that means
// that you can never complete rewrites for a page with so many resources
// that the initial round of fetches gets some dropped.
const int kRememberFetchDroppedTtlSec = 10;

// Maximum size of response content in bytes. -1 indicates that there is no size
// limit.
const int64 kCacheSizeUnlimited = -1;

}  // namespace

const char HTTPCache::kCacheTimeUs[] = "cache_time_us";
const char HTTPCache::kCacheHits[] = "cache_hits";
const char HTTPCache::kCacheMisses[] = "cache_misses";
const char HTTPCache::kCacheBackendHits[] = "cache_backend_hits";
const char HTTPCache::kCacheBackendMisses[] = "cache_backend_misses";
const char HTTPCache::kCacheFallbacks[] = "cache_fallbacks";
const char HTTPCache::kCacheExpirations[] = "cache_expirations";
const char HTTPCache::kCacheInserts[] = "cache_inserts";
const char HTTPCache::kCacheDeletes[] = "cache_deletes";

// This used for doing prefix match for etag in fetcher code.
const char HTTPCache::kEtagPrefix[] = "W/\"PSA-";

HTTPCache::HTTPCache(CacheInterface* cache, Timer* timer, Hasher* hasher,
                     Statistics* stats)
    : cache_(cache),
      timer_(timer),
      hasher_(hasher),
      force_caching_(false),
      disable_html_caching_on_https_(false),
      cache_time_us_(stats->GetVariable(kCacheTimeUs)),
      cache_hits_(stats->GetVariable(kCacheHits)),
      cache_misses_(stats->GetVariable(kCacheMisses)),
      cache_backend_hits_(stats->GetVariable(kCacheBackendHits)),
      cache_backend_misses_(stats->GetVariable(kCacheBackendMisses)),
      cache_fallbacks_(stats->GetVariable(kCacheFallbacks)),
      cache_expirations_(stats->GetVariable(kCacheExpirations)),
      cache_inserts_(stats->GetVariable(kCacheInserts)),
      cache_deletes_(stats->GetVariable(kCacheDeletes)),
      name_(FormatName(cache->Name())) {
  remember_not_cacheable_ttl_seconds_ = kRememberNotCacheableTtlSec;
  remember_fetch_failed_ttl_seconds_ = kRememberFetchFailedTtlSec;
  remember_fetch_dropped_ttl_seconds_ = kRememberFetchDroppedTtlSec;
  max_cacheable_response_content_length_ = kCacheSizeUnlimited;
}

HTTPCache::~HTTPCache() {}

GoogleString HTTPCache::FormatName(StringPiece cache) {
  return StrCat("HTTPCache(", cache, ")");
}

void HTTPCache::SetIgnoreFailurePuts() {
  ignore_failure_puts_.set_value(true);
}

bool HTTPCache::IsCurrentlyValid(const RequestHeaders* request_headers,
                                 const ResponseHeaders& headers, int64 now_ms) {
  if (force_caching_) {
    return true;
  }

  if ((request_headers == NULL && !headers.IsProxyCacheable()) ||
      (request_headers != NULL &&
       !headers.IsProxyCacheableGivenRequest(*request_headers))) {
    // TODO(jmarantz): Should we have a separate 'force' bit that doesn't
    // expired resources to be valid, but does ignore cache-control:private?
    return false;
  }
  if (headers.CacheExpirationTimeMs() > now_ms) {
    return true;
  }
  return false;
}

bool HTTPCache::IsAlreadyExpired(const RequestHeaders* request_headers,
                                 const ResponseHeaders& headers) {
  return !IsCurrentlyValid(request_headers, headers, timer_->NowMs());
}

class HTTPCacheCallback : public CacheInterface::Callback {
 public:
  HTTPCacheCallback(const GoogleString& key, MessageHandler* handler,
                    HTTPCache::Callback* callback, HTTPCache* http_cache)
      : key_(key),
        handler_(handler),
        callback_(callback),
        http_cache_(http_cache) {
    start_us_ = http_cache_->timer()->NowUs();
    start_ms_ = start_us_ / 1000;
  }

  virtual void Done(CacheInterface::KeyState backend_state) {
    HTTPCache::FindResult result = HTTPCache::kNotFound;

    int64 now_us = http_cache_->timer()->NowUs();
    int64 now_ms = now_us / 1000;
    ResponseHeaders* headers = callback_->response_headers();
    bool is_expired = false;
    if ((backend_state == CacheInterface::kAvailable) &&
        callback_->http_value()->Link(value(), headers, handler_) &&
        callback_->IsCacheValid(key_, *headers) &&
        // To resolve Issue 664 we sanitize 'Connection' headers on
        // HTTPCache::Put, but cache entries written before the bug
        // was fixed may have Connection or Transfer-Encoding so treat
        // unsanitary headers as a MISS.  Note that we could at this
        // point actually write the sanitized headers back into the
        // HTTPValue, or better still the HTTPCache, but the former
        // would be slow, and the latter might be complex for
        // write-throughs.  Simply responding with a MISS will let us
        // correct our caches permanently, without having to do a one
        // time full-flush that would impact clean cache entries.
        // Once the caches are all clean, the Sanitize call will be a
        // relatively fast check.
        !headers->Sanitize()) {
      // While stale responses can potentially be used in case of fetch
      // failures, responses invalidated via a cache flush should never be
      // returned under any scenario.
      // TODO(sriharis) : Should we keep statistic for number of invalidated
      // lookups, i.e., #times IsCacheValid returned false?
      // Check that the response is currently valid and is fresh as per the
      // caller's definition of freshness. For example, if we are using a
      // two-level HTTPCache, while freshening a resource, this ensures that we
      // check both caches and don't return a valid response from the L1 cache
      // that is about to expire soon. We instead also check the L2 cache which
      // could have a fresher response. We don't need to pass request_headers
      // here, as we shouldn't have put things in here that required
      // Authorization in the first place.
      int64 override_cache_ttl_ms = callback_->OverrideCacheTtlMs(key_);
      if (override_cache_ttl_ms > 0) {
        // Use the OverrideCacheTtlMs if specified.
        headers->ForceCaching(override_cache_ttl_ms);
      }
      // Is the response still valid?
      is_expired = !http_cache_->IsCurrentlyValid(NULL, *headers, now_ms);
      bool is_valid_and_fresh = (!is_expired) && callback_->IsFresh(*headers);
      int http_status = headers->status_code();

      if (http_status == HttpStatus::kRememberNotCacheableStatusCode ||
          http_status == HttpStatus::kRememberNotCacheableAnd200StatusCode ||
          http_status == HttpStatus::kRememberFetchFailedStatusCode) {
        // If the response was stored as uncacheable and a 200, it may since
        // have since been added to the override caching group. Hence, we
        // consider it invalid if override_cache_ttl_ms > 0.
        if (override_cache_ttl_ms > 0 &&
            http_status == HttpStatus::kRememberNotCacheableAnd200StatusCode) {
          is_valid_and_fresh = false;
        }
        if (is_valid_and_fresh) {
          int64 remember_not_found_time_ms = headers->CacheExpirationTimeMs()
              - start_ms_;
          const char* status = NULL;
          if (http_status == HttpStatus::kRememberNotCacheableStatusCode ||
              http_status ==
                  HttpStatus::kRememberNotCacheableAnd200StatusCode) {
            status = "not-cacheable";
            result = HTTPCache::kRecentFetchNotCacheable;
          } else {
            status = "not-found";
            result = HTTPCache::kRecentFetchFailed;
          }
          if (handler_ != NULL) {
            handler_->Message(kInfo,
                              "HTTPCache key=%s: remembering %s status "
                              "for %ld seconds.",
                              key_.c_str(), status,
                              static_cast<long>(  // NOLINT
                                  remember_not_found_time_ms / 1000));
          }
        }
      } else {
        if (is_valid_and_fresh) {
          result = HTTPCache::kFound;
          if (headers->UpdateCacheHeadersIfForceCached()) {
            // If the cache headers were updated as a result of it being force
            // cached, we need to reconstruct the HTTPValue with the new
            // headers.
            StringPiece content;
            callback_->http_value()->ExtractContents(&content);
            callback_->http_value()->Clear();
            callback_->http_value()->Write(content, handler_);
            callback_->http_value()->SetHeaders(headers);
          }
        } else {
          if (http_cache_->force_caching_ || headers->IsProxyCacheable()) {
            callback_->fallback_http_value()->Link(callback_->http_value());
          }
        }
      }
    }

    // TODO(gee): Perhaps all of this belongs in TimingInfo.
    int64 elapsed_us = std::max(static_cast<int64>(0), now_us - start_us_);
    http_cache_->UpdateStats(key_, backend_state, result,
                             !callback_->fallback_http_value()->Empty(),
                             is_expired, elapsed_us, handler_);
    callback_->ReportLatencyMs(elapsed_us/1000);
    if (result != HTTPCache::kFound) {
      headers->Clear();
      callback_->http_value()->Clear();
    }
    callback_->Done(result);
    delete this;
  }

 private:
  GoogleString key_;
  MessageHandler* handler_;
  HTTPCache::Callback* callback_;
  HTTPCache* http_cache_;
  int64 start_us_;
  int64 start_ms_;

  DISALLOW_COPY_AND_ASSIGN(HTTPCacheCallback);
};

void HTTPCache::Find(const GoogleString& key, MessageHandler* handler,
                     Callback* callback) {
  HTTPCacheCallback* cb = new HTTPCacheCallback(key, handler, callback, this);
  cache_->Get(key, cb);
}

void HTTPCache::UpdateStats(
    const GoogleString& key,
    CacheInterface::KeyState backend_state, FindResult result,
    bool has_fallback, bool is_expired, int64 delta_us,
    MessageHandler* handler) {
  cache_time_us_->Add(delta_us);
  if (backend_state == CacheInterface::kAvailable) {
    cache_backend_hits_->Add(1);
  } else {
    cache_backend_misses_->Add(1);
  }
  if (result == kFound) {
    cache_hits_->Add(1);
    DCHECK(!has_fallback);
  } else {
    cache_misses_->Add(1);
    if (has_fallback) {
      cache_fallbacks_->Add(1);
    }
    if (is_expired) {
      handler->Message(kInfo, "Cache entry is expired: %s", key.c_str());
      cache_expirations_->Add(1);
    }
  }
}

void HTTPCache::RememberNotCacheable(const GoogleString& key,
                                     bool is_200_status_code,
                                     MessageHandler* handler) {
  RememberFetchFailedorNotCacheableHelper(
      key, handler,
      is_200_status_code ? HttpStatus::kRememberNotCacheableAnd200StatusCode :
                           HttpStatus::kRememberNotCacheableStatusCode,
      remember_not_cacheable_ttl_seconds_);
}

void HTTPCache::RememberFetchFailed(const GoogleString& key,
                                    MessageHandler* handler) {
  RememberFetchFailedorNotCacheableHelper(key, handler,
      HttpStatus::kRememberFetchFailedStatusCode,
      remember_fetch_failed_ttl_seconds_);
}

void HTTPCache::RememberFetchDropped(const GoogleString& key,
                                    MessageHandler* handler) {
  RememberFetchFailedorNotCacheableHelper(key, handler,
      HttpStatus::kRememberFetchFailedStatusCode,
      remember_fetch_dropped_ttl_seconds_);
}

void HTTPCache::set_max_cacheable_response_content_length(int64 value) {
  DCHECK(value >= kCacheSizeUnlimited);
  if (value >= kCacheSizeUnlimited) {
    max_cacheable_response_content_length_ = value;
  }
}

void HTTPCache::RememberFetchFailedorNotCacheableHelper(const GoogleString& key,
    MessageHandler* handler, HttpStatus::Code code, int64 ttl_sec) {
  ResponseHeaders headers;
  headers.set_status_code(code);
  int64 now_ms = timer_->NowMs();
  headers.SetDateAndCaching(now_ms, ttl_sec * 1000);
  headers.ComputeCaching();
  Put(key, &headers, "", handler);
}

HTTPValue* HTTPCache::ApplyHeaderChangesForPut(
    const GoogleString& key, int64 start_us, const StringPiece* content,
    ResponseHeaders* headers, HTTPValue* value, MessageHandler* handler) {
  if ((headers->status_code() != HttpStatus::kOK) &&
      ignore_failure_puts_.value()) {
    return NULL;
  }
  DCHECK(value != NULL || content != NULL);

  // Clear out Set-Cookie headers before storing the response into cache.
  bool headers_mutated = headers->Sanitize();
  // TODO(sriharis): Modify date headers.
  // TODO(nikhilmadan): Set etags from hash of content.

  StringPiece new_content;

  // Add an Etag if the original response didn't have any.
  if (headers->Lookup1(HttpAttributes::kEtag) == NULL) {
    GoogleString hash;
    if (content == NULL) {
      bool success = value->ExtractContents(&new_content);
      DCHECK(success);
      content = &new_content;
    }
    hash = hasher_->Hash(*content);
    headers->Add(HttpAttributes::kEtag, FormatEtag(hash));
    headers_mutated = true;
  }

  if (headers_mutated || value == NULL) {
    HTTPValue* new_value = new HTTPValue;  // Will be deleted by calling Put.
    new_value->SetHeaders(headers);
    if (content == NULL) {
      bool success = value->ExtractContents(&new_content);
      DCHECK(success);
      new_value->Write(new_content, handler);
    } else {
      new_value->Write(*content, handler);
    }
    return new_value;
  }
  return value;
}

void HTTPCache::PutInternal(const GoogleString& key, int64 start_us,
                            HTTPValue* value) {
  cache_->Put(key, value->share());
  if (cache_time_us_ != NULL) {
    int64 delta_us = timer_->NowUs() - start_us;
    cache_time_us_->Add(delta_us);
    cache_inserts_->Add(1);
  }
}

// We do not check cache invalidation in Put. It is assumed that the date header
// will be greater than the cache_invalidation_timestamp, if any, in domain
// config.
void HTTPCache::Put(const GoogleString& key, HTTPValue* value,
                    MessageHandler* handler) {
  int64 start_us = timer_->NowUs();
  // Extract headers and contents.
  ResponseHeaders headers;
  bool success = value->ExtractHeaders(&headers, handler);
  DCHECK(success);
  if (!MayCacheUrl(key, headers)) {
    return;
  }
  if (!force_caching_ && !(headers.IsProxyCacheable() &&
                           IsCacheableBodySize(value->contents_size()))) {
    LOG(DFATAL) << "trying to Put uncacheable data for key " << key;
    return;
  }
  // Apply header changes.
  HTTPValue* new_value = ApplyHeaderChangesForPut(key, start_us, NULL,
                                                  &headers, value, handler);
  // Put into underlying cache.
  if (new_value != NULL) {
    PutInternal(key, start_us, new_value);
    // Delete new_value if it is newly allocated.
    if (new_value != value) {
      delete new_value;
    }
  }
}

void HTTPCache::Put(const GoogleString& key, ResponseHeaders* headers,
                    const StringPiece& content, MessageHandler* handler) {
  if (!MayCacheUrl(key, *headers)) {
    return;
  }
  int64 start_us = timer_->NowUs();
  int64 now_ms = start_us / 1000;
  // Note: this check is only valid if the caller didn't send an Authorization:
  // header.
  // TODO(morlovich): expose the request_headers in the API?
  if ((!IsCurrentlyValid(NULL, *headers, now_ms) ||
       !IsCacheableBodySize(content.size()))
      && !force_caching_) {
    return;
  }
  // Apply header changes.
  // Takes ownership of the returned HTTPValue, which is guaranteed to have been
  // allocated by ApplyHeaderChangesForPut.
  scoped_ptr<HTTPValue> value(ApplyHeaderChangesForPut(
      key, start_us, &content, headers, NULL, handler));
  // Put into underlying cache.
  if (value.get() != NULL) {
    PutInternal(key, start_us, value.get());
  }
}

bool HTTPCache::IsCacheableContentLength(ResponseHeaders* headers) const {
  int64 content_length;
  bool content_length_found = headers->FindContentLength(&content_length);
  return (!content_length_found || IsCacheableBodySize(content_length));
}

bool HTTPCache::IsCacheableBodySize(int64 body_size) const {
  return (max_cacheable_response_content_length_ == -1 ||
          body_size <= max_cacheable_response_content_length_);
}

bool HTTPCache::MayCacheUrl(const GoogleString& url,
                            const ResponseHeaders& headers) {
  GoogleUrl gurl(url);
  // TODO(sligocki): Should we restrict this to IsWebValid()?
  // That would break google_font_service_input_resource which uses gfnt:
  if (!gurl.IsAnyValid()) {
    return false;
  }
  if (disable_html_caching_on_https_ && gurl.SchemeIs("https")) {
    return !headers.IsHtmlLike();
  }
  return true;
}

void HTTPCache::Delete(const GoogleString& key) {
  cache_deletes_->Add(1);
  return cache_->Delete(key);
}

void HTTPCache::InitStats(Statistics* statistics) {
  statistics->AddVariable(kCacheTimeUs);
  statistics->AddVariable(kCacheHits);
  statistics->AddVariable(kCacheMisses);
  statistics->AddVariable(kCacheBackendHits);
  statistics->AddVariable(kCacheBackendMisses);
  statistics->AddVariable(kCacheFallbacks);
  statistics->AddVariable(kCacheExpirations);
  statistics->AddVariable(kCacheInserts);
  statistics->AddVariable(kCacheDeletes);
}

GoogleString HTTPCache::FormatEtag(StringPiece hash) {
  return StrCat(kEtagPrefix, hash, "\"");
}

HTTPCache::Callback::~Callback() {
  if (owns_response_headers_) {
    delete response_headers_;
  }
}

void HTTPCache::Callback::ReportLatencyMs(int64 latency_ms) {
  if (is_background_) {
    return;
  }

  if (request_context().get() == NULL) {
    DLOG(FATAL) << "NOTREACHED";
    return;
  }

  ReportLatencyMsImpl(latency_ms);
}

void HTTPCache::Callback::ReportLatencyMsImpl(int64 latency_ms) {
  request_context()->mutable_timing_info()->SetHTTPCacheLatencyMs(latency_ms);
}

}  // namespace net_instaweb
