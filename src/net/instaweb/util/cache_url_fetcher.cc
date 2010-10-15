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

#include "base/basictypes.h"
#include "net/instaweb/util/public/http_cache.h"
#include "net/instaweb/util/public/http_value.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

namespace net_instaweb {

namespace {

const char kRememberNotCached[] = "X-Instaweb-Disable-cache";

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

  DISALLOW_COPY_AND_ASSIGN(AsyncFetchWithHeaders);
};

}  // namespace

CacheUrlFetcher::AsyncFetch::AsyncFetch(const StringPiece& url,
                                        HTTPCache* cache,
                                        MessageHandler* handler,
                                        bool force_caching)
    : message_handler_(handler),
      url_(url.data(), url.size()),
      http_cache_(cache),
      force_caching_(force_caching) {
}

CacheUrlFetcher::AsyncFetch::~AsyncFetch() {
}

/*
 * Note: this can be called from a different thread than the one where
 * the request was made.  We are depending on the caches being thread-safe
 * if necessary.
 */
void CacheUrlFetcher::AsyncFetch::UpdateCache() {
  // TODO(jmarantz): allow configuration of whether we ignore
  // IsProxyCacheable, e.g. for content served from the same host
  MetaData* response_headers = ResponseHeaders();
  if ((http_cache_->Query(url_.c_str()) == CacheInterface::kNotFound)) {
    if (force_caching_ || response_headers->IsProxyCacheable()) {
      value_.SetHeaders(*response_headers);
      http_cache_->Put(url_.c_str(), &value_, message_handler_);
    } else {
      // Leave value_ alone as we prep a cache entry to indicate that
      // this url is not cacheable.  This is because this code is
      // shared with cache_url_async_fetcher.cc, which needs to
      // actually pass through the real value and headers, even while
      // remembering the non-cachability of the URL.
      HTTPValue dummy_value;
      SimpleMetaData remember_not_cached;

      // We need to set the header status code to 'OK' to satisfy
      // HTTPCache::IsCurrentlyValid.  We rely on the detection of the
      // header X-Instaweb-Disable-cache to avoid letting this escape into
      // the wild.  We may want to revisit if this proves problematic.
      remember_not_cached.SetStatusAndReason(HttpStatus::kOK);
      remember_not_cached.SetDate(http_cache_->timer()->NowMs());
      remember_not_cached.Add("Cache-control", "public, max-age=300");
      remember_not_cached.Add(kRememberNotCached, "1");  // value doesn't matter
      dummy_value.Write("", message_handler_);
      dummy_value.SetHeaders(remember_not_cached);
      http_cache_->Put(url_.c_str(), &dummy_value, message_handler_);
    }
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

bool CacheUrlFetcher::AsyncFetch::EnableThreaded() const {
  // Our cache implementations are thread-safe, so it's OK to update
  // them asynchronously.
  return true;
}

void CacheUrlFetcher::AsyncFetch::Start(
    UrlAsyncFetcher* fetcher, const MetaData& request_headers) {
  fetcher->StreamingFetch(url_, request_headers, ResponseHeaders(),
                          &value_, message_handler_, this);
}

CacheUrlFetcher::~CacheUrlFetcher() {
}

bool CacheUrlFetcher::RememberNotCached(const MetaData& headers) {
  CharStarVector not_cached_values;
  return headers.Lookup(kRememberNotCached, &not_cached_values);
}

bool CacheUrlFetcher::StreamingFetchUrl(
    const std::string& url, const MetaData& request_headers,
    MetaData* response_headers, Writer* writer, MessageHandler* handler) {
  bool ret = false;
  HTTPValue value;
  StringPiece contents;
  ret = (http_cache_->Get(url.c_str(), &value, response_headers, handler) &&
         value.ExtractContents(&contents));
  if (ret) {
    // If we have remembered that this value is not cachable, then mutate
    // the reponse code and return false.  Note that we must use an X-code
    // in the headers, rather than the status code, so that HTTPCache will
    // not reject the item on retrieval, spoiling our ability to remember
    // fact that the item is uncacheable.
    if (RememberNotCached(*response_headers)) {
      response_headers->SetStatusAndReason(HttpStatus::kUnavailable);
      ret = false;
    } else {
      ret = writer->Write(contents, handler);
    }
  } else if (sync_fetcher_ != NULL) {
    // We need to hang onto a copy of the data so we can shove it
    // into the cache, which lacks a streaming Put.
    std::string content;
    StringWriter string_writer(&content);
    ret = sync_fetcher_->StreamingFetchUrl(
        url, request_headers, response_headers, &string_writer, handler);
    ret &= writer->Write(content, handler);
    if (ret) {
      if (force_caching_ || response_headers->IsProxyCacheable()) {
        value.Clear();
        value.SetHeaders(*response_headers);
        value.Write(content, handler);
        http_cache_->Put(url.c_str(), &value, handler);
      }
    } else {
      // TODO(jmarantz): Consider caching that this request is not fetchable
    }
  } else {
    AsyncFetch* fetch = new AsyncFetchWithHeaders(url, http_cache_, handler,
                                                  force_caching_);
    fetch->Start(async_fetcher_, request_headers);
  }
  return ret;
}

}  // namespace net_instaweb
