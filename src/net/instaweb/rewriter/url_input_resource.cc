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

#include "net/instaweb/rewriter/public/url_input_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/http_value.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

namespace net_instaweb {

UrlInputResource::~UrlInputResource() {
}

class UrlReadIfCachedCallback : public Resource::AsyncCallback {
 public:
  UrlReadIfCachedCallback(bool* data_available, bool* callback_called,
                          HTTPValue* value, MessageHandler* handler)
      : data_available_(data_available),
        callback_called_(callback_called),
        value_(value) {
  }

  virtual void Done(bool success, HTTPValue* value) {
    if (callback_called_ != NULL) {
      *callback_called_ = true;
    }
    if (data_available_ != NULL) {
      *data_available_ = success;
    }
    if ((value_ != NULL) && success) {
      SharedString& contents_and_headers = value->share();
      value_->Link(&contents_and_headers, handler_);
    }
    delete this;
  }

  void detach() {
    data_available_ = NULL;
    callback_called_ = NULL;
    value_ = NULL;
    handler_ = NULL;
  }

 private:
  bool* data_available_;
  bool* callback_called_;
  HTTPValue* value_;
  MessageHandler* handler_;
};

bool UrlInputResource::ReadIfCached(MessageHandler* handler) {
  // Be very careful -- we are issuing an async fetch, but we only
  // care about the result if it's cached.  If it's not cached, then
  // our callback will still get called, but the stack-variable which
  // we are giving it to write the result will be out of scope.  So if
  // the callback was not immediately called then we "detach" the
  // callback from the stack variables, to avoid corrupting memory.
  CHECK_EQ(0, meta_data_.major_version());
  bool data_available = false;
  bool callback_called = false;
  UrlReadIfCachedCallback* cb = new UrlReadIfCachedCallback(
      &data_available, &callback_called, &value_, handler);
  ReadAsync(cb, handler);
  if (callback_called) {
    if (data_available) {
      data_available = value_.ExtractHeaders(&meta_data_, handler);
      if (data_available) {
        DetermineContentType();
      }
    }
  } else {
    // If the data is not cached, then we have queued an async fetch.   Tell
    // the fetch callback *not* to write the status code into our local
    // variable, since we are going to return now.
    //
    // Note, there is no real concurrency or async behavior at this time --
    // the async callbacks must be called by some kind of event loop and
    // will not interrupt execution.
    cb->detach();
  }
  return data_available;
}


class UrlInputResourceCallback : public UrlAsyncFetcher::Callback {
 public:
  explicit UrlInputResourceCallback(Resource::AsyncCallback* callback)
      : callback_(callback) {
  }

  virtual void Done(bool status) {
    if (status) {
      value_.SetHeaders(response_headers_);
    }
    callback_->Done(status, &value_);
    delete this;
  }

  void Fetch(UrlAsyncFetcher* fetcher, const std::string& url,
             const MetaData& request_headers, MessageHandler* handler) {
    fetcher->StreamingFetch(url, request_headers, &response_headers_, &value_,
                            handler, this);
  }

 private:
  UrlInputResource* resource_;
  Resource::AsyncCallback* callback_;
  HTTPValue value_;
  SimpleMetaData response_headers_;
};

void UrlInputResource::ReadAsync(AsyncCallback* callback,
                                 MessageHandler* message_handler) {
  if (loaded()) {
    if (callback != NULL) {
      callback->Done(true, &value_);
    }
  } else {
    // TODO(jmarantz): consider request_headers.  E.g. will we ever
    // get different resources depending on user-agent?
    SimpleMetaData request_headers;
    UrlInputResourceCallback* cb = new UrlInputResourceCallback(callback);
    cb->Fetch(resource_manager_->url_async_fetcher(), url_, request_headers,
              message_handler);
  }
}

}  // namespace net_instaweb
