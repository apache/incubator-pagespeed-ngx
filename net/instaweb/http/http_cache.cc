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
#include "net/instaweb/http/public/http_cache_failure.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/inflating_fetch.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/opt/logging/request_timing_info.h"

namespace net_instaweb {

namespace {

// Increment this value to flush HTTP cache.
// Similar to RewriteOptions::kOptionVersion which can be used to flush the
// metadata cache.
const int kHttpCacheVersion = 3;

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
      cache_levels_(1),
      compression_level_(0),
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
  max_cacheable_response_content_length_ = kCacheSizeUnlimited;
  SetVersion(kHttpCacheVersion);
}

void HTTPCache::SetVersion(int version_number) {
  version_prefix_ = StrCat("v", IntegerToString(version_number), "/");
}

HTTPCache::~HTTPCache() {}

GoogleString HTTPCache::FormatName(StringPiece cache) {
  return StrCat("HTTPCache(", cache, ")");
}

void HTTPCache::SetIgnoreFailurePuts() {
  ignore_failure_puts_.set_value(true);
}

bool HTTPCache::IsExpired(const ResponseHeaders& headers, int64 now_ms) {
  if (force_caching_) {
    return false;
  }

  if (headers.CacheExpirationTimeMs() > now_ms) {
    return false;
  }
  return true;
}

bool HTTPCache::IsExpired(const ResponseHeaders& headers) {
  return IsExpired(headers, timer_->NowMs());
}

class HTTPCacheCallback : public CacheInterface::Callback {
 public:
  HTTPCacheCallback(const GoogleString& key,
                    const GoogleString& fragment,
                    MessageHandler* handler,
                    HTTPCache::Callback* callback, HTTPCache* http_cache)
      : key_(key),
        fragment_(fragment),
        handler_(handler),
        callback_(callback),
        http_cache_(http_cache),
        result_(HTTPCache::kNotFound, kFetchStatusNotSet),
        cache_level_(0) {
    start_us_ = http_cache_->timer()->NowUs();
    start_ms_ = start_us_ / 1000;
  }

