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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_CACHE_URL_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_CACHE_URL_FETCHER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/http/public/url_fetcher.h"

namespace net_instaweb {

class MessageHandler;
class UrlAsyncFetcher;

// Composes a URL fetcher with an http cache, to generate a caching
// URL fetcher.
//
// This fetcher will return true and provide an immediate result for
// entries in the cache.  When entries are not in the cache, it will
// initiate an asynchronous 'get' and store the result in the cache.
//
// When the supplied url fetcher indicates that the entry is not
// cacheable, then we cache the fact that the resource is not cacheable
// for 5 minutes, and during that time we will not request the element
// again, but instead will return false from StreamingUrlFetch.  This
// will allow us to quickly punt on rewrites for non-cacheable items.
//
// TODO(jmarantz): allow experimental adjustment of the timeout
// period, and whether or not we will retain resources marked as
// cache-control:private.
//
// See also CacheUrlAsyncFetcher, which will yield its results asynchronously
// for elements not in the cache, and immediately for results that are.
class CacheUrlFetcher : public UrlFetcher {
 public:
  CacheUrlFetcher(HTTPCache* cache, UrlFetcher* fetcher)
      : http_cache_(cache),
        sync_fetcher_(fetcher),
        async_fetcher_(NULL),
        force_caching_(false) {
  }
  CacheUrlFetcher(HTTPCache* cache, UrlAsyncFetcher* fetcher)
      : http_cache_(cache),
        sync_fetcher_(NULL),
        async_fetcher_(fetcher),
        force_caching_(false) {
  }
  virtual ~CacheUrlFetcher();

  virtual bool StreamingFetchUrl(
      const GoogleString& url,
      const RequestHeaders& request_headers,
      ResponseHeaders* response_headers,
      Writer* fetched_content_writer,
      MessageHandler* message_handler);

  // Helper class to hold state for a single asynchronous fetch.  When
  // the fetch is complete, we'll put the resource in the cache.
  //
  // This class is declared here to facilitate code-sharing with
  // CacheAsyncUrlFetcher.
  class AsyncFetch : public UrlAsyncFetcher::Callback {
   public:
    AsyncFetch(const StringPiece& url, HTTPCache* cache,
               MessageHandler* handler, bool force_caching);
    virtual ~AsyncFetch();

    virtual void Done(bool success);
    void Start(UrlAsyncFetcher* fetcher,
               const RequestHeaders& request_headers);

    // This hook allows the CacheUrlAsyncFetcher to capture the headers for
    // its client, while still enabling this class to cache them.
    virtual ResponseHeaders* response_headers() = 0;

    void UpdateCache();

    virtual bool EnableThreaded() const;

   protected:
    HTTPValue value_;
    MessageHandler* message_handler_;

   private:
    GoogleString url_;
    HTTPCache* http_cache_;
    bool force_caching_;
  };

  void set_force_caching(bool force) {
    force_caching_ = force;
    http_cache_->set_force_caching(force);
  }

  // Determines whether the HTTP response headers indicate a response that
  // we have cached to indicate that the response is not cacheable.  This
  // is in this header file so the functionality can be shared between
  // CacheUrlAsyncFetcher and CacheUrlFetcher.
  static bool RememberNotCached(const ResponseHeaders& headers);

 private:
  HTTPCache* http_cache_;
  UrlFetcher* sync_fetcher_;
  UrlAsyncFetcher* async_fetcher_;
  bool force_caching_;

  DISALLOW_COPY_AND_ASSIGN(CacheUrlFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_CACHE_URL_FETCHER_H_
