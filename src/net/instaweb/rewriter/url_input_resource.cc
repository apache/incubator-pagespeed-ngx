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

  void Fetch(UrlAsyncFetcher* fetcher, MessageHandler* handler) {
    // TODO(jmarantz): consider request_headers.  E.g. will we ever
    // get different resources depending on user-agent?
    const SimpleMetaData request_headers;
    message_handler_ = handler;
    fetcher->StreamingFetch(url(), request_headers, response_headers(),
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
  UrlReadIfCachedCallback(const std::string& url,
                          bool* data_available,
                          bool* callback_called,
                          Resource* resource)
      : url_(url),
        data_available_(data_available),
        callback_called_(callback_called),
        http_cache_(resource->resource_manager()->http_cache()),
        resource_(resource) {
  }

  virtual void Done(bool success) {
    AddToCache(success);
    if (resource_ != NULL) {
      success = resource_->Link(&http_value_, message_handler_);
      // TODO(jmarantz): We should never fail here -- a failure means that
      // our URL fetcher generated bogus headers.  But this fails in
      // net/instaweb/validator:validation_regtest so for now we comment out
      // the CHECK and turn it into an if.
      //
      // CHECK(success);
      if (success) {
        CHECK_EQ(response_headers_.status_code(),
                 resource_->metadata()->status_code());
      }
      *callback_called_ = true;
      *data_available_ = success;
    }
    delete this;
  }

  // If the fetcher has not called our Done() method by the time ReadIfCached
  // must return, then it will leave this callback alive, but "detach" itself
  // so that we don't try to access resources that have gone out of scope.
  // Just to be clear we'll null all those out.
  void Detach() {
    if (*callback_called_) {
      delete this;
    } else {
      resource_ = NULL;
      data_available_ = NULL;
      callback_called_ = NULL;
    }
  }

  // Indicate that it's OK for the callback to be executed on a different thread
  virtual bool EnableThreaded() const { return true; }

  virtual MetaData* response_headers() { return &response_headers_; }
  virtual HTTPValue* http_value() { return &http_value_; }
  virtual std::string url() const { return url_; }
  virtual HTTPCache* http_cache() { return http_cache_; }

 private:
  std::string url_;
  bool* data_available_;
  bool* callback_called_;
  HTTPCache* http_cache_;
  HTTPValue http_value_;
  SimpleMetaData response_headers_;
  Resource* resource_;
};

bool UrlInputResource::ReadIfCached(MessageHandler* handler) {
  // TODO(jmarantz): This code is more complicated than it needs
  // to be.  In a live server, ReadIfCached will generally return
  // false, as the asynchronous fetch will not complete before
  // the if-statement that follows.  However, in testing and
  // file-rewriting scenarios where we are the underlying fetch
  // is reading from the file system and blocking until completion,
  // then this code will be needed.  We should simplify the code
  // and change the file-rewriting infrastructure to accomodate that.
  //
  // Ones simple way to do this is to change UrlAsyncFetcher::StreamingFetch
  // to return a bool indicating whether it short-circuited the async
  // fetch and called the callback directly.  This is much simpler to
  // use and the existing clients of async fetchers can ignore this
  // if they like.
  //
  // In the meantime we definitely have a race right now in Apache
  // because the callback will be delivered in another thread.

  // Be very careful -- we are issuing an async fetch, but we only
  // care about the result if it's cached.  If it's not cached, then
  // our callback will still get called, but the stack-variable which
  // we are giving it to write the result will be out of scope.  So if
  // the callback was not immediately called then we "detach" the
  // callback from the stack variables, to avoid corrupting memory.
  meta_data_.Clear();
  value_.Clear();
  bool data_available = false;
  bool callback_called = false;
  UrlReadIfCachedCallback* cb = new UrlReadIfCachedCallback(
      url_, &data_available, &callback_called, this);
  cb->Fetch(resource_manager_->url_async_fetcher(), handler);
  if (!callback_called) {
    // If the data is not cached, then we have queued an async fetch.   Tell
    // the fetch callback *not* to write the status code into our local
    // variable, since we are going to return now.
    //
    // Note, there is no real concurrency or async behavior at this time --
    // the async callbacks must be called by some kind of event loop and
    // will not interrupt execution.
    cb->Detach();
  }
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
