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

#include <algorithm>

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/url_escaper.h"

namespace {

// We encode the input timestamp under this header
const char kInputTimestampKey[] = "RewriteSingleResourceFilter_InputTimestamp";

}  // namespace

namespace net_instaweb {

RewriteSingleResourceFilter::~RewriteSingleResourceFilter() {}

// Manage rewriting an input resource after it has been fetched/loaded.
class RewriteSingleResourceFilter::FetchCallback
    : public Resource::AsyncCallback {
 public:
  FetchCallback(RewriteSingleResourceFilter* filter,
                Resource* input_resource, OutputResource* output_resource,
                ResponseHeaders* response_headers, Writer* response_writer,
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
      WriteFromResource(output_resource_);
    } else {
      // Rewrite failed. If we have the original, write it out instead.
      if (input_resource_->ContentsValid()) {
        WriteFromResource(input_resource_.get());
        success = true;
      } else {
        // If not, log the failure.
        std::string url = input_resource_.get()->url();
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

  RewriteSingleResourceFilter* filter_;
  scoped_ptr<Resource> input_resource_;
  OutputResource* output_resource_;
  ResponseHeaders* response_headers_;
  Writer* response_writer_;
  MessageHandler* handler_;
  UrlAsyncFetcher::Callback* base_callback_;

  DISALLOW_COPY_AND_ASSIGN(FetchCallback);
};

bool RewriteSingleResourceFilter::Fetch(
    OutputResource* output_resource,
    Writer* response_writer,
    const RequestHeaders& request_headers,
    ResponseHeaders* response_headers,
    MessageHandler* message_handler,
    UrlAsyncFetcher::Callback* base_callback) {
  bool ret = false;
  Resource* input_resource =
      resource_manager_->CreateInputResourceFromOutputResource(
          resource_manager_->url_escaper(), output_resource,
          driver_->options(), message_handler);
  if (input_resource != NULL) {
    // Callback takes ownership of input_resource.
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

OutputResource::CachedResult* RewriteSingleResourceFilter::RewriteWithCaching(
    const StringPiece& in_url, UrlSegmentEncoder* encoder) {

  scoped_ptr<Resource> input_resource(CreateInputResource(in_url));
  if (input_resource.get() == NULL) {
    return NULL;
  }

  return RewriteResourceWithCaching(input_resource.get(), encoder);
}

OutputResource::CachedResult*
RewriteSingleResourceFilter::RewriteResourceWithCaching(
    Resource* input_resource, UrlSegmentEncoder* encoder) {
  MessageHandler* handler = html_parse_->message_handler();

  scoped_ptr<OutputResource> output_resource(
      resource_manager_->CreateOutputResourceFromResource(
          filter_prefix_, NULL, encoder, input_resource,
          driver_->options(), handler));
  if (output_resource.get() == NULL) {
    return NULL;
  }

  // See if we already have the result.
  if (output_resource->cached_result() != NULL) {
    OutputResource::CachedResult* cached =
        output_resource->ReleaseCachedResult();

    // We may need to freshen here.. Note that we check the metadata we have in
    // cached result and not the actual input resource since we've not read
    // the latter and so don't have any metadata for it.
    int64 input_timestamp_ms = 0;
    std::string input_timestamp_value;
    if (cached->Remembered(kInputTimestampKey, &input_timestamp_value) &&
        StringToInt64(input_timestamp_value, &input_timestamp_ms)) {
      if (resource_manager_->IsImminentlyExpiring(
              input_timestamp_ms, cached->origin_expiration_time_ms())) {
        input_resource->Freshen(handler);
      }
    }

    return cached;
  }

  HTTPCache::FindResult input_state =
      resource_manager_->ReadIfCachedWithStatus(input_resource, handler);
  if (input_state == HTTPCache::kNotFound) {
    // The resource has not finished fetching yet; so the caller can't
    // rewrite but there is nothing for us to cache.
    // TODO(morlovich): This is inaccurate with synchronous fetchers
    // the first time we get a 404.
    return NULL;
  }

  bool ok;
  if (input_state == HTTPCache::kFound) {
    // Remember input timestamp in the cached result to know when to freshen.
    OutputResource::CachedResult* result =
        output_resource->EnsureCachedResultCreated();
    int64 time_ms = input_resource->metadata()->timestamp_ms();
    result->SetRemembered(kInputTimestampKey, Integer64ToString(time_ms));

    ok = RewriteLoadedResource(input_resource, output_resource.get());
    if (ok) {
      CHECK(output_resource->type() != NULL);
    }
  } else {
    DCHECK_EQ(HTTPCache::kRecentFetchFailedDoNotRefetch, input_state);
    ok = false;
    handler->Message(kInfo, "%s: Couldn't fetch resource %s to rewrite.",
                     base_gurl().spec().c_str(), input_resource->url().c_str());
  }

  if (!ok) {
    // Either we couldn't rewrite this successfully or the input file plain
    // isn't there. If so, do not try again until the input file expires
    // or a minimal TTL has passed.
    int64 now_ms = resource_manager_->timer()->NowMs();
    int64 expire_at = std::max(now_ms + ResponseHeaders::kImplicitCacheTtlMs,
                               input_resource->CacheExpirationTimeMs());
    resource_manager_->WriteUnoptimizable(output_resource.get(), expire_at,
                                          handler);
  }

  // Note: we want to return this even if optimization failed in case the filter
  // has stashed some useful information about the input.
  return output_resource->ReleaseCachedResult();
}

}  // namespace net_instaweb