  virtual bool ValidateCandidate(const GoogleString& key,
                                 CacheInterface::KeyState backend_state) {
    ++cache_level_;
    int64 now_us = http_cache_->timer()->NowUs();
    int64 now_ms = now_us / 1000;
    ResponseHeaders* headers = callback_->response_headers();
    bool is_expired = false;
    if ((backend_state == CacheInterface::kAvailable) &&
        callback_->http_value()->Link(value(), headers, handler_) &&
        (http_cache_->force_caching_ ||
         headers->IsProxyCacheable(callback_->req_properties(),
                                   callback_->RespectVaryOnResources(),
                                   ResponseHeaders::kHasValidator)) &&
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
      is_expired = http_cache_->IsExpired(*headers, now_ms);
      bool is_valid_and_fresh = !is_expired && callback_->IsFresh(*headers);
      HttpStatus::Code http_status =
          static_cast<HttpStatus::Code>(headers->status_code());

      if (HttpCacheFailure::IsFailureCachingStatus(http_status)) {
        // If the response was stored as uncacheable and a 200, it may since
        // have been added to the override caching group, hence, if
        // override_cache_ttl_ms > 0 we have to disregard the cached failure.
        if (override_cache_ttl_ms > 0 &&
            http_status == HttpStatus::kRememberNotCacheableAnd200StatusCode) {
          is_valid_and_fresh = false;
        }
        if (is_valid_and_fresh) {  // is the failure caching still valid?
          int64 remaining_cache_failure_time_ms =
              headers->CacheExpirationTimeMs() - start_ms_;
          result_ = HTTPCache::FindResult(
              HTTPCache::kRecentFailure,
              HttpCacheFailure::DecodeFailureCachingStatus(http_status));
          if (handler_ != NULL) {
            handler_->Message(kInfo,
                              "HTTPCache key=%s fragment=%s: remembering "
                              "recent failure for %ld seconds.",
                              key_.c_str(), fragment_.c_str(),
                              static_cast<long>(  // NOLINT
                                  remaining_cache_failure_time_ms / 1000));
          }
        }
      } else {
        if (is_valid_and_fresh) {
          result_ = HTTPCache::FindResult(HTTPCache::kFound, kFetchStatusOK);
          callback_->fallback_http_value()->Clear();
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
          if (http_cache_->force_caching_ ||
              headers->IsProxyCacheable(callback_->req_properties(),
                                        callback_->RespectVaryOnResources(),
                                        ResponseHeaders::kHasValidator)) {
            ResponseHeaders fallback_headers;
            if (callback_->request_context()->accepts_gzip() ||
                !callback_->http_value()->ExtractHeaders(&fallback_headers,
                                                         handler_) ||
                !InflatingFetch::UnGzipValueIfCompressed(
                    *callback_->http_value(), &fallback_headers,
                    callback_->fallback_http_value(), handler_)) {
              // If we don't need to unzip, or can't unzip, then just
              // link the value and fallback together.
              callback_->fallback_http_value()->Link(callback_->http_value());
            }
          }
        }
      }
    }

    // TODO(gee): Perhaps all of this belongs in TimingInfo.
    int64 elapsed_us = std::max(static_cast<int64>(0), now_us - start_us_);
    http_cache_->cache_time_us()->Add(elapsed_us);
    callback_->ReportLatencyMs(elapsed_us/1000);
    if (cache_level_ == http_cache_->cache_levels() ||
        result_.status == HTTPCache::kFound) {
      http_cache_->UpdateStats(key_, fragment_, backend_state, result_,
                               !callback_->fallback_http_value()->Empty(),
                               is_expired, handler_);
    }

    if (result_.status != HTTPCache::kFound) {
      headers->Clear();
      callback_->http_value()->Clear();
    } else if (!callback_->request_context()->accepts_gzip() &&
               headers->IsGzipped()) {
      HTTPValue new_value;
      GoogleString inflated;
      if (InflatingFetch::UnGzipValueIfCompressed(
              *callback_->http_value(), headers, &new_value, handler_)) {
        callback_->http_value()->Link(&new_value);
      }
    }
    start_ms_ = now_ms;
    start_us_ = now_us;
    return result_.status == HTTPCache::kFound;
  }

  virtual void Done(CacheInterface::KeyState backend_state) {
    callback_->Done(result_);
    delete this;
  }

 private:
  GoogleString key_;
  GoogleString fragment_;
  RequestHeaders::Properties req_properties_;
  MessageHandler* handler_;
  HTTPCache::Callback* callback_;
  HTTPCache* http_cache_;
  HTTPCache::FindResult result_;
  int64 start_us_;
  int64 start_ms_;
  int cache_level_;

