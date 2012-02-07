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

// Author: nikhilmadan@google.com (Nikhil Madan)

#include "net/instaweb/rewriter/public/ajax_rewrite_context.h"

#include <algorithm>

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class MessageHandler;

AjaxRewriteResourceSlot::AjaxRewriteResourceSlot(const ResourcePtr& resource)
    : ResourceSlot(resource) {}

AjaxRewriteResourceSlot::~AjaxRewriteResourceSlot() {}

void AjaxRewriteResourceSlot::Render() {
  // Do nothing.
}

RecordingFetch::RecordingFetch(AsyncFetch* async_fetch,
                               const ResourcePtr& resource,
                               AjaxRewriteContext* context,
                               MessageHandler* handler)
    : SharedAsyncFetch(async_fetch),
      handler_(handler),
      resource_(resource),
      context_(context),
      can_ajax_rewrite_(false) {}

RecordingFetch::~RecordingFetch() {}

void RecordingFetch::HandleHeadersComplete() {
  can_ajax_rewrite_ = CanAjaxRewrite();
  if (can_ajax_rewrite_) {
    cache_value_.SetHeaders(response_headers());
  } else {
    // It's not worth trying to rewrite any more. This cleans up the context
    // and frees the driver. Leaving this context around causes problems in
    // the html flow in particular.
    context_->driver_->FetchComplete();
  }
  base_fetch()->HeadersComplete();
}

bool RecordingFetch::HandleWrite(const StringPiece& content,
                                 MessageHandler* handler) {
  bool result = base_fetch()->Write(content, handler);
  if (can_ajax_rewrite_) {
    result &= cache_value_.Write(content, handler);
  }
  return result;
}

bool RecordingFetch::HandleFlush(MessageHandler* handler) {
  return base_fetch()->Flush(handler);
}

void RecordingFetch::HandleDone(bool success) {
  base_fetch()->Done(success);

  if (can_ajax_rewrite_) {
    resource_->Link(&cache_value_, handler_);
    context_->DetachFetch();
    context_->StartFetchReconstructionParent();
    context_->driver_->FetchComplete();
  }
  delete this;
}

bool RecordingFetch::CanAjaxRewrite() {
  const ContentType* type = response_headers()->DetermineContentType();
  response_headers()->ComputeCaching();
  if (type == NULL) {
    return false;
  }
  if (type->type() == ContentType::kCss ||
      type->type() == ContentType::kJavascript ||
      type->IsImage()) {
    if (!context_->driver_->resource_manager()->http_cache()->IsAlreadyExpired(
        *response_headers())) {
      return true;
    }
  }
  return false;
}

AjaxRewriteContext::AjaxRewriteContext(RewriteDriver* driver,
                                       const GoogleString& url)
    : SingleRewriteContext(driver, NULL, NULL),
      driver_(driver),
      url_(url),
      is_rewritten_(true),
      etag_prefix_(StrCat(HTTPCache::kEtagPrefix, id(), "-")) {
  set_notify_driver_on_fetch_done(true);
}

AjaxRewriteContext::~AjaxRewriteContext() {}

void AjaxRewriteContext::Harvest() {
  if (num_nested() == 1) {
    RewriteContext* nested_context = nested(0);
    if (nested_context->num_slots() == 1) {
      ResourcePtr nested_resource = nested_context->slot(0)->resource();
      if (nested_context->slot(0)->was_optimized() &&
          num_output_partitions() == 1) {
        CachedResult* partition = output_partition(0);
        LOG(INFO) << "Ajax rewrite succeeded for " << url_
                  << " and the rewritten resource is "
                  << nested_resource->url();
        partition->set_url(nested_resource->url());
        partition->set_optimizable(true);
        RewriteDone(kRewriteOk, 0);
        return;
      }
    }
  }
  LOG(INFO) << "Ajax rewrite failed for " << url_;
  RewriteDone(kRewriteFailed, 0);
}

void AjaxRewriteContext::FetchTryFallback(const GoogleString& url,
                                          const StringPiece& hash) {
  const char* request_etag = async_fetch()->request_headers()->Lookup1(
      HttpAttributes::kIfNoneMatch);
  if (request_etag != NULL && !hash.empty() &&
      StringEqualConcat(request_etag, etag_prefix_, hash)) {
    // Serve out a 304.
    async_fetch()->response_headers()->Clear();
    async_fetch()->response_headers()->SetStatusAndReason(
        HttpStatus::kNotModified);
    async_fetch()->Done(true);
    driver_->FetchComplete();
  } else {
    if (url == url_) {
      // If the fallback url is the same as the original url, no rewriting is
      // happening.
      is_rewritten_ = false;
      // TODO(nikhilmadan): RewriteContext::FetchTryFallback is going to look up
      // the cache. The fetcher may also do so. Should we just call
      // StartFetchReconstruction() here instead?
    } else {
      // Save the hash of the resource.
      rewritten_hash_ = hash.as_string();
    }
    RewriteContext::FetchTryFallback(url, hash);
  }
}

