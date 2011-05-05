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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/simple_text_filter.h"

#include <algorithm>  // for std::max
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"

namespace net_instaweb {

SimpleTextFilter::Rewriter::~Rewriter() {
}

SimpleTextFilter::SimpleTextFilter(Rewriter* rewriter, RewriteDriver* driver)
    : RewriteFilter(driver, rewriter->id()),
      rewriter_(rewriter) {
}

SimpleTextFilter::~SimpleTextFilter() {
}

SimpleTextFilter::Context::~Context() {
}

RewriteSingleResourceFilter::RewriteResult SimpleTextFilter::Context::Rewrite(
    const Resource* input_resource, OutputResource* output_resource) {
  RewriteSingleResourceFilter::RewriteResult result =
      RewriteSingleResourceFilter::kRewriteFailed;
  GoogleString rewritten;
  ResourceManager* resource_manager = this->resource_manager();
  if (rewriter_->RewriteText(input_resource->url(),
                             input_resource->contents(), &rewritten,
                             resource_manager))  {
    MessageHandler* message_handler = resource_manager->message_handler();
    int64 origin_expire_time_ms = input_resource->CacheExpirationTimeMs();
    if (resource_manager->Write(HttpStatus::kOK, rewritten, output_resource,
                                origin_expire_time_ms, message_handler)) {
      result = RewriteSingleResourceFilter::kRewriteOk;
    }
  }
  return result;
}

void SimpleTextFilter::StartElementImpl(HtmlElement* element) {
  HtmlElement::Attribute* attr = rewriter_->FindResourceAttribute(element);
  if (attr != NULL) {
    ResourcePtr resource = CreateInputResource(attr->value());
    if (resource.get() != NULL) {
      ResourceSlotPtr slot(driver_->GetSlot(resource, element, attr));
      driver_->InitiateRewrite(new Context(slot, rewriter_, driver_));
    }
  }
}

// Manage rewriting an input resource after it has been fetched/loaded.
class SimpleTextFilter::FetchCallback : public Resource::AsyncCallback {
 public:
  FetchCallback(const RewriterPtr& rewriter,
                const ResourcePtr& input_resource,
                const OutputResourcePtr& output_resource,
                ResponseHeaders* response_headers, Writer* response_writer,
                MessageHandler* handler,
                UrlAsyncFetcher::Callback* base_callback)
      : Resource::AsyncCallback(input_resource),
        resource_manager_(input_resource->resource_manager()),
        rewriter_(rewriter),
        input_resource_(input_resource),
        output_resource_(output_resource),
        response_headers_(response_headers),
        response_writer_(response_writer),
        handler_(handler),
        base_callback_(base_callback) {}

  virtual void Done(bool success) {
    CHECK_EQ(input_resource_.get(), resource().get());
    GoogleString output;
    if (success) {
      // This checks HTTP status was 200 OK.
      success = input_resource_->ContentsValid();
    }
    if (success) {
      // Call the rewrite hook.
      success = rewriter_->RewriteText(
          input_resource_->url(), input_resource_->contents(), &output,
          resource_manager_);
    }
    if (success) {
      int64 origin_expire_time_ms =
          input_resource_->metadata()->CacheExpirationTimeMs();
      success = resource_manager_->Write(HttpStatus::kOK, output,
                                         output_resource_.get(),
                                         origin_expire_time_ms, handler_);
    }
    if (success) {
      WriteFromResource(output_resource_.get());
    } else {
      CacheRewriteFailure();
      // Rewrite failed. If we have the original, write it out instead.
      if (input_resource_->ContentsValid()) {
        WriteFromResource(input_resource_.get());
        success = true;
      } else {
        // If not, log the failure.
        GoogleString url = input_resource_.get()->url();
        handler_->Error(
            output_resource_->name().as_string().c_str(), 0,
            "Resource based on %s but cannot find the original", url.c_str());
      }
    }

    base_callback_->Done(success);
    delete this;
  }

 private:
  void WriteFromResource(Resource* resource) {
    // Copy headers and content to HTTP response.
    // TODO(sligocki): It might be worth streaming this.
    response_headers_->CopyFrom(*resource->metadata());
    response_writer_->Write(resource->contents(), handler_);
  }

  void CacheRewriteFailure() {
    // Either we couldn't rewrite this successfully or the input resource plain
    // isn't there. If so, do not try again until the input resource expires
    // or a minimal TTL has passed.
    int64 now_ms = resource_manager_->timer()->NowMs();
    int64 expire_at_ms = std::max(now_ms + ResponseHeaders::kImplicitCacheTtlMs,
                                  input_resource_->CacheExpirationTimeMs());
    CachedResult* result = output_resource_->EnsureCachedResultCreated();
    result->set_cache_version(0 /* FilterCacheFormatVersion()*/);
    if (input_resource_->ContentsValid()) {
      // TODO(jmarantz): handle & test ReuseByContentHash:
      //
      // if (ReuseByContentHash()) {
      //   cached->set_input_hash(
      //   resource_manager_->hasher()->Hash(input_resource->contents()));
      // }
    }
    resource_manager_->WriteUnoptimizable(output_resource_.get(),
                                          expire_at_ms, handler_);
  }


  ResourceManager* resource_manager_;
  SimpleTextFilter::RewriterPtr rewriter_;
  ResourcePtr input_resource_;
  OutputResourcePtr output_resource_;
  ResponseHeaders* response_headers_;
  Writer* response_writer_;
  MessageHandler* handler_;
  UrlAsyncFetcher::Callback* base_callback_;

  DISALLOW_COPY_AND_ASSIGN(FetchCallback);
};

bool SimpleTextFilter::Fetch(
    const OutputResourcePtr& output_resource,
    Writer* response_writer,
    const RequestHeaders& request_headers,
    ResponseHeaders* response_headers,
    MessageHandler* message_handler,
    UrlAsyncFetcher::Callback* base_callback) {
  bool ret = false;

  // TODO(jmarantz): this code was copied from
  // rewrite_single_resource_filter.cc but in fact we should be
  // re-using the multi-input fetching framework from RewriteContext.
  ResourcePtr input_resource = CreateInputResourceFromOutputResource(
      output_resource.get());
  if (input_resource.get() != NULL) {
    // Callback takes ownership of input_resource, and any custom escaper.
    FetchCallback* fetch_callback = new FetchCallback(
        rewriter_, input_resource, output_resource,
        response_headers, response_writer, message_handler, base_callback);
    driver_->ReadAsync(fetch_callback, message_handler);
    ret = true;
  } else {
    GoogleString url;
    output_resource->name().CopyToString(&url);
    message_handler->Error(url.c_str(), 0, "Unable to decode resource string");
  }
  return ret;
}


}  // namespace net_instaweb
