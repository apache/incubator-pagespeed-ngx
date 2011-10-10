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

class HTTPCache;
class MessageHandler;
class RequestHeaders;
class ResponseHeaders;

// Composes an asynchronous URL fetcher with an http cache, to
// generate an asynchronous caching URL fetcher.
//
// This fetcher will asynchronously check the cache. If the url
// is found in cache and is still valid, the fetch's callback will be
// called right away. Otherwise an async fetch will be performed in
// the fetcher, the result of which will be written into the cache.
//
// TODO(sligocki): In order to use this for fetching resources for rewriting
// we'd need to integrate resource locking in this class. Do we want that?
class CacheUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  CacheUrlAsyncFetcher(HTTPCache* cache, UrlAsyncFetcher* fetcher,
                       bool respect_vary)
      : http_cache_(cache),
        fetcher_(fetcher),
        respect_vary_(respect_vary),
        ignore_recent_fetch_failed_(false) {
  }
  virtual ~CacheUrlAsyncFetcher();

  virtual bool Fetch(const GoogleString& url,
                     const RequestHeaders& request_headers,
                     ResponseHeaders* response_headers,
                     MessageHandler* message_handler,
                     AsyncFetch* base_fetch);

  void set_ignore_recent_fetch_failed(bool x) {
    ignore_recent_fetch_failed_ = x;
  }

 private:
  // Not owned by CacheUrlAsyncFetcher.
  HTTPCache* http_cache_;
  UrlAsyncFetcher* fetcher_;

  bool respect_vary_;
  bool ignore_recent_fetch_failed_;

  DISALLOW_COPY_AND_ASSIGN(CacheUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_CACHE_URL_ASYNC_FETCHER_H_
