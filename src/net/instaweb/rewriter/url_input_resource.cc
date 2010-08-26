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

// Author: sligocki@google.com (Shawn Ligocki)
//         jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/url_input_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/http_value.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

namespace net_instaweb {

UrlInputResource::~UrlInputResource() {
}

// Shared fetch callback, used by both ReadAsync and ReadIfCached
class UrlResourceFetchCallback : public UrlAsyncFetcher::Callback {
 public:
  UrlResourceFetchCallback() : message_handler_(NULL) { }
  virtual ~UrlResourceFetchCallback() {}

  void AddToCache(bool success) {
    if (success) {
      HTTPValue* value = http_value();
      value->SetHeaders(*response_headers());
      http_cache()->Put(url(), value, NULL);
    } else {
      // TODO(jmarantz): consider caching our failure to fetch this resource
    }
  }

  bool Fetch(UrlAsyncFetcher* fetcher, MessageHandler* handler) {
    // TODO(jmarantz): consider request_headers.  E.g. will we ever
    // get different resources depending on user-agent?
    const SimpleMetaData request_headers;
    message_handler_ = handler;
    return fetcher->StreamingFetch(url(), request_headers, response_headers(),
                                   http_value(), handler, this);
  }

  // The two derived classes differ in how they provide the
  // fields below.  The Async callback gets them from the resource,
  // which must be live at the time it is called.  The ReadIfCached
  // cannot rely on the resource still being alive when the callback
  // is called, so it must keep them locally in the class.
  virtual MetaData* response_headers() = 0;
  virtual HTTPValue* http_value() = 0;
  virtual std::string url() const = 0;
  virtual HTTPCache* http_cache() = 0;

 protected:
  MessageHandler* message_handler_;
};

class UrlReadIfCachedCallback : public UrlResourceFetchCallback {
 public:
  UrlReadIfCachedCallback(const std::string& url, HTTPCache* http_cache)
      : url_(url),
        http_cache_(http_cache) {
  }

  virtual void Done(bool success) {
    AddToCache(success);
    delete this;
  }

  // Indicate that it's OK for the callback to be executed on a different
  // thread, as it only populates the cache, which is thread-safe.
  virtual bool EnableThreaded() const { return true; }

  virtual MetaData* response_headers() { return &response_headers_; }
  virtual HTTPValue* http_value() { return &http_value_; }
  virtual std::string url() const { return url_; }
  virtual HTTPCache* http_cache() { return http_cache_; }

 private:
  std::string url_;
  HTTPCache* http_cache_;
  HTTPValue http_value_;
  SimpleMetaData response_headers_;
};

bool UrlInputResource::ReadIfCached(MessageHandler* handler) {
  meta_data_.Clear();
  value_.Clear();

  HTTPCache* http_cache = resource_manager()->http_cache();
  UrlReadIfCachedCallback* cb = new UrlReadIfCachedCallback(url_, http_cache);

  // If the fetcher can satisfy the request instantly, then we
  // can try to populate the resource from the cache.
  bool data_available =
      (cb->Fetch(resource_manager_->url_async_fetcher(), handler) &&
       http_cache->Get(url_, &value_, &meta_data_, handler));
  return data_available;
}


class UrlReadAsyncFetchCallback : public UrlResourceFetchCallback {
 public:
  explicit UrlReadAsyncFetchCallback(Resource::AsyncCallback* callback,
                                    UrlInputResource* resource)
      : resource_(resource),
        callback_(callback) {
  }

  virtual void Done(bool success) {
    AddToCache(success);
    callback_->Done(success, resource_);
    delete this;
  }

  virtual MetaData* response_headers() { return &resource_->meta_data_; }
  virtual HTTPValue* http_value() { return &resource_->value_; }
  virtual std::string url() const { return resource_->url(); }
  virtual HTTPCache* http_cache() {
    return resource_->resource_manager()->http_cache();
  }

 private:
  UrlInputResource* resource_;
  Resource::AsyncCallback* callback_;
};

void UrlInputResource::ReadAsync(AsyncCallback* callback,
                                 MessageHandler* message_handler) {
  CHECK(callback != NULL) << "A callback must be supplied, or else it will "
      "not be possible to determine when it's safe to delete the resource.";
  if (loaded()) {
    callback->Done(true, this);
  } else {
    UrlReadAsyncFetchCallback* cb =
        new UrlReadAsyncFetchCallback(callback, this);
    cb->Fetch(resource_manager_->url_async_fetcher(), message_handler);
  }
}

}  // namespace net_instaweb
