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

#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

class LogRecord;
class MessageHandler;
class RequestHeaders;
class Variable;

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
      log_record_(NULL),
      owns_request_headers_(false),
      owns_response_headers_(false),
      owns_log_record_(false),
      headers_complete_(false) {
  }

  virtual ~AsyncFetch();

  // Called when ResponseHeaders have been set, but before writing contents.
  // Contract: Must be called (at most once) before Write, Flush or Done.
  // Automatically invoked (if neccessary) before the first call to Write,
  // Flush, or Done.  This interface is intended for callers (e.g. Fetchers).
  // Implementors of the AsyncFetch interface must override
  // HandleHeadersComplete.
  void HeadersComplete();

  // Fetch complete.  This interface is intended for callers
  // (e.g. Fetchers).  Implementors must override HandleDone.
  void Done(bool success);

  // Data available.  This interface is intended for callers.  Implementors
  // must override HandlerWrite and HandleFlush.
  virtual bool Write(const StringPiece& sp, MessageHandler* handler);
  virtual bool Flush(MessageHandler* handler);

  // Is the cache entry corresponding to headers valid? Default is that it is
  // valid. Sub-classes can provide specific implementations, e.g., based on
  // cache invalidation timestamp in domain specific options.
  // Used by CacheUrlAsyncFetcher.
  // TODO(nikhilmadan): Consider making this virtual so that subclass authors
  // are forced to look at this function.
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
  // request_headers() or via set_request_headers before calling this.
  const RequestHeaders* request_headers() const;

  // See doc for request_headers and set_request_headers.
  ResponseHeaders* response_headers();
  void set_response_headers(ResponseHeaders* headers);

  virtual bool EnableThreaded() const { return false; }

  // Indicates whether the request is a background fetch. These can be scheduled
  // differently by the fetcher.
  virtual bool IsBackgroundFetch() const { return false; }

  // Resets the 'headers_complete_' flag.
  // TODO(jmarantz): should this also clear the response headers?
  virtual void Reset() { headers_complete_ = false; }

  bool headers_complete() const { return headers_complete_; }

  // Returns a pointer to the logging info, extracting it from the log record.
  virtual LoggingInfo* logging_info();

  // Returns a pointer to a log record that wraps this fetch's logging
  // info, lazily constructing it if needed.
  virtual LogRecord* log_record();

  // Sets the log record to the specifid pointer.  The caller must
  // guarantee that the pointed-to log record remains valid as long as the
  // AsyncFetch is running.
  void set_log_record(LogRecord* log_record);

  // Returns logging information in a string eg. c1:0;c2:2;hf:45;.
  // c1 is cache 1, c2 is cache 2, hf is headers fetch.
  GoogleString LoggingString();

 protected:
  virtual bool HandleWrite(const StringPiece& sp, MessageHandler* handler) = 0;
  virtual bool HandleFlush(MessageHandler* handler) = 0;
  virtual void HandleDone(bool success) = 0;
  virtual void HandleHeadersComplete() = 0;

  // Returns a pointer to the log record, with no lazy construction behavior.
  LogRecord* log_record_or_null() { return log_record_; }

  // Sets LogRecord and claims ownership.
  void set_owned_log_record(LogRecord* log_record);

 private:
  RequestHeaders* request_headers_;
  ResponseHeaders* response_headers_;
  LogRecord* log_record_;
  bool owns_request_headers_;
  bool owns_response_headers_;
  bool owns_log_record_;
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

// Creates an AsyncFetch object using an existing Writer* object,
// which is used to delegate Write and Flush operations.  This
// class is still abstract, and requires inheritors to implement Done().
class AsyncFetchUsingWriter : public AsyncFetch {
 public:
  explicit AsyncFetchUsingWriter(Writer* writer) : writer_(writer) {}
  virtual ~AsyncFetchUsingWriter();

 protected:
  virtual bool HandleWrite(const StringPiece& sp, MessageHandler* handler);
  virtual bool HandleFlush(MessageHandler* handler);

 private:
  Writer* writer_;
  DISALLOW_COPY_AND_ASSIGN(AsyncFetchUsingWriter);
};

