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

// Author: jmarantz@google.com (Joshua Marantz)
//         sligocki@google.com (Shawn Ligocki)
//
// AsyncFetch represents the context of a single fetch.

#ifndef NET_INSTAWEB_HTTP_PUBLIC_ASYNC_FETCH_H_
#define NET_INSTAWEB_HTTP_PUBLIC_ASYNC_FETCH_H_

#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

class MessageHandler;
class RequestHeaders;

// Abstract base class for encapsulating streaming, asynchronous HTTP fetches.
//
// If you want to fetch a resources, implement this interface, create an
// instance and call UrlAsyncFetcher::Fetch() with it.
//
// It combines the 3 callbacks we expect to get from fetchers
// (Write, Flush and Done) and adds a HeadersComplete indicator that is
// useful in any place where we want to deal with and send headers before
// Write or Done are called.
//
// Note that it automatically invokes HeadersComplete before the first call to
// Write, Flush or Done.
class AsyncFetch : public Writer {
 public:
  AsyncFetch() :
      request_headers_(NULL),
      response_headers_(NULL),
      owns_request_headers_(false),
      owns_response_headers_(false),
      headers_complete_(false) {
  }
  virtual ~AsyncFetch();

  // Called when ResponseHeaders have been set, but before writing
  // contents.  Contract: Must be called (exactly once) before Write,
  // Flush or Done.  This interface is intended for callers
  // (e.g. Fetchers).  Implementors of the AsyncFetch interface must
  // override HandleHeadersComplete.
  void HeadersComplete();

  // Fetch complete.  This interface is intended for callers
  // (e.g. Fetchers).  Implementors must override HandleDone.
  void Done(bool success);

  // Fetch complete.  This interface is intended for callers.  Implementors
  // must override HandlerWrite and HandleFlush.
  virtual bool Write(const StringPiece& sp, MessageHandler* handler);
  virtual bool Flush(MessageHandler* handler);

  // Is the cache entry corresponding to headers valid? Default is that it is
  // valid. Sub-classes can provide specific implementations, e.g., based on
  // cache invalidation timestamp in domain specific options.
  // Used by CacheUrlAsyncFetcher.
  virtual bool IsCachedResultValid(const ResponseHeaders& headers) {
    return true;
  }

  // Returns a pointer to the request-headers, lazily constructing
  // them if needed.  If they are constructed here (as opposed to
  // being set with set_request_headers) then they will be owned by
  // the class instance.
  RequestHeaders* request_headers();

  // Sets the request-headers to the specifid pointer.  The caller must
  // guarantee that the pointed-to headers remain valid as long as the
  // AsyncFetch is running.
  void set_request_headers(RequestHeaders* headers);

  // Returns the request_headers as a const pointer: it is required
  // that the RequestHeaders be pre-initialized via non-const
  // request_heaers() or via set_request_headers before calling this.
  const RequestHeaders* request_headers() const;

  // See doc for request_headers and set_request_headers.
  ResponseHeaders* response_headers();
  void set_response_headers(ResponseHeaders* headers);

  virtual bool EnableThreaded() const { return false; }

  void Reset() { headers_complete_ = false; }

 protected:
  virtual bool HandleWrite(const StringPiece& sp, MessageHandler* handler) = 0;
  virtual bool HandleFlush(MessageHandler* handler) = 0;
  virtual void HandleDone(bool success) = 0;
  virtual void HandleHeadersComplete() = 0;

 private:
  RequestHeaders* request_headers_;
  ResponseHeaders* response_headers_;
  bool owns_request_headers_;
  bool owns_response_headers_;
  bool headers_complete_;

  DISALLOW_COPY_AND_ASSIGN(AsyncFetch);
};

// Class to represent an Async fetch that collects the response-data into
// a string, which can be accessed via buffer() and cleared via Reset().
//
// TODO(jmarantz): move StringAsyncFetch into its own file.
class StringAsyncFetch : public AsyncFetch {
 public:
  StringAsyncFetch() : buffer_pointer_(&buffer_) { Init(); }

  explicit StringAsyncFetch(GoogleString* buffer) : buffer_pointer_(buffer) {
    Init();
  }
  virtual ~StringAsyncFetch();

  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    content.AppendToString(buffer_pointer_);
    return true;
  }
  virtual bool HandleFlush(MessageHandler* handler) { return true; }
  virtual void HandleHeadersComplete() {}
  virtual void HandleDone(bool success) {
    success_ = success;
    done_ = true;
  }

  bool success() const { return success_; }
  bool done() const { return done_; }
  const GoogleString& buffer() const { return *buffer_pointer_; }

  void Reset() {
    done_ = false;
    success_ = false;
    buffer_pointer_->clear();
    response_headers()->Clear();
    AsyncFetch::Reset();
  }

 private:
  void Init() {
    success_ = false;
    done_ = false;
  }

  GoogleString buffer_;
  GoogleString* buffer_pointer_;
  bool success_;
  bool done_;

  DISALLOW_COPY_AND_ASSIGN(StringAsyncFetch);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_ASYNC_FETCH_H_
