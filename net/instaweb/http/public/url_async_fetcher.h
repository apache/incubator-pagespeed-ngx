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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_URL_ASYNC_FETCHER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

class AsyncFetch;
class MessageHandler;

// UrlAsyncFetcher is an interface for asynchronously fetching URLs.
// The results of a fetch are asynchronously passed back to the callbacks
// in the supplied AsyncFetch object.
class UrlAsyncFetcher {
 public:
  static const int64 kUnspecifiedTimeout;

  virtual ~UrlAsyncFetcher();

  // Asynchronously fetch a URL, set the response headers and stream the
  // contents to fetch and call fetch->Done() when the fetch finishes.
  //
  // There is an unchecked contract that response_headers are set before the
  // response_writer or callback are used.
  // Caution, several implementations do not satisfy this contract (but should).
  //
  // TODO(sligocki): GoogleString -> GoogleUrl or at least StringPiece.
  // TODO(sligocki): Include the URL in the fetch, like the request headers.
  virtual void Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch) = 0;

  // Determine if the fetcher supports fetching using HTTPS. By default we
  // assume a fetcher can.
  virtual bool SupportsHttps() const { return true; }

  // Returns a maximum time that we will allow fetches to take, or
  // kUnspecifiedTimeout (the default) if we don't promise to timeout fetches.
  virtual int64 timeout_ms() { return kUnspecifiedTimeout; }

  // Stops all active fetches and prevents further fetches from starting,
  // calling back to ->Done(false).
  //
  // Base-class implementation is empty for forward compatibility.
  virtual void ShutDown();

  // Always requests content from servers using gzip.  If the request headers
  // do not accept that encoding, then it will be decompressed while streaming.
  void set_fetch_with_gzip(bool x) { fetch_with_gzip_ = x; }
  bool fetch_with_gzip() const { return fetch_with_gzip_; }

  // Returns a new InflatingFetch to handle auto-inflating the
  // response if needed.
  AsyncFetch* EnableInflation(AsyncFetch* fetch) const;

 protected:
  // Put this in protected to make sure nobody constructs this class except
  // for subclasses.
  UrlAsyncFetcher() : fetch_with_gzip_(false) {}

 private:
  bool fetch_with_gzip_;

  DISALLOW_COPY_AND_ASSIGN(UrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_URL_ASYNC_FETCHER_H_
