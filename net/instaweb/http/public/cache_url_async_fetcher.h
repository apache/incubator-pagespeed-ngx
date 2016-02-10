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

#include "base/logging.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

class AsyncFetch;
class Hasher;
class Histogram;
class HTTPCache;
class MessageHandler;
class NamedLockManager;
class Sequence;
class Variable;

// Composes an asynchronous URL fetcher with an http cache, to
// generate an asynchronous caching URL fetcher.
//
// This fetcher will asynchronously check the cache. If the url
// is found in cache and is still valid, the fetch's callback will be
// called right away.  This includes any cached failures or that URL
// is uncacheable, unless set_ignore_recent_fetch_failed(true) is called.
// Otherwise (if fetcher != NULL) an async fetch will be performed in the
// fetcher, the result of which will be written into the cache. In case the
// fetch fails and there is a stale response in the cache, we serve the stale
// response.
//
// If fetcher == NULL, this will only perform a cache lookup and then call
// the callback immediately.
//
// In case of cache hit and resource is about to expire (80% of TTL or 5 mins
// which ever is minimum), it will trigger background fetch to freshen the value
// in cache. Background fetch only be triggered only if async_op_hooks_ != NULL,
// otherwise, fetcher object accessed by BackgroundFreshenFetch may be deleted
// by the time origin fetch finishes.
//
// TODO(sligocki): In order to use this for fetching resources for rewriting
// we'd need to integrate resource locking in this class. Do we want that?
class CacheUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  // Interface for managing async operations in CacheUrlAsyncFetcher. It helps
  // to protect the lifetime of the injected objects.
  class AsyncOpHooks {
   public:
    AsyncOpHooks() {}
    virtual ~AsyncOpHooks();

    // Called when CacheUrlAsyncFetcher is about to start async operation.
    virtual void StartAsyncOp() = 0;
    // Called when async operation is ended.
    virtual void FinishAsyncOp() = 0;
  };

  // None of these are owned by CacheUrlAsyncFetcher.
  CacheUrlAsyncFetcher(const Hasher* lock_hasher,
                       NamedLockManager* lock_manager,
                       HTTPCache* cache,
                       const GoogleString& fragment,
                       AsyncOpHooks* async_op_hooks,
                       UrlAsyncFetcher* fetcher);
  virtual ~CacheUrlAsyncFetcher();

  virtual bool SupportsHttps() const { return fetcher_->SupportsHttps(); }

  virtual void Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* base_fetch);

  // HTTP status code used to indicate that we failed the Fetch because
  // result was not found in cache. (Only happens if fetcher_ == NULL).
  static const int kNotInCacheStatus;

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

  void set_fallback_responses_served_while_revalidate(Variable* x) {
    fallback_responses_served_while_revalidate_ = x;
  }

  Variable* fallback_responses_served_while_revalidate() const {
    return fallback_responses_served_while_revalidate_;
  }

  void set_num_conditional_refreshes(Variable* x) {
    num_conditional_refreshes_ = x;
  }

  Variable* num_conditional_refreshes() const {
    return num_conditional_refreshes_;
  }

  void set_num_proactively_freshen_user_facing_request(Variable* x) {
    num_proactively_freshen_user_facing_request_ = x;
  }

  Variable* num_proactively_freshen_user_facing_request() const {
    return num_proactively_freshen_user_facing_request_;
  }

  void set_respect_vary(bool x) { respect_vary_ = x; }
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

  void set_serve_stale_while_revalidate_threshold_sec(int64 x) {
    serve_stale_while_revalidate_threshold_sec_ = x;
  }

  int64 serve_stale_while_revalidate_threshold_sec() const {
    return serve_stale_while_revalidate_threshold_sec_;
  }

  void set_default_cache_html(bool x) { default_cache_html_ = x; }
  bool default_cache_html() const { return default_cache_html_; }

  void set_proactively_freshen_user_facing_request(bool x) {
    proactively_freshen_user_facing_request_ = x;
  }
  bool proactively_freshen_user_facing_request() const {
    return proactively_freshen_user_facing_request_;
  }

  void set_own_fetcher(bool x) { own_fetcher_ = x; }

  // By default, the CacheUrlAsyncFetcher will call its fetcher callbacks
  // on whatever thread the cache or the fetcher happen to be on (e.g. the
  // memcached thread).  Setting the response_sequence ensures that cached
  // responses call their callbacks by queueing on that sequence rather than
  // executing them directly.
  //
  // TODO(jmarantz): this currently only makes sense to call when there is no
  // fetcher, as the implementation does not queue up fetcher-callbacks; only
  // cache callbacks.  But if we want to fully implement bandwidth optimization
  // at the expense of latency, we are going to have to add sequencing of
  // fetch callbacks in addition to cache callbacks.  This is not hard to do,
  // but we don't need it yet so it's not done.
  void set_response_sequence(Sequence* x) {
    CHECK(fetcher_ == NULL);
    response_sequence_ = x;
  }

 private:
  // Not owned by CacheUrlAsyncFetcher.
  const Hasher* lock_hasher_;
  NamedLockManager* lock_manager_;
  HTTPCache* http_cache_;
  GoogleString fragment_;
  UrlAsyncFetcher* fetcher_;  // may be NULL.
  AsyncOpHooks* async_op_hooks_;

  Histogram* backend_first_byte_latency_;  // may be NULL.
  Variable* fallback_responses_served_;  // may be NULL.
  Variable* fallback_responses_served_while_revalidate_;  // may be NULL.
  Variable* num_conditional_refreshes_;  // may be NULL.
  Variable* num_proactively_freshen_user_facing_request_;  // may be NULL.

  bool respect_vary_;
  bool ignore_recent_fetch_failed_;
  bool serve_stale_if_fetch_error_;
  bool default_cache_html_;
  bool proactively_freshen_user_facing_request_;
  bool own_fetcher_;  // set true to transfer ownership of fetcher to this.
  int64 serve_stale_while_revalidate_threshold_sec_;
  Sequence* response_sequence_;

  DISALLOW_COPY_AND_ASSIGN(CacheUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_CACHE_URL_ASYNC_FETCHER_H_
