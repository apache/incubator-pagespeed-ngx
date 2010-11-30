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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/url_escaper.h"

namespace net_instaweb {

RewriteSingleResourceFilter::~RewriteSingleResourceFilter() {}

// Manage rewriting an input resource after it has been fetched/loaded.
class RewriteSingleResourceFilter::FetchCallback
    : public Resource::AsyncCallback {
 public:
  FetchCallback(RewriteSingleResourceFilter* filter,
                Resource* input_resource, OutputResource* output_resource,
                MetaData* response_headers, Writer* response_writer,
                MessageHandler* handler,
                UrlAsyncFetcher::Callback* base_callback)
      : filter_(filter),
        input_resource_(input_resource),
        output_resource_(output_resource),
        response_headers_(response_headers),
        response_writer_(response_writer),
        handler_(handler),
        base_callback_(base_callback) {}

  virtual void Done(bool success, Resource* resource) {
    CHECK_EQ(input_resource_.get(), resource);
    if (success) {
      // This checks HTTP status was 200 OK.
      success = input_resource_->ContentsValid();
    }
    if (success) {
      // Call the rewrite hook.
      success = filter_->RewriteLoadedResource(input_resource_.get(),
                                               output_resource_);
    }
    if (success) {
      // Copy headers and content to HTTP response.
      // TODO(sligocki): It might be worth streaming this.
      response_headers_->CopyFrom(*output_resource_->metadata());
      response_writer_->Write(output_resource_->contents(), handler_);
    }
    base_callback_->Done(success);
    delete this;
  }

 private:
  RewriteSingleResourceFilter* filter_;
  scoped_ptr<Resource> input_resource_;
  OutputResource* output_resource_;
  MetaData* response_headers_;
  Writer* response_writer_;
  MessageHandler* handler_;
  UrlAsyncFetcher::Callback* base_callback_;

  DISALLOW_COPY_AND_ASSIGN(FetchCallback);
};

bool RewriteSingleResourceFilter::Fetch(
    OutputResource* output_resource,
    Writer* response_writer,
    const MetaData& request_headers,
    MetaData* response_headers,
    MessageHandler* message_handler,
    UrlAsyncFetcher::Callback* base_callback) {
  bool ret = false;
  Resource* input_resource =
      resource_manager_->CreateInputResourceFromOutputResource(
          resource_manager_->url_escaper(), output_resource,
          driver_->options(), message_handler);
  if (input_resource != NULL) {
    // Callback takes ownership of input_resoruce.
    FetchCallback* fetch_callback = new FetchCallback(
        this, input_resource, output_resource,
        response_headers, response_writer, message_handler, base_callback);
    resource_manager_->ReadAsync(input_resource, fetch_callback,
                                 message_handler);
    ret = true;
  } else {
    std::string url;
    output_resource->name().CopyToString(&url);
    message_handler->Error(url.c_str(), 0, "Unable to decode resource string");
  }
  return ret;
}

}  // namespace net_instaweb
