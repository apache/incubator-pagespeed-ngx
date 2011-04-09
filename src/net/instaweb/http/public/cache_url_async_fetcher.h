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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_CACHE_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_CACHE_URL_ASYNC_FETCHER_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/http/public/url_async_fetcher.h"

namespace net_instaweb {

class MessageHandler;
class UrlAsyncFetcher;

// Composes an asynchronous URL fetcher with an http cache, to
// generate an asynchronous caching URL fetcher.
//
// This fetcher will call the callback immediately for entries in the
// cache.  When entries are not in the cache, it will initiate an
// asynchronous 'get' and store the result in the cache, as well as
// calling the passed-in callback.
//
// See also CacheUrlFetcher, which will returns results only for
// URLs still in the cache.
class CacheUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  CacheUrlAsyncFetcher(HTTPCache* cache, UrlAsyncFetcher* fetcher)
      : http_cache_(cache),
        fetcher_(fetcher),
        force_caching_(false) {
  }
  virtual ~CacheUrlAsyncFetcher();

  virtual bool StreamingFetch(
      const GoogleString& url,
      const RequestHeaders& request_headers,
      ResponseHeaders* response_headers,
      Writer* fetched_content_writer,
      MessageHandler* message_handler,
      Callback* callback);

  void set_force_caching(bool force) {
    force_caching_ = force;
    http_cache_->set_force_caching(force);
  }

 private:
  HTTPCache* http_cache_;
  UrlAsyncFetcher* fetcher_;
  bool force_caching_;

  DISALLOW_COPY_AND_ASSIGN(CacheUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_CACHE_URL_ASYNC_FETCHER_H_