// Creates an AsyncFetch object using an existing AsyncFetcher*,
// sharing the response & request headers, and by default delegating
// all 4 Handle methods to the base fetcher.  Any one of them can
// be overridden by inheritors of this class.
class SharedAsyncFetch : public AsyncFetch {
 public:
  explicit SharedAsyncFetch(AsyncFetch* base_fetch);
  virtual ~SharedAsyncFetch();

  AsyncFetch* base_fetch() { return base_fetch_; }
  const AsyncFetch* base_fetch() const { return base_fetch_; }

 protected:
  virtual void HandleDone(bool success) {
    base_fetch_->Done(success);
  }

  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    return base_fetch_->Write(content, handler);
  }

  virtual bool HandleFlush(MessageHandler* handler) {
    return base_fetch_->Flush(handler);
  }

  virtual void HandleHeadersComplete() {
    base_fetch_->HeadersComplete();
  }

  virtual bool EnableThreaded() const {
    return base_fetch_->EnableThreaded();
  }

  virtual bool IsCachedResultValid(const ResponseHeaders& headers) {
    return base_fetch_->IsCachedResultValid(headers);
  }

  virtual bool IsBackgroundFetch() const {
    return base_fetch_->IsBackgroundFetch();
  }

 private:
  AsyncFetch* base_fetch_;
  DISALLOW_COPY_AND_ASSIGN(SharedAsyncFetch);
};

// Creates a SharedAsyncFetch object using an existing AsyncFetch and a fallback
// value that is used in case the fetched response is an error. Note that in
// case the fetched response is an error and we have a non-empty fallback value,
// we completely ignore the fetched response.
// Also, note that this class gets deleted when HandleDone is called.
class FallbackSharedAsyncFetch : public SharedAsyncFetch {
 public:
  // Warning header to be added if a stale response is served.
  static const char kStaleWarningHeaderValue[];

  FallbackSharedAsyncFetch(AsyncFetch* base_fetch, HTTPValue* fallback,
                           MessageHandler* handler);
  virtual ~FallbackSharedAsyncFetch();

  void set_fallback_responses_served(Variable* x) {
    fallback_responses_served_ = x;
  }

  bool serving_fallback() const { return serving_fallback_; }

 protected:
  virtual void HandleDone(bool success);
  virtual bool HandleWrite(const StringPiece& content, MessageHandler* handler);
  virtual bool HandleFlush(MessageHandler* handler);
  virtual void HandleHeadersComplete();

 private:
  // Note that this is only used while serving the fallback response.
  MessageHandler* handler_;
  HTTPValue fallback_;
  bool serving_fallback_;
  Variable* fallback_responses_served_;  // may be NULL.

  DISALLOW_COPY_AND_ASSIGN(FallbackSharedAsyncFetch);
};

// Creates a SharedAsyncFetch object using an existing AsyncFetch and a cached
// value (that may be stale) that is used to conditionally check if the resource
// at the origin has changed. If the resource hasn't changed and we get a 304,
// we serve the cached response, thus avoiding the download of the entire
// content.
// Note that we if you want the conditionally validated resource to be treated
// as a newly fetched with the original ttl, you should use this fetch such that
// the fixing of date headers happens in the base fetch.
// Also, note that this class gets deleted when HandleDone is called.
class ConditionalSharedAsyncFetch : public SharedAsyncFetch {
 public:
  ConditionalSharedAsyncFetch(AsyncFetch* base_fetch, HTTPValue* cached_value,
                              MessageHandler* handler);
  virtual ~ConditionalSharedAsyncFetch();

  void set_num_conditional_refreshes(Variable* x) {
    num_conditional_refreshes_ = x;
  }

 protected:
  virtual void HandleDone(bool success);
  virtual bool HandleWrite(const StringPiece& content, MessageHandler* handler);
  virtual bool HandleFlush(MessageHandler* handler);
  virtual void HandleHeadersComplete();

 private:
  // Note that this is only used while serving the cached response.
  MessageHandler* handler_;
  HTTPValue cached_value_;
  // Indicates that we received a 304 from the origin and are serving out the
  // cached value.
  bool serving_cached_value_;
  // Indicates that we added conditional headers to the request.
  bool added_conditional_headers_to_request_;

  Variable* num_conditional_refreshes_;  // may be NULL.

  DISALLOW_COPY_AND_ASSIGN(ConditionalSharedAsyncFetch);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_ASYNC_FETCH_H_
