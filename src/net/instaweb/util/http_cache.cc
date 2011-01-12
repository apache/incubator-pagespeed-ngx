/**
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

#include "net/instaweb/util/public/http_cache.h"

#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/http_value.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/meta_data.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

// Remember that a Fetch failed for 5 minutes.
// TODO(jmarantz): consider allowing this to be configurable.
//
// TODO(jmarantz): We could handle cc-private a little differently:
// in this case we could arguably remember it using the original cc-private ttl.
const char kRememberNotFoundCacheControl[] = "max-age=300";

}  // namespace

const char HTTPCache::kCacheTimeUs[] = "cache_time_us";
const char HTTPCache::kCacheHits[] = "cache_hits";
const char HTTPCache::kCacheMisses[] = "cache_misses";
const char HTTPCache::kCacheExpirations[] = "cache_expirations";
const char HTTPCache::kCacheInserts[] = "cache_inserts";


HTTPCache::~HTTPCache() {}

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
  if (cache_expirations_ != NULL) {
    cache_expirations_->Add(1);
  }
  return false;
}

bool HTTPCache::IsAlreadyExpired(const ResponseHeaders& headers) {
  return !IsCurrentlyValid(headers, timer_->NowMs());
}

HTTPCache::FindResult HTTPCache::Find(
    const std::string& key, HTTPValue* value, ResponseHeaders* headers,
    MessageHandler* handler) {
  SharedString cache_buffer;

  int64 start_us = timer_->NowUs();
  int64 now_ms = start_us / 1000;
  FindResult ret = kNotFound;

  if ((cache_->Get(key, &cache_buffer) &&
       value->Link(&cache_buffer, headers, handler))) {
    if (IsCurrentlyValid(*headers, now_ms)) {
      if (headers->status_code() == HttpStatus::kRememberNotFoundStatusCode) {
        long remember_not_found_time_ms = headers->CacheExpirationTimeMs()
            - now_ms;
        handler->Info(key.c_str(), 0,
                      "HTTPCache: remembering not-found status for %ld seconds",
                      remember_not_found_time_ms / 1000);
        ret = kRecentFetchFailedDoNotRefetch;
      } else {
        ret = kFound;
      }
    }
  }

  if (cache_time_us_ != NULL) {
    long delta_us = timer_->NowUs() - start_us;
    cache_time_us_->Add(delta_us);
    if (ret == kFound) {
      cache_hits_->Add(1);
    } else {
      cache_misses_->Add(1);
    }
  }

  if (ret != kFound) {
    headers->Clear();
    value->Clear();
  }
  return ret;
}

void HTTPCache::RememberNotCacheable(const std::string& key,
                                     MessageHandler* handler) {
  ResponseHeaders headers;
  headers.set_status_code(HttpStatus::kRememberNotFoundStatusCode);
  headers.Add(HttpAttributes::kCacheControl, kRememberNotFoundCacheControl);
  int64 now_ms = timer_->NowMs();
  headers.UpdateDateHeader(HttpAttributes::kDate, now_ms);
  headers.ComputeCaching();
  Put(key, &headers, "", handler);
}

void HTTPCache::PutHelper(const std::string& key, int64 now_us,
                          HTTPValue* value, MessageHandler* handler) {
  SharedString* shared_string = value->share();
  cache_->Put(key, shared_string);
  if (cache_time_us_ != NULL) {
    int64 delta_us = timer_->NowUs() - now_us;
    cache_time_us_->Add(delta_us);
    cache_inserts_->Add(1);
  }
}

void HTTPCache::Put(const std::string& key, HTTPValue* value,
                    MessageHandler* handler) {
  PutHelper(key, timer_->NowUs(), value, handler);
}

void HTTPCache::Put(const std::string& key, ResponseHeaders* headers,
                    const StringPiece& content,
                    MessageHandler* handler) {
  int64 start_us = timer_->NowUs();
  int64 now_ms = start_us / 1000;
  if (!IsCurrentlyValid(*headers, now_ms)) {
    return;
  }

  HTTPValue value;
  value.SetHeaders(headers);
  value.Write(content, handler);
  PutHelper(key, start_us, &value, handler);
}

CacheInterface::KeyState HTTPCache::Query(const std::string& key) {
  return cache_->Query(key);
}

void HTTPCache::Delete(const std::string& key) {
  return cache_->Delete(key);
}

void HTTPCache::Initialize(Statistics* statistics) {
  statistics->AddVariable(kCacheTimeUs);
  statistics->AddVariable(kCacheHits);
  statistics->AddVariable(kCacheMisses);
  statistics->AddVariable(kCacheExpirations);
  statistics->AddVariable(kCacheInserts);
}

void HTTPCache::SetStatistics(Statistics* statistics) {
  if (statistics != NULL) {
    cache_time_us_ = statistics->GetVariable(kCacheTimeUs);
    cache_hits_ = statistics->GetVariable(kCacheHits);
    cache_misses_ = statistics->GetVariable(kCacheMisses);
    cache_expirations_ = statistics->GetVariable(kCacheExpirations);
    cache_inserts_ = statistics->GetVariable(kCacheInserts);
  }
}

}  // namespace net_instaweb
