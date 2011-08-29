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

#include "net/instaweb/http/public/url_async_fetcher.h"

#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

const int64 UrlAsyncFetcher::kUnspecifiedTimeout = 0;

UrlAsyncFetcher::~UrlAsyncFetcher() {
}

UrlAsyncFetcher::Callback::~Callback() {
}

AsyncFetch::~AsyncFetch() {
}

bool UrlAsyncFetcher::Callback::EnableThreaded() const {
  // Most fetcher callbacks are not prepared to be called from a different
  // thread.
  return false;
}

namespace {

class WriterCallbackFetch : public AsyncFetch {
 public:
  WriterCallbackFetch(Writer* writer, UrlAsyncFetcher::Callback* callback)
      : writer_(writer), callback_(callback) {}
  virtual ~WriterCallbackFetch() {}

  virtual void HeadersComplete() {}

  virtual bool Write(const StringPiece& content, MessageHandler* handler) {
    return writer_->Write(content, handler);
  }

  virtual bool Flush(MessageHandler* handler) {
    return writer_->Flush(handler);
  }

  virtual void Done(bool success) {
    callback_->Done(success);
    delete this;
  }

 private:
  Writer* writer_;
  UrlAsyncFetcher::Callback* callback_;

  DISALLOW_COPY_AND_ASSIGN(WriterCallbackFetch);
};

}  // namespace

// Default implementation of StreamingFetch uses Fetch.
bool UrlAsyncFetcher::StreamingFetch(const GoogleString& url,
                                     const RequestHeaders& request_headers,
                                     ResponseHeaders* response_headers,
                                     Writer* response_writer,
                                     MessageHandler* message_handler,
                                     Callback* callback) {
  WriterCallbackFetch* fetch =
      new WriterCallbackFetch(response_writer, callback);
  return Fetch(url, request_headers, response_headers, message_handler, fetch);
}

namespace {

// Thin interface classes to allow FixupAsyncFetch to be used as both
// a Writer and a Callback.
class AsyncFetchWriter : public Writer {
 public:
  AsyncFetchWriter(AsyncFetch* fetch) : fetch_(fetch) {}
  virtual ~AsyncFetchWriter() {}

  virtual bool Write(const StringPiece& content, MessageHandler* handler) {
    return fetch_->Write(content, handler);
  }

  virtual bool Flush(MessageHandler* handler) {
    return fetch_->Flush(handler);
  }

 private:
  AsyncFetch* fetch_;

  DISALLOW_COPY_AND_ASSIGN(AsyncFetchWriter);
};

class AsyncFetchCallback : public UrlAsyncFetcher::Callback {
 public:
  AsyncFetchCallback(AsyncFetch* fetch) : fetch_(fetch) {}
  virtual ~AsyncFetchCallback() {}

  virtual void Done(bool status) { fetch_->Done(status); }

 private:
  AsyncFetch* fetch_;

  DISALLOW_COPY_AND_ASSIGN(AsyncFetchCallback);
};

// An AsyncFetch that automatically calls HeadersComplete the first time that
// Write, Flush or Done are called.
//
// Used to implement StreamingAsyncFetch using StreamingFetch.
class FixupAsyncFetch : public AsyncFetch {
 public:
  FixupAsyncFetch(AsyncFetch* base_fetch)
      : base_fetch_(base_fetch),
        writer_interface_(new AsyncFetchWriter(this)),
        callback_interface_(new AsyncFetchCallback(this)),
        headers_complete_(false) {}
  virtual ~FixupAsyncFetch() {}

  // Interfaces to let this be used with old StreamingFetch interface.
  Writer* writer_interface() { return writer_interface_.get(); }
  UrlAsyncFetcher::Callback* callback_interface() {
    return callback_interface_.get();
  }

  virtual void HeadersComplete() {
    DCHECK(!headers_complete_);
    base_fetch_->HeadersComplete();
    headers_complete_ = true;
  }

  virtual bool Write(const StringPiece& content, MessageHandler* handler) {
    if (!headers_complete_) {
      HeadersComplete();
    }
    return base_fetch_->Write(content, handler);
  }

  virtual bool Flush(MessageHandler* handler) {
    if (!headers_complete_) {
      HeadersComplete();
    }
    return base_fetch_->Flush(handler);
  }

  virtual void Done(bool success) {
    if (!headers_complete_) {
      HeadersComplete();
    }
    base_fetch_->Done(success);
    delete this;
  }

 private:
  AsyncFetch* base_fetch_;

  scoped_ptr<Writer> writer_interface_;
  scoped_ptr<UrlAsyncFetcher::Callback> callback_interface_;

  bool headers_complete_;

  DISALLOW_COPY_AND_ASSIGN(FixupAsyncFetch);
};

}  // namespace

// Default implementation of Fetch uses StreamingFetch.
bool UrlAsyncFetcher::Fetch(const GoogleString& url,
                            const RequestHeaders& request_headers,
                            ResponseHeaders* response_headers,
                            MessageHandler* message_handler,
                            AsyncFetch* fetch) {
  // Fixup fetch provides a Writer* and Callback* interface and runs
  // HeadersComplete when appropriate (before all other AsyncFetch calls).
  FixupAsyncFetch* fixup_fetch = new FixupAsyncFetch(fetch);
  return StreamingFetch(url, request_headers, response_headers,
                        fixup_fetch->writer_interface(),
                        message_handler,
                        fixup_fetch->callback_interface());
}

namespace {

// Callback for default implementation of ConditionalFetch.
// Gets called back with status from StreamingFetch which does not have
// modified bit set. Sets modified bit appropriately based upon HTTP
// response status code.
class ConditionalFetchCallback : public UrlAsyncFetcher::Callback {
 public:
  ConditionalFetchCallback(UrlAsyncFetcher::Callback* base_callback,
                           const ResponseHeaders* response_headers)
      : base_callback_(base_callback),
        response_headers_(response_headers) {}
  virtual ~ConditionalFetchCallback() {}

  virtual void Done(bool success) {
    base_callback_->set_modified(
        response_headers_->status_code() != HttpStatus::kNotModified);
    base_callback_->Done(success);
    delete this;
  }

  // TODO(sligocki): See if we can set EnableThreaded() = True

 private:
  UrlAsyncFetcher::Callback* base_callback_;
  const ResponseHeaders* response_headers_;

  DISALLOW_COPY_AND_ASSIGN(ConditionalFetchCallback);
};

}  // namespace

bool UrlAsyncFetcher::ConditionalFetch(const GoogleString& url,
                                       int64 mod_time_ms,
                                       const RequestHeaders& request_headers,
                                       ResponseHeaders* response_headers,
                                       Writer* response_writer,
                                       MessageHandler* message_handler,
                                       Callback* base_callback) {
  // Default implementation just sets the If-Modified-Since GET header.
  RequestHeaders conditional_headers;
  conditional_headers.CopyFrom(request_headers);
  GoogleString mod_time_string;
  if (ConvertTimeToString(mod_time_ms, &mod_time_string)) {
    conditional_headers.Add(HttpAttributes::kIfModifiedSince, mod_time_string);
  } else {
    message_handler->Message(kError, "Invalid time value %s",
                             Integer64ToString(mod_time_ms).c_str());
  }
  Callback* sub_callback =
      new ConditionalFetchCallback(base_callback, response_headers);
  return StreamingFetch(url, conditional_headers,
                        response_headers, response_writer,
                        message_handler, sub_callback);
}

}  // namespace instaweb
