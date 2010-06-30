/**
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

#include "net/instaweb/util/public/cache_url_fetcher.h"
#include "net/instaweb/util/public/http_cache.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

namespace net_instaweb {

namespace {

// The synchronous version of the caching fetch must supply a
// response_headers buffer that will still be valid at when the fetch
// completes and the callback executes.
class AsyncFetchWithHeaders : public CacheUrlFetcher::AsyncFetch {
 public:
  AsyncFetchWithHeaders(const StringPiece& url, HTTPCache* cache,
                        MessageHandler* handler, bool force_caching)
      : CacheUrlFetcher::AsyncFetch(url, cache, handler, force_caching) {
  }

  virtual MetaData* ResponseHeaders() {
    return &response_headers_;
  }
 private:
  SimpleMetaData response_headers_;
};

}  // namespace

CacheUrlFetcher::AsyncFetch::AsyncFetch(const StringPiece& url,
                                        HTTPCache* cache,
                                        MessageHandler* handler,
                                        bool force_caching)
    : message_handler_(handler),
      url_(url.data(), url.size()),
      writer_(&content_),
      http_cache_(cache),
      force_caching_(force_caching) {
}

CacheUrlFetcher::AsyncFetch::~AsyncFetch() {
}

void CacheUrlFetcher::AsyncFetch::UpdateCache() {
  // TODO(jmarantz): allow configuration of whether we ignore
  // IsProxyCacheable, e.g. for content served from the same host
  MetaData* response_headers = ResponseHeaders();
  if ((force_caching_ || response_headers->IsCacheable()) &&
      (http_cache_->Query(url_.c_str(), message_handler_) ==
       CacheInterface::kNotFound)) {
    http_cache_->Put(url_.c_str(), *response_headers, content_,
                     message_handler_);
  }
}

void CacheUrlFetcher::AsyncFetch::Done(bool success) {
  if (success) {
    UpdateCache();
  } else {
    message_handler_->Info(url_.c_str(), 0, "Fetch failed, not caching.");
    // TODO(jmarantz): cache that this request is not fetchable
  }
  delete this;
}

void CacheUrlFetcher::AsyncFetch::Start(
    UrlAsyncFetcher* fetcher, const MetaData& request_headers) {
  fetcher->StreamingFetch(url_, request_headers, ResponseHeaders(),
                          &writer_, message_handler_, this);
}

CacheUrlFetcher::~CacheUrlFetcher() {
}


bool CacheUrlFetcher::StreamingFetchUrl(
    const std::string& url, const MetaData& request_headers,
    MetaData* response_headers, Writer* writer, MessageHandler* handler) {
  bool ret = false;
  ret = http_cache_->Get(url.c_str(), response_headers, writer, handler);
  if (!ret) {
    if (sync_fetcher_ != NULL) {
      // We need to hang onto a copy of the data so we can shove it
      // into the cache, which currently lacks a streaming Put.
      std::string content;
      StringWriter string_writer(&content);
      ret = sync_fetcher_->StreamingFetchUrl(
          url, request_headers, response_headers, &string_writer, handler);
      writer->Write(content, handler);
      if (ret) {
        if (force_caching_ || response_headers->IsCacheable()) {
          http_cache_->Put(url.c_str(), *response_headers, content, handler);
        }
        ret = true;
      } else {
        // TODO(jmarantz): Consider caching that this request is not fetchable
      }
    } else {
      AsyncFetch* fetch = new AsyncFetchWithHeaders(url, http_cache_, handler,
                                                    force_caching_);
      fetch->Start(async_fetcher_, request_headers);
    }
  }
  return ret;
}

}  // namespace net_instaweb
