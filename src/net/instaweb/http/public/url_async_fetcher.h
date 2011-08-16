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

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;
class RequestHeaders;
class ResponseHeaders;
class Writer;

// Abstract base class for encapsulating streaming, asynchronous HTTP fetches.
//
// If you want to fetch a resources, implement this interface, create an
// instance and call UrlAsyncFetcher::StreamingAsyncFetch() with it.
//
// It combines the 3 callbacks we expect to get from fetchers
// (Write, Flush and Done) and adds a HeadersComplete indicator that is
// useful in any place where we want to deal with and send headers before
// Write or Done are called.
class AsyncFetch {
 public:
  AsyncFetch() {}
  virtual ~AsyncFetch();

  // TODO(sligocki): Make headers accessible through AsyncFetch?

  // Called when ResponseHeaders have been set, but before writing contents.
  // Contract: Must be called (exactly once) before Write, Flush or Done.
  virtual void HeadersComplete() = 0;

  // Write a chunk of body content.
  virtual bool Write(const StringPiece& content, MessageHandler* handler) = 0;
  virtual bool Flush(MessageHandler* handler) = 0;

  // Fetch complete.
  virtual void Done(bool success) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AsyncFetch);
};

class UrlAsyncFetcher {
 public:
  static const int64 kUnspecifiedTimeout;
  class Callback {
   public:
    Callback() : modified_(true) {}
    virtual ~Callback();
    virtual void Done(bool success) = 0;

    // Set to true if it's OK to call the callback from a different
    // thread.  The base class implementation returns false.
    virtual bool EnableThreaded() const;

    // Callers should set these before calling Done(), if appropriate.
    void set_modified(bool modified) { modified_ = modified; }
    bool modified() const { return modified_; }

   private:
    // If we are doing a ConditionalFetch, this tells us if the resource
    // has been modified. If true, the response will have the new contents
    // just like for a normal StreamingFetch. If false, only the response
    // headers are meaningful.
    bool modified_;
  };

  virtual ~UrlAsyncFetcher();

  // Fetch a URL, set response_headers and stream the output to response_writer.
  // response_headers and response_writer must be valid until callback->Done().
  //
  // There is an unchecked contract that response_headers are set before the
  // response_writer or callback are used.
  // Caution, several implementations do not satisfy this contract (but should).
  //
  // This function returns true if the request was immediately satisfied.
  // In either case, the callback will be called with the completion status,
  // so it's safe to ignore the return value.
  // TODO(sligocki): GoogleString -> GoogleUrl
  virtual bool StreamingFetch(const GoogleString& url,
                              const RequestHeaders& request_headers,
                              ResponseHeaders* response_headers,
                              Writer* response_writer,
                              MessageHandler* message_handler,
                              Callback* callback) = 0;

  // Fetch with AsyncFetch interface.
  //
  // Default implementation uses StreamingFetch method and calls
  // HeadersComplete right before the first call to Write, Flush or Done.
  //
  // Future implementations ought to call HeadersComplete directly.
  //
  // Return value is the same as StreamingFetch. (Returns true iff callback
  // has already been called by the time Fetch returns.)
  //
  // TODO(sligocki): Stick all other params into AsyncFetch object (url,
  // request_headers, response_headers, message_handler).
  virtual bool Fetch(const GoogleString& url,
                     const RequestHeaders& request_headers,
                     ResponseHeaders* response_headers,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch);

  // Like StreamingFetch, but sends out a conditional GET that will not
  // return the contents if they have not been modified since
  // if_modified_since_ms.
  // TODO(sligocki): GoogleString -> GoogleUrl
  virtual bool ConditionalFetch(const GoogleString& url,
                                int64 if_modified_since_ms,
                                const RequestHeaders& request_headers,
                                ResponseHeaders* response_headers,
                                Writer* response_writer,
                                MessageHandler* message_handler,
                                Callback* callback);

  // Returns a maximum time that we will allow fetches to take, or
  // kUnspecifiedTimeout (the default) if we don't promise to timeout fetches.
  virtual int64 timeout_ms() { return kUnspecifiedTimeout; }
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_URL_ASYNC_FETCHER_H_
