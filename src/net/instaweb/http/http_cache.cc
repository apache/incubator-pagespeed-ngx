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
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

// Remember that a Fetch failed for 5 minutes by default.
//
// TODO(jmarantz): We could handle cc-private a little differently:
// in this case we could arguably remember it using the original cc-private ttl.
const int kRememberNotCacheableTtl = 300;
const int kRememberFetchFailedTtl = 300;

// We use an extremely low TTL for load-shed resources since we don't
// want this to get in the way of debugging.
const int kRememberFetchDroppedTtl = 10;

// Maximum size of response content in bytes. -1 indicates that there is no size
// limit.
const int64 kCacheSizeUnlimited = -1;

}  // namespace

const char HTTPCache::kCacheTimeUs[] = "cache_time_us";
const char HTTPCache::kCacheHits[] = "cache_hits";
const char HTTPCache::kCacheMisses[] = "cache_misses";
const char HTTPCache::kCacheExpirations[] = "cache_expirations";
const char HTTPCache::kCacheInserts[] = "cache_inserts";
const char HTTPCache::kCacheDeletes[] = "cache_deletes";
const char HTTPCache::kEtagPrefix[] = "W/PSA-";


HTTPCache::HTTPCache(CacheInterface* cache, Timer* timer, Hasher* hasher,
                     Statistics* stats)
    : cache_(cache),
      timer_(timer),
      hasher_(hasher),
      force_caching_(false),
      cache_time_us_(stats->GetVariable(kCacheTimeUs)),
      cache_hits_(stats->GetVariable(kCacheHits)),
      cache_misses_(stats->GetVariable(kCacheMisses)),
      cache_expirations_(stats->GetVariable(kCacheExpirations)),
      cache_inserts_(stats->GetVariable(kCacheInserts)),
      cache_deletes_(stats->GetVariable(kCacheDeletes)),
      name_(StrCat("HTTPCache using backend : ", cache->Name())) {
  remember_not_cacheable_ttl_seconds_ = kRememberNotCacheableTtl;
  remember_fetch_failed_ttl_seconds_ = kRememberFetchFailedTtl;
  remember_fetch_dropped_ttl_seconds_ = kRememberFetchDroppedTtl;
  max_cacheable_response_content_length_ = kCacheSizeUnlimited;
}

HTTPCache::~HTTPCache() {}

void HTTPCache::SetIgnoreFailurePuts() {
  ignore_failure_puts_.set_value(true);
}

bool HTTPCache::IsCurrentlyValid(const RequestHeaders* request_headers,
                                 const ResponseHeaders& headers, int64 now_ms) {
  if (force_caching_) {
    return true;
  }
  if (!headers.IsCacheable()) {
    return false;
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
  cache_expirations_->Add(1);
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

  virtual void Done(CacheInterface::KeyState state) {
    HTTPCache::FindResult result = HTTPCache::kNotFound;

    int64 now_us = http_cache_->timer()->NowUs();
    int64 now_ms = now_us / 1000;
    ResponseHeaders* headers = callback_->response_headers();
    if ((state == CacheInterface::kAvailable) &&
        callback_->http_value()->Link(value(), headers, handler_) &&
        callback_->IsCacheValid(key_, *headers)) {
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
      bool is_valid = http_cache_->IsCurrentlyValid(NULL, *headers, now_ms) &&
          callback_->IsFresh(*headers);
      int http_status = headers->status_code();

      if (http_status == HttpStatus::kRememberNotCacheableStatusCode ||
          http_status == HttpStatus::kRememberNotCacheableAnd200StatusCode ||
          http_status == HttpStatus::kRememberFetchFailedStatusCode) {
        // If the response was stored as uncacheable and a 200, it may since
        // have since been added to the override caching group. Hence, we
        // consider it invalid if override_cache_ttl_ms > 0.
        if (override_cache_ttl_ms > 0 &&
            http_status == HttpStatus::kRememberNotCacheableAnd200StatusCode) {
          is_valid = false;
        }
        if (is_valid) {
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
        if (is_valid) {
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
          if (http_cache_->force_caching_ ||
              (headers->IsCacheable() && headers->IsProxyCacheable())) {
            callback_->fallback_http_value()->Link(callback_->http_value());
          }
        }
      }
    }

    int64 elapsed_us = std::max(static_cast<int64>(0), now_us - start_us_);
    http_cache_->UpdateStats(result, elapsed_us);
    callback_->SetTimingMs(elapsed_us/1000);
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

namespace {

// TODO(jmarantz): remove this class once all blocking usages of
// HTTPCache are remove in favor of non-blocking usages.
class SynchronizingCallback : public HTTPCache::Callback {
 public:
  SynchronizingCallback()
      : called_(false),
        result_(HTTPCache::kNotFound) {
  }
  bool called() const { return called_; }
  HTTPCache::FindResult result() const { return result_; }

  virtual void Done(HTTPCache::FindResult result) {
    result_ = result;
    called_ = true;
  }

  virtual bool IsCacheValid(const GoogleString& key,
                            const ResponseHeaders& headers) {
    // We don't support custom invalidation policy on the legacy sync path.
    return true;
  }

 private:
  bool called_;
  HTTPCache::FindResult result_;

  DISALLOW_COPY_AND_ASSIGN(SynchronizingCallback);
};

}  // namespace

void HTTPCache::UpdateStats(FindResult result, int64 delta_us) {
  cache_time_us_->Add(delta_us);
  if (result == kFound) {
    cache_hits_->Add(1);
  } else {
    cache_misses_->Add(1);
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
    headers->Add(HttpAttributes::kEtag, kEtagPrefix + hash);
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
  if (!force_caching_ &&
      !(headers.IsCacheable() && headers.IsProxyCacheable() &&
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

void HTTPCache::Delete(const GoogleString& key) {
  cache_deletes_->Add(1);
  return cache_->Delete(key);
}

void HTTPCache::InitStats(Statistics* statistics) {
  statistics->AddVariable(kCacheTimeUs);
  statistics->AddVariable(kCacheHits);
  statistics->AddVariable(kCacheMisses);
  statistics->AddVariable(kCacheExpirations);
  statistics->AddVariable(kCacheInserts);
  statistics->AddVariable(kCacheDeletes);
}

HTTPCache::Callback::~Callback() {
  if (owns_response_headers_) {
    delete response_headers_;
  }
  if (owns_logging_info_) {
    delete logging_info_;
  }
}

void HTTPCache::Callback::SetTimingMs(int64 timing_value_ms) {
  logging_info()->mutable_timing_info()->set_cache1_ms(timing_value_ms);
}

void HTTPCache::Callback::set_logging_info(LoggingInfo* logging_info) {
  DCHECK(!owns_logging_info_);
  if (owns_logging_info_) {
    delete logging_info_;
  }
  logging_info_ = logging_info;
  owns_logging_info_ = false;
}

LoggingInfo* HTTPCache::Callback::logging_info() {
  if (logging_info_ == NULL) {
    logging_info_ = new LoggingInfo;
    owns_logging_info_ = true;
  }
  return logging_info_;
}

}  // namespace net_instaweb