  DISALLOW_COPY_AND_ASSIGN(HTTPCacheCallback);
};

void HTTPCache::Find(const GoogleString& key, const GoogleString& fragment,
                     MessageHandler* handler, Callback* callback) {
  HTTPCacheCallback* cb = new HTTPCacheCallback(
      key, fragment, handler, callback, this);
  cache_->Get(CompositeKey(key, fragment), cb);
}

void HTTPCache::UpdateStats(
    const GoogleString& key, const GoogleString& fragment,
    CacheInterface::KeyState backend_state, FindResult result,
    bool has_fallback, bool is_expired, MessageHandler* handler) {
  if (backend_state == CacheInterface::kAvailable) {
    cache_backend_hits_->Add(1);
  } else {
    cache_backend_misses_->Add(1);
  }
  if (result.status == kFound) {
    cache_hits_->Add(1);
    DCHECK(!has_fallback);
  } else {
    cache_misses_->Add(1);
    if (has_fallback) {
      cache_fallbacks_->Add(1);
    }
    if (is_expired) {
      handler->Message(kInfo, "Cache entry is expired: %s (fragment=%s)",
                       key.c_str(), fragment.c_str());
      cache_expirations_->Add(1);
    }
  }
}

void HTTPCache::set_max_cacheable_response_content_length(int64 value) {
  DCHECK(value >= kCacheSizeUnlimited);
  if (value >= kCacheSizeUnlimited) {
    max_cacheable_response_content_length_ = value;
  }
}

void HTTPCache::RememberFailure(
    const GoogleString& key,
    const GoogleString& fragment,
    FetchResponseStatus failure_status,
    MessageHandler* handler) {
  HttpStatus::Code code = HttpCacheFailure::EncodeFailureCachingStatus(
      failure_status);
  int64 ttl_sec = remember_failure_policy_.ttl_sec_for_status[failure_status];
  ResponseHeaders headers;
  headers.set_status_code(code);
  int64 now_ms = timer_->NowMs();
  headers.SetDateAndCaching(now_ms, ttl_sec * 1000);
  headers.ComputeCaching();
  Put(key, fragment, RequestHeaders::Properties(),
      ResponseHeaders::kRespectVaryOnResources,
      &headers, "", handler);
}

HTTPValue* HTTPCache::ApplyHeaderChangesForPut(
    int64 start_us, const StringPiece* content, ResponseHeaders* headers,
    HTTPValue* value, MessageHandler* handler) {
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

void HTTPCache::PutInternal(bool preserve_response_headers,
                            const GoogleString& key,
                            const GoogleString& fragment, int64 start_us,
                            HTTPValue* value, ResponseHeaders* response_headers,
                            MessageHandler* handler) {
  HTTPValue working_value;

  // Check to see if the HTTPValue is worth gzipping.
  // TODO(jcrowell): investigate switching to mod_gzip from mod_deflate so that
  // we can set some heuristic on minimum size where compressing the data no
  // longer makes filesize smaller. For now, we pay the penalty of compression
  // from mod_deflate if we don't precompress everything, so just compress
  // everything.
  if (!value->Empty() && compression_level_ != 0) {
    const ContentType* type = response_headers->DetermineContentType();
    if ((type != NULL) && type->IsCompressible() &&
        !response_headers->IsGzipped()) {
      ResponseHeaders* headers_to_gzip = response_headers;
      ResponseHeaders headers_copy;
      if (preserve_response_headers) {
        headers_copy.CopyFrom(*response_headers);
        headers_to_gzip = &headers_copy;
      }

      // Canonicalize header order so x-original-content-length is always
      // last.  This helps tests act more consistently.
      const char* orig_content_length = headers_to_gzip->Lookup1(
          HttpAttributes::kXOriginalContentLength);
      if (orig_content_length != NULL) {
        GoogleString save_content_length = orig_content_length;
        headers_to_gzip->RemoveAll(HttpAttributes::kXOriginalContentLength);
        headers_to_gzip->Add(HttpAttributes::kXOriginalContentLength,
                             save_content_length);
      }
      headers_to_gzip->ComputeCaching();

      if (InflatingFetch::GzipValue(compression_level_, *value,
                                    &working_value, headers_to_gzip,
                                    handler)) {
        // The resource is text (js, css, html, svg, etc.), and not previously
        // compressed, so we'll compress it and stick the new compressed version
        // in the cache.
        value = &working_value;
      }
    }
  } else if ((compression_level_ == 0) && response_headers->IsGzipped()) {
    ResponseHeaders* headers_to_unzip = response_headers;
    ResponseHeaders headers_copy;
    if (preserve_response_headers) {
      headers_copy.CopyFrom(*response_headers);
      headers_to_unzip = &headers_copy;
    }

    if (InflatingFetch::UnGzipValueIfCompressed(
            *value, headers_to_unzip, &working_value, handler)) {
      value = &working_value;
    }
  }
  // TODO(jcrowell): prevent the unzip-rezip flow when sending compressed data
  // directly to a client through InflatingFetch.
  cache_->Put(CompositeKey(key, fragment), value->share());
  if (cache_time_us_ != NULL) {
    int64 delta_us = timer_->NowUs() - start_us;
    cache_time_us_->Add(delta_us);
  }
}

// We do not check cache invalidation in Put. It is assumed that the date header
// will be greater than the cache_invalidation_timestamp, if any, in domain
// config.
void HTTPCache::Put(const GoogleString& key, const GoogleString& fragment,
                    RequestHeaders::Properties req_properties,
                    const HttpOptions& http_options,
                    HTTPValue* value, MessageHandler* handler) {
  int64 start_us = timer_->NowUs();
  // Extract headers and contents.
  ResponseHeaders headers(http_options);
  bool success = value->ExtractHeaders(&headers, handler);
  DCHECK(success);
  if (!MayCacheUrl(key, headers)) {
    return;
  }
  if (!force_caching_ &&
      !(headers.IsProxyCacheable(
          req_properties,
          ResponseHeaders::GetVaryOption(http_options.respect_vary),
          ResponseHeaders::kHasValidator) &&
        IsCacheableBodySize(value->contents_size()))) {
    LOG(DFATAL) << "trying to Put uncacheable data for key=" << key
                << " fragment=" << fragment;
    return;
  }
  // Apply header changes.
  HTTPValue* new_value = ApplyHeaderChangesForPut(
      start_us, NULL, &headers, value, handler);
  // Put into underlying cache.
  if (new_value != NULL) {
    PutInternal(false /* preserve_response_headers */,
                key, fragment, start_us, new_value, &headers, handler);
    if (cache_inserts_ != NULL) {
      cache_inserts_->Add(1);
    }

    // Delete new_value if it is newly allocated.
    if (new_value != value) {
      delete new_value;
    }
  }
}

void HTTPCache::Put(const GoogleString& key, const GoogleString& fragment,
                    RequestHeaders::Properties req_properties,
                    ResponseHeaders::VaryOption respect_vary_on_resources,
                    ResponseHeaders* headers,
                    const StringPiece& content, MessageHandler* handler) {
  if (!MayCacheUrl(key, *headers)) {
    return;
  }
  int64 start_us = timer_->NowUs();
  int64 now_ms = start_us / 1000;
  if ((IsExpired(*headers, now_ms) ||
       !headers->IsProxyCacheable(req_properties, respect_vary_on_resources,
                                  ResponseHeaders::kHasValidator) ||
       !IsCacheableBodySize(content.size())) &&
      !force_caching_) {
    return;
  }
  // Apply header changes.
  // Takes ownership of the returned HTTPValue, which is guaranteed to have been
  // allocated by ApplyHeaderChangesForPut.
  scoped_ptr<HTTPValue> value(
      ApplyHeaderChangesForPut(start_us, &content, headers, NULL, handler));
  // Put into underlying cache.
  if (value.get() != NULL) {
    PutInternal(true /* preserve_response_headers */,
                key, fragment, start_us, value.get(), headers, handler);
    if (cache_inserts_ != NULL) {
      cache_inserts_->Add(1);
    }
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

void HTTPCache::Delete(const GoogleString& key, const GoogleString& fragment) {
  cache_deletes_->Add(1);
  DeleteInternal(CompositeKey(key, fragment));
}

void HTTPCache::DeleteInternal(const GoogleString& key_fragment) {
  cache_->Delete(key_fragment);
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
    LOG(DFATAL) << "NOTREACHED";
    return;
  }

  ++cache_level_;
  if (cache_level_ == 1) {
    request_context()->mutable_timing_info()->SetHTTPCacheLatencyMs(latency_ms);
  } else if (cache_level_ == 2) {
    request_context()->mutable_timing_info()->SetL2HTTPCacheLatencyMs(
        latency_ms);
  }
}

}  // namespace net_instaweb
