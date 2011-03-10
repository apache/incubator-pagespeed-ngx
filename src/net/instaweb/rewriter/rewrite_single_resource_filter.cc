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
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/url_escaper.h"

namespace net_instaweb {

// ... and the input timestamp under this key.
const char RewriteSingleResourceFilter::kInputTimestampKey[] =
    "RewriteSingleResourceFilter_InputTimestamp";

RewriteSingleResourceFilter::~RewriteSingleResourceFilter() {}

// Manage rewriting an input resource after it has been fetched/loaded.
class RewriteSingleResourceFilter::FetchCallback
    : public Resource::AsyncCallback {
 public:
  FetchCallback(RewriteSingleResourceFilter* filter,
                UrlSegmentEncoder* custom_url_encoder,
                Resource* input_resource, OutputResource* output_resource,
                ResponseHeaders* response_headers, Writer* response_writer,
                MessageHandler* handler,
                UrlAsyncFetcher::Callback* base_callback)
      : filter_(filter),
        custom_url_encoder_(custom_url_encoder),
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
      RewriteResult rewrite_result = filter_->RewriteLoadedResourceAndCacheIfOk(
          input_resource_.get(), output_resource_,
          filter_->EncoderToUse(custom_url_encoder_.get()));
      success = (rewrite_result == kRewriteOk);
    }

