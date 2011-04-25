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

#include "net/instaweb/http/public/cache_url_async_fetcher.h"

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/http/public/cache_url_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

namespace {

// This version of the caching async fetcher callback uses the
// caller-supplied response-header buffer, and also forwards the
// content to the client before caching it.
class ForwardingAsyncFetch : public CacheUrlFetcher::AsyncFetch {
 public:
  ForwardingAsyncFetch(const StringPiece& url, HTTPCache* cache,
                       MessageHandler* handler, Callback* callback,
                       Writer* writer, ResponseHeaders* response_headers,
                       bool force_caching)
      : CacheUrlFetcher::AsyncFetch(url, cache, handler, force_caching),
        callback_(callback),
        client_writer_(writer),
        response_headers_(response_headers) {
  }

  virtual void Done(bool success) {
    // Copy the data to the client even with a failure; there may be useful
    // error messages in the content.
    StringPiece contents;
    if (value_.ExtractContents(&contents)) {
      client_writer_->Write(contents, message_handler_);
    } else {
      success = false;
    }

    // Update the cache before calling the client Done callback, which might
    // delete the headers.  Note that if the value is not cacheable, we will
    // record in the cache that this entry is not cacheable, but we will still
    // call the async fetcher callback with the value.  This allows us to
    // do resource-serving via CacheUrlAsyncFetcher, where we need to serve
    // the resources even if they are not cacheable.
    //
    // We do not update the cache at all if the fetch failed.
    if (success) {
      UpdateCache();
    }

    callback_->Done(success);
    delete this;
  }

  virtual ResponseHeaders* response_headers() { return response_headers_; }

 private:
  Callback* callback_;
  Writer* client_writer_;
  ResponseHeaders* response_headers_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingAsyncFetch);
};

}  // namespace

CacheUrlAsyncFetcher::~CacheUrlAsyncFetcher() {
}

bool CacheUrlAsyncFetcher::StreamingFetch(
    const GoogleString& url, const RequestHeaders& request_headers,
    ResponseHeaders* response_headers, Writer* writer, MessageHandler* handler,
    Callback* callback) {
  HTTPValue value;
  StringPiece contents;
  bool ret = false;
  if ((http_cache_->Find(url, &value, response_headers, handler) ==
       HTTPCache::kFound) &&
      !CacheUrlFetcher::RememberNotCached(*response_headers) &&
      value.ExtractContents(&contents)) {
    bool success = writer->Write(contents, handler);
    callback->Done(success);
    ret = true;
  } else {
    response_headers->Clear();
    ForwardingAsyncFetch* fetch = new ForwardingAsyncFetch(
        url, http_cache_, handler, callback, writer, response_headers,
        force_caching_);
    fetch->Start(fetcher_, request_headers);
  }
  return ret;
}

}  // namespace net_instaweb
