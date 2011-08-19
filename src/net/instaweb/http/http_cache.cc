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

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
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


HTTPCache::HTTPCache(CacheInterface* cache, Timer* timer, Statistics* stats)
    : cache_(cache),
      timer_(timer),
      force_caching_(false),
      cache_time_us_(stats->GetVariable(kCacheTimeUs)),
      cache_hits_(stats->GetVariable(kCacheHits)),
      cache_misses_(stats->GetVariable(kCacheMisses)),
      cache_expirations_(stats->GetVariable(kCacheExpirations)),
      cache_inserts_(stats->GetVariable(kCacheInserts)) {
}

HTTPCache::~HTTPCache() {}

void HTTPCache::SetReadOnly() {
  readonly_.set_value(true);
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
        http_cache_->IsCurrentlyValid(*headers, now_ms)) {
      if (headers->status_code() == HttpStatus::kRememberNotFoundStatusCode) {
        int64 remember_not_found_time_ms = headers->CacheExpirationTimeMs()
            - start_ms_;
        if (handler_ != NULL) {
          handler_->Info(
              key_.c_str(), 0,
              "HTTPCache: remembering not-found status for %ld seconds",
              static_cast<long>(remember_not_found_time_ms / 1000));  // NOLINT
        }
        result = HTTPCache::kRecentFetchFailedDoNotRefetch;
      } else {
        result = HTTPCache::kFound;
      }
    }

    http_cache_->UpdateStats(result, now_us - start_us_);
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

 private:
  bool called_;
  HTTPCache::FindResult result_;

  DISALLOW_COPY_AND_ASSIGN(SynchronizingCallback);
};

}  // namespace

// Legacy blocking version of HTTPCache::Find.  This will fail if the
// cache implementation does not call its callback immediately.
//
// TODO(jmarantz): remove this when blocking callers of HTTPCache::Find
// are removed from the codebase.
HTTPCache::FindResult HTTPCache::Find(
    const GoogleString& key, HTTPValue* value, ResponseHeaders* headers,
    MessageHandler* handler) {
  SharedString cache_buffer;

  SynchronizingCallback callback;
  Find(key, handler, &callback);
  CHECK(callback.called()) << "Non-blocking caches not yet supported";
  if (callback.result() == kFound) {
    if (value != NULL) {
      value->Link(callback.http_value());
    }
  }
  if (headers != NULL) {
    headers->CopyFrom(*callback.response_headers());
  }
  return callback.result();
}

CacheInterface::KeyState HTTPCache::Query(const GoogleString& key) {
  HTTPCache::FindResult find_result = Find(key, NULL, NULL, NULL);
  CacheInterface::KeyState state = (find_result == kFound)
      ? CacheInterface::kAvailable : CacheInterface::kNotFound;
  return state;
}

void HTTPCache::UpdateStats(FindResult result, int64 delta_us) {
  if (cache_time_us_ != NULL) {
    cache_time_us_->Add(delta_us);
    if (result == kFound) {
      cache_hits_->Add(1);
    } else {
      cache_misses_->Add(1);
    }
  }
}

void HTTPCache::RememberNotCacheable(const GoogleString& key,
                                     MessageHandler* handler) {
  ResponseHeaders headers;
  headers.set_status_code(HttpStatus::kRememberNotFoundStatusCode);
  headers.Add(HttpAttributes::kCacheControl, kRememberNotFoundCacheControl);
  int64 now_ms = timer_->NowMs();
  headers.UpdateDateHeader(HttpAttributes::kDate, now_ms);
  headers.ComputeCaching();
  Put(key, &headers, "", handler);
}

void HTTPCache::PutHelper(const GoogleString& key, int64 now_us,
                          HTTPValue* value, MessageHandler* handler) {
  if (readonly_.value()) {
    return;
  }

  SharedString* shared_string = value->share();
  cache_->Put(key, shared_string);
  if (cache_time_us_ != NULL) {
    int64 delta_us = timer_->NowUs() - now_us;
    cache_time_us_->Add(delta_us);
    cache_inserts_->Add(1);
  }
}

void HTTPCache::Put(const GoogleString& key, HTTPValue* value,
                    MessageHandler* handler) {
  PutHelper(key, timer_->NowUs(), value, handler);
}

void HTTPCache::Put(const GoogleString& key, ResponseHeaders* headers,
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

void HTTPCache::Delete(const GoogleString& key) {
  return cache_->Delete(key);
}

void HTTPCache::Initialize(Statistics* statistics) {
  statistics->AddVariable(kCacheTimeUs);
  statistics->AddVariable(kCacheHits);
  statistics->AddVariable(kCacheMisses);
  statistics->AddVariable(kCacheExpirations);
  statistics->AddVariable(kCacheInserts);
}

HTTPCache::Callback::~Callback() {
}

}  // namespace net_instaweb
