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
//
// UrlFetcher is an interface for asynchronously fetching urls.  The
// caller must supply a callback to be called when the fetch is complete.

#ifndef NET_INSTAWEB_HTTP_PUBLIC_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_URL_ASYNC_FETCHER_H_

#include "base/basictypes.h"
#include <string>

namespace net_instaweb {

class MessageHandler;
class RequestHeaders;
class ResponseHeaders;
class Writer;

class UrlAsyncFetcher {
 public:
  static const int64 kUnspecifiedTimeout;
  struct Callback {
    virtual ~Callback();
    virtual void Done(bool success) = 0;

    // Set to true if it's OK to call the callback from a different
    // thread.  The base class implementation returns false.
    virtual bool EnableThreaded() const;
  };

  virtual ~UrlAsyncFetcher();

  // Fetch a URL, streaming the output to fetched_content_writer, and
  // returning the headers.  request_headers is optional -- it can be NULL.
  // response_headers and fetched_content_writer must be valid until
  // the call to Done().
  //
  // This function returns true if the request was immediately satisfied.
  // In either case, the callback will be called with the completion status,
  // so it's safe to ignore the return value.
  virtual bool StreamingFetch(const std::string& url,
                              const RequestHeaders& request_headers,
                              ResponseHeaders* response_headers,
                              Writer* response_writer,
                              MessageHandler* message_handler,
                              Callback* callback) = 0;
  // Returns a maximum time that we will allow fetches to take, or
  // kUnspecifiedTimeout (the default) if we don't promise to timeout fetches.
  virtual int64 timeout_ms() { return kUnspecifiedTimeout; }
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_URL_ASYNC_FETCHER_H_