void AjaxRewriteContext::FixFetchFallbackHeaders(ResponseHeaders* headers) {
  if (is_rewritten_) {
    if (!rewritten_hash_.empty()) {
      headers->Replace(HttpAttributes::kEtag,
                       StrCat(etag_prefix_, rewritten_hash_));
    }

    headers->ComputeCaching();
    int64 expire_at_ms = kint64max;
    int64 date_ms = kint64max;
    for (int j = 0, m = partitions()->other_dependency_size(); j < m; ++j) {
      InputInfo dependency = partitions()->other_dependency(j);
      if (dependency.has_expiration_time_ms() && dependency.has_date_ms()) {
        date_ms = std::min(date_ms, dependency.date_ms());
        expire_at_ms = std::min(expire_at_ms, dependency.expiration_time_ms());
      }
    }
    int64 now_ms = Manager()->timer()->NowMs();
    if (expire_at_ms == kint64max) {
      // If expire_at_ms is not set, set the cache ttl to kImplicitCacheTtlMs.
      expire_at_ms = now_ms + ResponseHeaders::kImplicitCacheTtlMs;
    } else if (stale_rewrite()) {
      // If we are serving a stale rewrite, set the cache ttl to the minimum of
      // kImplicitCacheTtlMs and the original ttl.
      expire_at_ms = now_ms + std::min(ResponseHeaders::kImplicitCacheTtlMs,
                                       expire_at_ms - date_ms);
    }
    headers->SetDateAndCaching(now_ms, expire_at_ms - now_ms);
  }
}

void AjaxRewriteContext::FetchCallbackDone(bool success) {
  if (is_rewritten_ && num_output_partitions() == 1) {
    // Ajax rewrites always apply on single rewrites.
    // Freshen the resource if possible. Note that since is_rewritten_ is true,
    // we got a metadata cache hit and a hit on the rewritten resource in cache.
    Freshen(*output_partition(0));
  }
  RewriteContext::FetchCallbackDone(success);
}

RewriteFilter* AjaxRewriteContext::GetRewriteFilter(
    const ContentType& type) {
  const RewriteOptions* options = driver_->options();
  if (type.type() == ContentType::kCss &&
      options->Enabled(RewriteOptions::kRewriteCss)) {
    return driver_->FindFilter(RewriteOptions::kCssFilterId);
  }
  if (type.type() == ContentType::kJavascript &&
      options->Enabled(RewriteOptions::kRewriteJavascript)) {
    return driver_->FindFilter(RewriteOptions::kJavascriptMinId);
  }
  if (type.IsImage() && options->Enabled(RewriteOptions::kRecompressImages) &&
      !driver_->ShouldNotRewriteImages()) {
    // TODO(nikhilmadan): This converts one image format to another. We
    // shouldn't do inter-conversion since we can't change the file extension.
    return driver_->FindFilter(RewriteOptions::kImageCompressionId);
  }
  return NULL;
}

void AjaxRewriteContext::RewriteSingle(const ResourcePtr& input,
                                       const OutputResourcePtr& output) {
  input->DetermineContentType();
  if (input->IsValidAndCacheable() && input->type() != NULL) {
    const ContentType* type = input->type();
    RewriteFilter* filter = GetRewriteFilter(*type);
    if (filter != NULL) {
      ResourceSlotPtr ajax_slot(
          new AjaxRewriteResourceSlot(slot(0)->resource()));
      RewriteContext* context = filter->MakeNestedRewriteContext(
          this, ajax_slot);
      if (context != NULL) {
        AddNestedContext(context);
        if (!is_rewritten_ && !rewritten_hash_.empty()) {
          // The ajax metadata was found but the rewritten resource is not.
          // Hence, make the nested rewrite skip the metadata and force a
          // rewrite.
          context->set_force_rewrite(true);
        }
        StartNestedTasks();
        return;
      } else {
        LOG(ERROR) << "Filter (" << filter->id() << ") does not support "
                   << "nested contexts.";
        ajax_slot.clear();
      }
    }
  }
  // Give up on the rewrite.
  RewriteDone(kRewriteFailed, 0);
  // TODO(nikhilmadan): If the resource is not cacheable, cache this in the
  // metadata so that the fetcher can skip reading from the cache.
}

bool AjaxRewriteContext::DecodeFetchUrls(
    const OutputResourcePtr& output_resource,
    MessageHandler* message_handler,
    GoogleUrlStarVector* url_vector) {
  GoogleUrl* url = new GoogleUrl(url_);
  url_vector->push_back(url);
  return true;
}

void AjaxRewriteContext::StartFetchReconstruction() {
  // The ajax metdata or the rewritten resource was not found in cache. Fetch
  // the original resource and trigger an asynchronous rewrite.
  if (num_slots() == 1) {
    ResourcePtr resource(slot(0)->resource());
    // If we get here, the resource must not have been rewritten.
    is_rewritten_ = false;
    RecordingFetch* fetch = new RecordingFetch(
        async_fetch(), resource, this, fetch_message_handler());
    driver_->async_fetcher()->Fetch(url_, fetch_message_handler(), fetch);
  } else {
    LOG(ERROR) << "Expected one resource slot, but found " << num_slots()
               << ".";
    delete this;
  }
}

void AjaxRewriteContext::StartFetchReconstructionParent() {
  RewriteContext::StartFetchReconstruction();
}

GoogleString AjaxRewriteContext::CacheKeySuffix() const {
  // Include driver_->ShouldNotRewriteImages() in the cache key to
  // prevent image rewrites when bot detection is enabled.
  return driver_->ShouldNotRewriteImages() ? "0" : "1";
}

}  // namespace net_instaweb
