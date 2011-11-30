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
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"

namespace net_instaweb {

class MessageHandler;

AjaxRewriteResourceSlot::~AjaxRewriteResourceSlot() {
}

void AjaxRewriteResourceSlot::Render() {
  // Do nothing.
}

AjaxRewriteContext::~AjaxRewriteContext() {
}

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
        RewriteDone(RewriteSingleResourceFilter::kRewriteOk, 0);
        return;
      }
    }
  }
  LOG(INFO) << "Ajax rewrite failed for " << url_;
  RewriteDone(RewriteSingleResourceFilter::kRewriteFailed, 0);
}

void AjaxRewriteContext::FetchTryFallback(const GoogleString& url,
                                          const StringPiece& hash) {
  const char* request_etag = request_headers_.Lookup1(
      HttpAttributes::kIfNoneMatch);
  if (request_etag != NULL && !hash.empty() &&
      StringEqualConcat(request_etag, etag_prefix_, hash)) {
    // Serve out a 304.
    ResponseHeaders* response_headers = fetch_response_headers();
    response_headers->Clear();
    response_headers->SetStatusAndReason(HttpStatus::kNotModified);
    fetch_callback()->Done(true);
    driver_->FetchComplete();
  } else {
    // Save the hash of the resource.
    rewritten_hash_ = hash.as_string();
    RewriteContext::FetchTryFallback(url, hash);
  }
}

void AjaxRewriteContext::FixFetchFallbackHeaders(ResponseHeaders* headers) {
  if (is_rewritten_ && !rewritten_hash_.empty()) {
      headers->Replace(HttpAttributes::kEtag,
                       StrCat(etag_prefix_, rewritten_hash_));
  }

  headers->ComputeCaching();
  int64 expire_at_ms = kint64max;
  for (int j = 0, m = partitions()->other_dependency_size(); j < m; ++j) {
    InputInfo dependency = partitions()->other_dependency(j);
    if (dependency.has_expiration_time_ms()) {
      expire_at_ms = std::min(expire_at_ms, dependency.expiration_time_ms());
    }
  }
  int64 cache_ttl_ms = expire_at_ms - headers->date_ms();
  if (expire_at_ms == kint64max) {
    // If expire_at_ms is not set, set the cache ttl to kImplicitCacheTtlMs.
    cache_ttl_ms = ResponseHeaders::kImplicitCacheTtlMs;
  }
  headers->SetCacheControlMaxAge(cache_ttl_ms);
}

// Records the fetch into the provided resource and passes through events to the
// underlying writer, response headers and callback.
class AjaxRewriteContext::RecordingFetch : public AsyncFetch {
 public:
  RecordingFetch(Writer* writer,
                 ResponseHeaders* response_headers,
                 MessageHandler* handler,
                 UrlAsyncFetcher::Callback* callback,
                 ResourcePtr resource,
                 AjaxRewriteContext* context)
      : writer_(writer),
        response_headers_(response_headers),
        handler_(handler),
        callback_(callback),
        resource_(resource),
        context_(context),
        can_ajax_rewrite_(false) { }

  virtual ~RecordingFetch() {}

  virtual void HeadersComplete() {
    can_ajax_rewrite_ = CanAjaxRewrite();
    if (can_ajax_rewrite_) {
      cache_value_.SetHeaders(response_headers_);
    } else {
      // It's not worth trying to rewrite any more. This cleans up the context
      // and frees the driver. Leaving this context around causes problems in
      // the html flow in particular.
      context_->driver_->FetchComplete();
    }
  }

  virtual bool Write(const StringPiece& content, MessageHandler* handler) {
    bool ret = true;
    ret &= writer_->Write(content, handler);
    if (can_ajax_rewrite_) {
      ret &= cache_value_.Write(content, handler);
    }
    return ret;
  }

  virtual bool Flush(MessageHandler* handler) {
    return writer_->Flush(handler);
  }

