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

// Author: sligocki@google.com (Shawn Ligocki)

#ifndef NET_INSTAWEB_HTTP_PUBLIC_CACHE_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_CACHE_URL_ASYNC_FETCHER_H_

#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class AsyncFetch;
class Histogram;
class HTTPCache;
class MessageHandler;
class Variable;

// Composes an asynchronous URL fetcher with an http cache, to
// generate an asynchronous caching URL fetcher.
//
// This fetcher will asynchronously check the cache. If the url
// is found in cache and is still valid, the fetch's callback will be
// called right away. Otherwise an async fetch will be performed in
// the fetcher, the result of which will be written into the cache.
// In case the fetch fails and there is a stale response in the cache, we serve
// the stale response.
//
// TODO(sligocki): In order to use this for fetching resources for rewriting
// we'd need to integrate resource locking in this class. Do we want that?
class CacheUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  CacheUrlAsyncFetcher(HTTPCache* cache, UrlAsyncFetcher* fetcher,
                       bool respect_vary)
      : http_cache_(cache),
        fetcher_(fetcher),
        backend_first_byte_latency_(NULL),
        fallback_responses_served_(NULL),
        respect_vary_(respect_vary),
        ignore_recent_fetch_failed_(false),
        serve_stale_if_fetch_error_(false) {
  }
  virtual ~CacheUrlAsyncFetcher();

  virtual bool SupportsHttps() const { return fetcher_->SupportsHttps(); }

  virtual bool Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* base_fetch);

  HTTPCache* http_cache() const { return http_cache_; }
  UrlAsyncFetcher* fetcher() const { return fetcher_; }

  void set_backend_first_byte_latency_histogram(Histogram* x) {
    backend_first_byte_latency_ = x;
  }

  Histogram* backend_first_byte_latency_histogram() const {
    return backend_first_byte_latency_;
  }

  void set_fallback_responses_served(Variable* x) {
    fallback_responses_served_ = x;
  }

  Variable* fallback_responses_served() const {
    return fallback_responses_served_;
  }

  bool respect_vary() const { return respect_vary_; }

  void set_ignore_recent_fetch_failed(bool x) {
    ignore_recent_fetch_failed_ = x;
  }

  bool ignore_recent_fetch_failed() const {
    return ignore_recent_fetch_failed_;
  }

  void set_serve_stale_if_fetch_error(bool x) {
    serve_stale_if_fetch_error_ = x;
  }

  bool serve_stale_if_fetch_error() const {
    return serve_stale_if_fetch_error_;
  }

 private:
  // Not owned by CacheUrlAsyncFetcher.
  HTTPCache* http_cache_;
  UrlAsyncFetcher* fetcher_;
  Histogram* backend_first_byte_latency_;  // may be NULL.
  Variable* fallback_responses_served_;  // may be NULL.

  bool respect_vary_;
  bool ignore_recent_fetch_failed_;
  bool serve_stale_if_fetch_error_;

  DISALLOW_COPY_AND_ASSIGN(CacheUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_CACHE_URL_ASYNC_FETCHER_H_
