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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/http/public/cache_url_async_fetcher.h"

#include "base/logging.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

class CachePutFetch : public AsyncFetch {
 public:
  CachePutFetch(const GoogleString& url, ResponseHeaders* response_headers,
                AsyncFetch* base_fetch,
                HTTPCache* cache, MessageHandler* handler)
      : url_(url), response_headers_(response_headers),
        base_fetch_(base_fetch), cache_(cache), handler_(handler),
        cacheable_(false) {}
  virtual ~CachePutFetch() {}

  virtual void HeadersComplete() {
    response_headers_->ComputeCaching();
    // TODO(sligocki): Consider appropriate Vary semantics.
    // We need to deal with HTML and other resources differently.
    // IsProxyCacheable() does not consider Vary.
    cacheable_ = response_headers_->IsProxyCacheable();
    if (cacheable_) {
      cache_value_.SetHeaders(response_headers_);
    }

    base_fetch_->HeadersComplete();
  }

  virtual bool Write(const StringPiece& content, MessageHandler* handler) {
    bool ret = true;
    ret &= base_fetch_->Write(content, handler);
    if (cacheable_) {
      ret &= cache_value_.Write(content, handler);
    }
    return ret;
  }

  virtual bool Flush(MessageHandler* handler) {
    // Note cache_value_.Flush doesn't do anything.
    return base_fetch_->Flush(handler);
  }

  virtual void Done(bool success) {
    // Finish fetch.
    base_fetch_->Done(success);
    // Add result to cache.
    if (cacheable_) {
      cache_->Put(url_, &cache_value_, handler_);
    }
    delete this;
  }

 private:
  const GoogleString url_;
  ResponseHeaders* response_headers_;
  AsyncFetch* base_fetch_;
  HTTPCache* cache_;
  MessageHandler* handler_;

  bool cacheable_;
  HTTPValue cache_value_;

  DISALLOW_COPY_AND_ASSIGN(CachePutFetch);
};

class CacheFindCallback : public HTTPCache::Callback {
 public:
  CacheFindCallback(const GoogleString& url,
                    const RequestHeaders& request_headers,
                    ResponseHeaders* response_headers,
                    AsyncFetch* base_fetch,
                    bool ignore_recent_fetch_failed,
                    HTTPCache* cache,
                    UrlAsyncFetcher* fetcher,
                    MessageHandler* handler)
      : url_(url),
        response_headers_(response_headers), base_fetch_(base_fetch),
        cache_(cache), fetcher_(fetcher), handler_(handler),
        ignore_recent_fetch_failed_(ignore_recent_fetch_failed) {
    request_headers_.CopyFrom(request_headers);
  }
  virtual ~CacheFindCallback() {}

  virtual void Done(HTTPCache::FindResult find_result) {
    switch (find_result) {
      case HTTPCache::kFound: {
        VLOG(1) << "Found in cache: " << url_;
        http_value()->ExtractHeaders(response_headers_, handler_);

        // Respond with a 304 if the If-Modified-Since / If-None-Match values
        // are equal to those in the request.
        if (ShouldReturn304()) {
          response_headers_->Clear();
          response_headers_->SetStatusAndReason(HttpStatus::kNotModified);
          base_fetch_->HeadersComplete();
        } else {
          base_fetch_->HeadersComplete();

          StringPiece contents;
          http_value()->ExtractContents(&contents);
          // TODO(sligocki): We are writing all the content in one shot, this
          // fact might be useful to the HtmlParser if this is HTML. Perhaps
          // we should add an API for conveying that information.
          base_fetch_->Write(contents, handler_);
        }

        base_fetch_->Done(true);
        break;
      }
      // Note: currently no resources fetched through CacheUrlAsyncFetcher
      // will be marked RememberFetchFailedOrNotCacheable.
      // TODO(sligocki): Should we mark resources as such in this class?
      case HTTPCache::kRecentFetchFailedOrNotCacheable:
        VLOG(1) << "RecentFetchFailedOrNotCacheable: " << url_;
        if (!ignore_recent_fetch_failed_) {
          base_fetch_->Done(false);
          break;
        } else {
          // If we are ignoring advice of kRecentFetchFailedOrNotCacheable,
          // we will refetch the resource as we would for kNotFound.
          //
          // For example, we should do this for fetches that are being proxied.

          // fall through
        }
      case HTTPCache::kNotFound: {
        VLOG(1) << "Did not find in cache: " << url_;
        CachePutFetch* put_fetch =
            new CachePutFetch(url_, response_headers_,
                              base_fetch_, cache_, handler_);
        fetcher_->Fetch(url_, request_headers_,
                        response_headers_, handler_, put_fetch);
        break;
      }
    }

    delete this;
  }

  virtual bool IsCacheValid(const ResponseHeaders& headers) {
    return base_fetch_->IsCachedResultValid(headers);
  }
 private:
  bool ShouldReturn304() const {
    if (ConditionalHeadersMatch(HttpAttributes::kIfNoneMatch,
                                HttpAttributes::kEtag)) {
      // If the Etag matches, return a 304.
      return true;
    }
    // Otherwise, return a 304 only if there was no If-None-Match header in the
    // request and the last modified timestamp matches.
    // (from http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html)
    return request_headers_.Lookup1(HttpAttributes::kIfNoneMatch) == NULL &&
        ConditionalHeadersMatch(HttpAttributes::kIfModifiedSince,
                                HttpAttributes::kLastModified);
  }

  bool ConditionalHeadersMatch(const GoogleString& request_header,
                               const GoogleString& response_header) const {
    const char* request_header_value =
        request_headers_.Lookup1(request_header);
    const char* response_header_value =
        response_headers_->Lookup1(response_header);
    return request_header_value != NULL && response_header_value != NULL &&
        strcmp(request_header_value, response_header_value) == 0;
  }

  const GoogleString url_;
  RequestHeaders request_headers_;
  ResponseHeaders* response_headers_;
  AsyncFetch* base_fetch_;
  HTTPCache* cache_;
  UrlAsyncFetcher* fetcher_;
  MessageHandler* handler_;

  bool ignore_recent_fetch_failed_;

  DISALLOW_COPY_AND_ASSIGN(CacheFindCallback);
};

}  // namespace

CacheUrlAsyncFetcher::~CacheUrlAsyncFetcher() {
}

bool CacheUrlAsyncFetcher::Fetch(
    const GoogleString& url, const RequestHeaders& request_headers,
    ResponseHeaders* response_headers, MessageHandler* handler,
    AsyncFetch* base_fetch) {

  if (request_headers.method() != RequestHeaders::kGet) {
    // Without this if there is URL which responds both on GET
    // and POST. If GET comes first, and POST next then the cached
    // entry will be reused. POST is allowed to invalidate GET response
    // in cache, but not use the value in cache. For now just bypassing
    // cache for non GET requests.
    fetcher_->Fetch(url, request_headers, response_headers, handler,
                    base_fetch);
    return false;
  }

  CacheFindCallback* find_callback =
      new CacheFindCallback(url, request_headers, response_headers, base_fetch,
                            ignore_recent_fetch_failed_,
                            http_cache_, fetcher_, handler);
  http_cache_->Find(url, handler, find_callback);
  // Cache interface does not tell us if the request was immediately resolved,
  // so we must say that it wasn't.
  return false;
}

}  // namespace net_instaweb
