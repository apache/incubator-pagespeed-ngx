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
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/timing.pb.h"
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
      cache_deletes_(stats->GetVariable(kCacheDeletes)) {
  remember_not_cacheable_ttl_seconds_ = kRememberNotCacheableTtl;
  remember_fetch_failed_ttl_seconds_ = kRememberFetchFailedTtl;
}

HTTPCache::~HTTPCache() {}

void HTTPCache::SetIgnoreFailurePuts() {
  ignore_failure_puts_.set_value(true);
}

bool HTTPCache::IsCurrentlyValid(const ResponseHeaders& headers, int64 now_ms) {
  if (force_caching_) {
    return true;
  }
  if (!headers.IsCacheable() || !headers.IsProxyCacheable()) {
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

bool HTTPCache::IsAlreadyExpired(const ResponseHeaders& headers) {
  return !IsCurrentlyValid(headers, timer_->NowMs());
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
        callback_->IsCacheValid(*headers)) {
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
      // could have a fresher response.
      bool is_valid = http_cache_->IsCurrentlyValid(*headers, now_ms) &&
          callback_->IsFresh(*headers);
      int http_status = headers->status_code();
      if (http_status == HttpStatus::kRememberNotCacheableStatusCode ||
          http_status == HttpStatus::kRememberFetchFailedStatusCode) {
        if (is_valid) {
          int64 remember_not_found_time_ms = headers->CacheExpirationTimeMs()
              - start_ms_;
          const char* status = NULL;
          if (http_status == HttpStatus::kRememberNotCacheableStatusCode) {
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

  virtual bool IsCacheValid(const ResponseHeaders& headers) {
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
                                     MessageHandler* handler) {
  RememberFetchFailedorNotCacheableHelper(key, handler,
      HttpStatus::kRememberNotCacheableStatusCode,
      remember_not_cacheable_ttl_seconds_);
}

void HTTPCache::RememberFetchFailed(const GoogleString& key,
                                    MessageHandler* handler) {
  RememberFetchFailedorNotCacheableHelper(key, handler,
      HttpStatus::kRememberFetchFailedStatusCode,
      remember_fetch_failed_ttl_seconds_);
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
      !(headers.IsCacheable() && headers.IsProxyCacheable())) {
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
  if (!IsCurrentlyValid(*headers, now_ms)) {
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

void HTTPCache::Delete(const GoogleString& key) {
  cache_deletes_->Add(1);
  return cache_->Delete(key);
}

void HTTPCache::Initialize(Statistics* statistics) {
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
  if (owns_timing_info_) {
    delete timing_info_;
  }
}

void HTTPCache::Callback::SetTimingMs(int64 timing_value_ms) {
  timing_info()->set_cache1_ms(timing_value_ms);
}

void HTTPCache::Callback::set_timing_info(TimingInfo* timing_info) {
  DCHECK(!owns_timing_info_);
  if (owns_timing_info_) {
    delete timing_info_;
  }
  timing_info_ = timing_info;
  owns_timing_info_ = false;
}

TimingInfo* HTTPCache::Callback::timing_info() {
  if (timing_info_ == NULL) {
    timing_info_ = new TimingInfo;
    owns_timing_info_ = true;
  }
  return timing_info_;
}

}  // namespace net_instaweb