  virtual void Done(bool success) {
    callback_->Done(success);

    if (can_ajax_rewrite_) {
      resource_->Link(&cache_value_, handler_);
      context_->DetachFetch();
      context_->StartFetchReconstructionParent();
      context_->driver_->FetchComplete();
    }
    delete this;
  }

 private:
  bool CanAjaxRewrite() {
    const ContentType* type = response_headers_->DetermineContentType();
    response_headers_->ComputeCaching();
    return (type != NULL && (type->type() ==  ContentType::kCss ||
            type->type() == ContentType::kJavascript ||
            type->type() == ContentType::kPng ||
            type->type() == ContentType::kJpeg) &&
            !context_->driver_->resource_manager()->http_cache()->
                IsAlreadyExpired(*response_headers_));
  }

  Writer* writer_;
  ResponseHeaders* response_headers_;
  MessageHandler* handler_;
  UrlAsyncFetcher::Callback* callback_;
  ResourcePtr resource_;
  AjaxRewriteContext* context_;
  bool can_ajax_rewrite_;

  HTTPValue cache_value_;

  DISALLOW_COPY_AND_ASSIGN(RecordingFetch);
};

void AjaxRewriteContext::RewriteSingle(const ResourcePtr& input,
                                       const OutputResourcePtr& output) {
  RewriteFilter* filter = NULL;
  const RewriteOptions* options = driver_->options();
  input->DetermineContentType();
  if (input->IsValidAndCacheable() && input->type() != NULL) {
    ContentType::Type type = input->type()->type();
    if (type == ContentType::kCss &&
        options->Enabled(RewriteOptions::kRewriteCss)) {
      filter = driver_->FindFilter(RewriteOptions::kCssFilterId);
    } else if (type == ContentType::kJavascript &&
               options->Enabled(RewriteOptions::kRewriteJavascript)) {
      filter = driver_->FindFilter(RewriteOptions::kJavascriptMinId);
    } else if ((type == ContentType::kPng || type == ContentType::kJpeg) &&
               options->Enabled(RewriteOptions::kRecompressImages) &&
               !driver_->ShouldNotRewriteImages()) {
      // TODO(nikhilmadan): This converts one image format to another. We
      // shouldn't do inter-conversion since we can't change the file extension.
      filter = driver_->FindFilter(RewriteOptions::kImageCompressionId);
    }
    if (filter != NULL) {
      ResourceSlotPtr ajax_slot(
          new AjaxRewriteResourceSlot(slot(0)->resource()));
      RewriteContext* context = filter->MakeNestedRewriteContext(
          this, ajax_slot);
      DCHECK(context != NULL);
      if (context != NULL) {
        AddNestedContext(context);
        StartNestedTasks();
        return;
      }
    }
  }
  // Give up on the rewrite.
  RewriteDone(RewriteSingleResourceFilter::kRewriteFailed, 0);
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
  DCHECK_EQ(1, num_slots());
  ResourcePtr resource(slot(0)->resource());
  // If we get here, the resource must not have been rewritten.
  is_rewritten_ = false;
  RecordingFetch* fetch = new RecordingFetch(
      fetch_writer(), fetch_response_headers(), fetch_message_handler(),
      fetch_callback(), resource, this);
  driver_->async_fetcher()->Fetch(url_, request_headers_,
                                  fetch_response_headers(),
                                  fetch_message_handler(), fetch);
}

void AjaxRewriteContext::StartFetchReconstructionParent() {
  RewriteContext::StartFetchReconstruction();
}

GoogleString AjaxRewriteContext::CacheKey() const {
  // Include driver_->ShouldNotRewriteImages() in the cache key to prevent image
  // rewrites when bot detection is enabled.
  return StrCat(RewriteContext::CacheKey(), ":",
                driver_->ShouldNotRewriteImages() ? "0" : "1");
}

}  // namespace net_instaweb