    if (success) {
      WriteFromResource(output_resource_);
    } else {
      filter_->CacheRewriteFailure(input_resource_.get(), output_resource_,
                                   handler_);
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
  scoped_ptr<UrlSegmentEncoder> custom_url_encoder_;
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
  scoped_ptr<UrlSegmentEncoder> custom_url_encoder(CreateCustomUrlEncoder());

  Resource* input_resource = driver_->CreateInputResourceFromOutputResource(
      EncoderToUse(custom_url_encoder.get()), output_resource);
  if (input_resource != NULL) {
    // Callback takes ownership of input_resource, and any custom escaper.
    FetchCallback* fetch_callback = new FetchCallback(
        this, custom_url_encoder.release(), input_resource, output_resource,
        response_headers, response_writer, message_handler, base_callback);
    driver_->ReadAsync(input_resource, fetch_callback, message_handler);
    ret = true;
  } else {
    std::string url;
    output_resource->name().CopyToString(&url);
    message_handler->Error(url.c_str(), 0, "Unable to decode resource string");
  }
  return ret;
}

CachedResult* RewriteSingleResourceFilter::RewriteWithCaching(
    const StringPiece& in_url, UrlSegmentEncoder* encoder) {

  scoped_ptr<Resource> input_resource(CreateInputResource(in_url));
  if (input_resource.get() == NULL) {
    return NULL;
  }

  return RewriteResourceWithCaching(input_resource.get(), encoder);
}

int RewriteSingleResourceFilter::FilterCacheFormatVersion() const {
  return 0;
}

bool RewriteSingleResourceFilter::ReuseByContentHash() const {
  return false;
}

RewriteSingleResourceFilter::RewriteResult
RewriteSingleResourceFilter::RewriteLoadedResourceAndCacheIfOk(
    const Resource* input_resource, OutputResource* output_resource,
    UrlSegmentEncoder* encoder) {
  CachedResult* result = output_resource->EnsureCachedResultCreated();
  result->set_input_timestamp_ms(input_resource->metadata()->timestamp_ms());
  UpdateCacheFormat(output_resource);
  UpdateInputHash(input_resource, result);
  RewriteResult res = RewriteLoadedResource(input_resource, output_resource,
                                            encoder);
  if (res == kRewriteOk) {
    CHECK(output_resource->type() != NULL);
  }
  return res;
}

void RewriteSingleResourceFilter::CacheRewriteFailure(
    const Resource* input_resource, OutputResource* output_resource,
    MessageHandler* handler) {
  // Either we couldn't rewrite this successfully or the input resource plain
  // isn't there. If so, do not try again until the input resource expires
  // or a minimal TTL has passed.
  int64 now_ms = resource_manager_->timer()->NowMs();
  int64 expire_at_ms = std::max(now_ms + ResponseHeaders::kImplicitCacheTtlMs,
                                input_resource->CacheExpirationTimeMs());
  UpdateCacheFormat(output_resource);
  if (input_resource->ContentsValid()) {
    UpdateInputHash(input_resource,
                    output_resource->EnsureCachedResultCreated());
  }
  resource_manager_->WriteUnoptimizable(output_resource, expire_at_ms, handler);
}

CachedResult* RewriteSingleResourceFilter::RewriteExternalResource(
    Resource* input_resource) {
  scoped_ptr<UrlSegmentEncoder> custom_encoder(CreateCustomUrlEncoder());
  return RewriteResourceWithCaching(input_resource,
                                    EncoderToUse(custom_encoder.get()));
}

CachedResult* RewriteSingleResourceFilter::RewriteResourceWithCaching(
    Resource* input_resource, UrlSegmentEncoder* encoder) {
  MessageHandler* handler = driver_->message_handler();

  scoped_ptr<OutputResource> output_resource(
      driver_->CreateOutputResourceFromResource(
          filter_prefix_, NULL, encoder, input_resource));
  if (output_resource.get() == NULL) {
    return NULL;
  }

  // See if we already have the result, and if it's valid.
  CachedResult* result = output_resource->EnsureCachedResultCreated();
  if (!IsValidCacheFormat(result)) {
    delete output_resource->ReleaseCachedResult();
    result = NULL;
  }

  // If we're checking the input hash we may be keeping the cached output
  // past the TTL of the input; in which case we can't just return it ---
  // we need to check whether the input has changed or not, which we do
  // further below.
  bool check_input_hash = ReuseByContentHash();
  if (result != NULL && !(check_input_hash && IsOriginExpired(result))) {
    return ReleaseCachedAfterAnyFreshening(input_resource,
                                           output_resource.get());
  }

  HTTPCache::FindResult input_state =
      driver_->ReadIfCachedWithStatus(input_resource);
  if (input_state == HTTPCache::kNotFound) {
    // The resource has not finished fetching yet; so the caller can't
    // rewrite but there is nothing for us to cache.
    // TODO(morlovich): This is inaccurate with synchronous fetchers
    // the first time we get a 404.
    return NULL;
  }

  bool ok;
  if (input_state == HTTPCache::kFound && input_resource->ContentsValid()) {
    if (check_input_hash) {
      output_resource->EnsureCachedResultCreated()->set_auto_expire(false);

      if (result != NULL) {
        std::string actual_hash =
            resource_manager_->hasher()->Hash(input_resource->contents());
        if (result->has_input_hash()
            && (actual_hash == result->input_hash())) {
          // The input has not changed, so we can return the same output.
          // We do need to update the cache entry for new input expiration
          // and load times.
          result->set_input_timestamp_ms(
              input_resource->metadata()->timestamp_ms());
          resource_manager_->CacheComputedResourceMapping(
              output_resource.get(), input_resource->CacheExpirationTimeMs(),
              handler);
          handler->Message(kInfo, "Resource %s expired but did not change.",
                           input_resource->url().c_str());

          return ReleaseCachedAfterAnyFreshening(input_resource,
                                                 output_resource.get());
        }
      }
    }

    // Do the actual rewrite -- unless some other process is doing it at the
    // same time. (The unlock will happen at Write or ~OutputResource)
    if (!output_resource->LockForCreation(resource_manager_,
                                          ResourceManager::kNeverBlock)) {
      handler->Message(kInfo, "%s: Someone else is trying to rewrite %s.",
                       base_url().spec_c_str(),
                       input_resource->url().c_str());
      return NULL;
    }

    RewriteResult res = RewriteLoadedResourceAndCacheIfOk(input_resource,
                                                          output_resource.get(),
                                                          encoder);
    if (res == kTooBusy) {
      // The system is too loaded to currently do a rewrite; in this case
      // we simply return NULL and don't write anything to the cache since
      // we plain don't know.
      return NULL;
    }

    ok = (res == kRewriteOk);
  } else {
    DCHECK_EQ(HTTPCache::kRecentFetchFailedDoNotRefetch, input_state);
    ok = false;
    handler->Message(kInfo, "%s: Couldn't fetch resource %s to rewrite.",
                     base_url().spec_c_str(), input_resource->url().c_str());
  }

  if (!ok) {
    CacheRewriteFailure(input_resource, output_resource.get(), handler);
  }

  // Note: we want to return this even if optimization failed in case the filter
  // has stashed some useful information about the input.
  return output_resource->ReleaseCachedResult();
}

// Fails for default-constructed, or cache-miss results.
bool RewriteSingleResourceFilter::IsValidCacheFormat(
    const CachedResult* cached) {
  return (cached->has_cache_version()
          && (cached->cache_version() == FilterCacheFormatVersion()));
}

void RewriteSingleResourceFilter::UpdateCacheFormat(
    OutputResource* output_resource) {
  CachedResult* result = output_resource->EnsureCachedResultCreated();
  result->set_cache_version(FilterCacheFormatVersion());
}

void RewriteSingleResourceFilter::UpdateInputHash(
    const Resource* input_resource, CachedResult* cached) {
  if (ReuseByContentHash()) {
    cached->set_input_hash(
        resource_manager_->hasher()->Hash(input_resource->contents()));
  }
}

bool RewriteSingleResourceFilter::IsOriginExpired(CachedResult* cached) const {
  int64 now_ms = resource_manager_->timer()->NowMs();
  return (now_ms > cached->origin_expiration_time_ms());
}

CachedResult* RewriteSingleResourceFilter::ReleaseCachedAfterAnyFreshening(
    Resource* input_resource, OutputResource* output_resource) {
  CachedResult* cached = output_resource->ReleaseCachedResult();

  // We may need to freshen here. Note that we check the metadata we have in
  // cached result and not the actual input resource since we've not read
  // the latter and so don't have any metadata for it.
  if (cached->has_input_timestamp_ms()) {
    if (resource_manager_->IsImminentlyExpiring(
            cached->input_timestamp_ms(),
            cached->origin_expiration_time_ms())) {
      input_resource->Freshen(driver_->message_handler());
    }
  }

  return cached;
}

UrlSegmentEncoder*
RewriteSingleResourceFilter::CreateCustomUrlEncoder() const {
  return NULL;
}

UrlSegmentEncoder* RewriteSingleResourceFilter::EncoderToUse(
    UrlSegmentEncoder* custom_encoder) {
  if (custom_encoder != NULL) {
    return custom_encoder;
  } else {
    return resource_manager_->url_escaper();
  }
}

}  // namespace net_instaweb
