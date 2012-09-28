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

#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/inflating_fetch.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

const int64 UrlAsyncFetcher::kUnspecifiedTimeout = 0;

// Statistics group names.
const char UrlAsyncFetcher::kStatisticsGroup[] = "Statistics";

UrlAsyncFetcher::~UrlAsyncFetcher() {
}

UrlAsyncFetcher::Callback::~Callback() {
}

bool UrlAsyncFetcher::Callback::EnableThreaded() const {
  // Most fetcher callbacks are not prepared to be called from a different
  // thread.
  return false;
}

namespace {

class WriterCallbackFetch : public AsyncFetchUsingWriter {
 public:
  WriterCallbackFetch(Writer* writer, const RequestHeaders& headers,
                      UrlAsyncFetcher::Callback* callback)
      : AsyncFetchUsingWriter(writer),
        callback_(callback) {
    request_headers()->CopyFrom(headers);
  }
  virtual ~WriterCallbackFetch() {}

  virtual bool EnableThreaded() const { return callback_->EnableThreaded(); }

 protected:
  virtual void HandleHeadersComplete() {}
  virtual void HandleDone(bool success) {
    callback_->Done(success);
    delete this;
  }

 private:
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
      new WriterCallbackFetch(response_writer, request_headers, callback);
  fetch->request_headers()->CopyFrom(request_headers);
  fetch->set_response_headers(response_headers);
  return Fetch(url, message_handler, fetch);
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
  explicit AsyncFetchCallback(AsyncFetch* async_fetch)
      : async_fetch_(async_fetch) {}
  virtual ~AsyncFetchCallback() {}
  virtual void Done(bool success) {
    async_fetch_->Done(success);
    delete this;
  }
  virtual bool EnableThreaded() const {
    return async_fetch_->EnableThreaded();
  }

 private:
  AsyncFetch* async_fetch_;

  DISALLOW_COPY_AND_ASSIGN(AsyncFetchCallback);
};

// An AsyncFetch that automatically calls HeadersComplete the first time that
// Write, Flush or Done are called.
//
// Used to implement StreamingAsyncFetch using StreamingFetch.
class FixupAsyncFetch : public SharedAsyncFetch {
 public:
  FixupAsyncFetch(AsyncFetch* base_fetch)
      : SharedAsyncFetch(base_fetch),
        writer_interface_(new AsyncFetchWriter(this)),
        callback_interface_(new AsyncFetchCallback(this)),
        headers_complete_(false) {
    set_response_headers(base_fetch->response_headers());
  }
  virtual ~FixupAsyncFetch() {}

  // Interfaces to let this be used with old StreamingFetch interface.
  Writer* writer_interface() { return writer_interface_.get(); }
  UrlAsyncFetcher::Callback* callback_interface() {
    return callback_interface_;
  }

  virtual bool EnableThreaded() const { return base_fetch()->EnableThreaded(); }

 protected:
  virtual void HandleDone(bool success) {
    base_fetch()->Done(success);
    delete this;
  }

 private:
  scoped_ptr<Writer> writer_interface_;
  UrlAsyncFetcher::Callback* callback_interface_;

  bool headers_complete_;

  DISALLOW_COPY_AND_ASSIGN(FixupAsyncFetch);
};

}  // namespace

// Default implementation of Fetch uses StreamingFetch.
bool UrlAsyncFetcher::Fetch(const GoogleString& url,
                            MessageHandler* message_handler,
                            AsyncFetch* fetch) {
  // Fixup fetch provides a Writer* and Callback* interface and runs
  // HeadersComplete when appropriate (before all other AsyncFetch calls).
  FixupAsyncFetch* fixup_fetch = new FixupAsyncFetch(fetch);
  return StreamingFetch(url,
                        *fetch->request_headers(),
                        fetch->response_headers(),
                        fixup_fetch->writer_interface(),
                        message_handler,
                        fixup_fetch->callback_interface());
}

void UrlAsyncFetcher::ShutDown() {
}

AsyncFetch* UrlAsyncFetcher::EnableInflation(AsyncFetch* fetch) const {
  InflatingFetch* inflating_fetch = new InflatingFetch(fetch);
  if (fetch_with_gzip_) {
    inflating_fetch->EnableGzipFromBackend();
  }
  return inflating_fetch;
}

}  // namespace instaweb
