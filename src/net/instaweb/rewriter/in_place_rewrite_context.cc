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

#include "net/instaweb/rewriter/public/in_place_rewrite_context.h"

#include <algorithm>

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/cache_url_async_fetcher.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/image_url_encoder.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/writer.h"  // for Writer
#include "pagespeed/kernel/http/user_agent_matcher.h"

namespace net_instaweb {

class MessageHandler;

const char InPlaceRewriteResourceSlot::kIproSlotLocation[] = "ipro";

// Names for Statistics variables.
const char InPlaceRewriteContext::kInPlaceOversizedOptStream[] =
    "in_place_oversized_opt_stream";
const char InPlaceRewriteContext::kInPlaceUncacheableRewrites[] =
    "in_place_uncacheable_rewrites";


InPlaceRewriteResourceSlot::InPlaceRewriteResourceSlot(
    const ResourcePtr& resource)
    : ResourceSlot(resource) {}

InPlaceRewriteResourceSlot::~InPlaceRewriteResourceSlot() {}

GoogleString InPlaceRewriteResourceSlot::LocationString() {
  return kIproSlotLocation;
}

void InPlaceRewriteResourceSlot::Render() {
  // Do nothing.
}

RecordingFetch::RecordingFetch(bool proxy_mode,
                               AsyncFetch* async_fetch,
                               const ResourcePtr& resource,
                               InPlaceRewriteContext* context,
                               MessageHandler* handler)
    : SharedAsyncFetch(async_fetch),
      proxy_mode_(proxy_mode),
      handler_(handler),
      resource_(resource),
      context_(context),
      can_in_place_rewrite_(false),
      streaming_(true),
      cache_value_writer_(
          &cache_value_, context_->FindServerContext()->http_cache()) {
  Statistics* stats = context->FindServerContext()->statistics();
  in_place_oversized_opt_stream_ =
      stats->GetVariable(InPlaceRewriteContext::kInPlaceOversizedOptStream);
  in_place_uncacheable_rewrites_ =
      stats->GetVariable(InPlaceRewriteContext::kInPlaceUncacheableRewrites);
}

RecordingFetch::~RecordingFetch() {}

void RecordingFetch::HandleHeadersComplete() {
  can_in_place_rewrite_ = CanInPlaceRewrite();
  streaming_ = ShouldStream();
  if (can_in_place_rewrite_) {
    // Save the headers, and wait to finalize them in HandleDone().
    saved_headers_.reset(new ResponseHeaders(*response_headers()));
    if (streaming_) {
      SharedAsyncFetch::HandleHeadersComplete();
    }
  } else {
    FreeDriver();
    if (proxy_mode_) {
      SharedAsyncFetch::HandleHeadersComplete();
    } else {
      // If we are the origin, we do not have to pass through bytes
      // if we aren't rewriting --- the caller is expected to fall back to
      // the server's native method if FetchInPlaceResource fails.
      //
      // It turns out that passing through HTML bytes in particular can
      // lead to deadlock with MPS + memcached due to blocking property cache
      // lookups getting invoked in a call chain off cache hits, which wedges
      // the CacheBatcher thread. So, to avoid this we get out of the way
      // of things we don't rewrite here, which includes HTML.
      //
      // Note that since that can lead to the fetch we are chained to being
      // deleted, we have to detach the header objects from the parent
      // fetch, since the CacheUrlAsyncFetcher will still be trying to write
      // to us.
      streaming_ = false;
      set_request_headers(NULL);
      // If we cannot rewrite in-place, we should not serve a 200/OK.  Serve
      // kNotInCacheStatus instead to fall back to the server's native method of
      // serving the url and indicate we do want it recorded.
      if (!response_headers()->IsErrorStatus()) {
        response_headers()->set_status_code(
            CacheUrlAsyncFetcher::kNotInCacheStatus);
      }
      set_response_headers(NULL);
      set_extra_response_headers(NULL);
      SharedAsyncFetch::HandleDone(false);
    }
  }
}

void RecordingFetch::FreeDriver() {
  // This cleans up the context and frees the driver. Leaving this context
  // around causes problems in the html flow in particular.
  context_->Driver()->FetchComplete();
}

bool RecordingFetch::ShouldStream() const {
  return !(can_in_place_rewrite_ &&
           context_->Options()->in_place_wait_for_optimized());
}

bool RecordingFetch::HandleWrite(const StringPiece& content,
                                 MessageHandler* handler) {
  bool result = true;
  if (streaming_) {
    result = SharedAsyncFetch::HandleWrite(content, handler);
  }
  if (can_in_place_rewrite_) {
    if (cache_value_writer_.CanCacheContent(content)) {
      result &= cache_value_writer_.Write(content, handler);
      DCHECK(cache_value_writer_.has_buffered());
    } else {
      // Cannot in-place rewrite a resource which is too big to fit in cache.
      // TODO(jkarlin): Do we make note that the resource was too big so that
      // we don't try to cache it later? Test and fix if not.
      can_in_place_rewrite_ = false;
      if (!streaming_) {
        // We need to start streaming now so write out what we've cached so far.
        streaming_ = true;
        in_place_oversized_opt_stream_->Add(1);
        StringPiece cache_contents;
        cache_value_.ExtractContents(&cache_contents);
        set_content_length(cache_contents.size() + content.size());
        SharedAsyncFetch::HandleHeadersComplete();
        SharedAsyncFetch::HandleWrite(cache_contents, handler);
        SharedAsyncFetch::HandleWrite(content, handler);
      }
      FreeDriver();
    }
  }
  return result;
}

bool RecordingFetch::HandleFlush(MessageHandler* handler) {
  if (streaming_) {
    return SharedAsyncFetch::HandleFlush(handler);
  }
  return true;
}

void RecordingFetch::HandleDone(bool success) {
  if (success && can_in_place_rewrite_ && resource_->UseHttpCache()) {
    // Extract X-Original-Content-Length from the response headers, which may
    // have been added by the fetcher, and set it in the Resource. This will
    // be used to build the X-Original-Content-Length for rewrites.
    const char* original_content_length_hdr = extra_response_headers()->Lookup1(
        HttpAttributes::kXOriginalContentLength);
    int64 ocl;
    if (original_content_length_hdr != NULL &&
        StringToInt64(original_content_length_hdr, &ocl)) {
      saved_headers_->SetOriginalContentLength(ocl);
    }
    // Now finalize the headers.
    cache_value_writer_.SetHeaders(saved_headers_.get());
  }

  if (streaming_) {
    SharedAsyncFetch::HandleDone(success);
  }

  if (success && can_in_place_rewrite_) {
    if (resource_->UseHttpCache()) {
      // Note, if !UseHttpCache() then the value will already be populated.
      // See InPlaceRewriteContext::StartFetchReconstruction.
      resource_->Link(&cache_value_, handler_);
    } else {
      DCHECK(resource_->loaded());
    }
    if (streaming_) {
      context_->DetachFetch();
    }
    context_->StartFetchReconstructionParent();
    if (streaming_) {
      context_->Driver()->FetchComplete();
    }
  }
  delete this;
}

bool RecordingFetch::CanInPlaceRewrite() {
  // We are rewriting only 200 responses.
  if (response_headers()->status_code() != HttpStatus::kOK) {
    return false;
  }

  const ContentType* type = response_headers()->DetermineContentType();
  if (type == NULL) {
    VLOG(2) << "CanInPlaceRewrite false. Content-Type is not defined. Url: "
            << resource_->url();
    return false;
  }

  // Note that this only checks the length, not the caching headers; the
  // latter are checked in IsProxyCacheable.
  if (!cache_value_writer_.CheckCanCacheElseClear(response_headers())) {
    return false;
  }
  if (type->type() == ContentType::kCss ||
      type->IsJs() ||
      type->IsImage()) {
    RewriteDriver* driver = context_->Driver();
    HTTPCache* const cache = driver->server_context()->http_cache();
    if (response_headers()->IsProxyCacheable(
            request_headers()->GetProperties(),
            ResponseHeaders::GetVaryOption(driver->options()->respect_vary()),
            ResponseHeaders::kNoValidator) &&
        !cache->IsExpired(*response_headers())) {
      return true;
    } else if (context_->rewrite_uncacheable()) {
      in_place_uncacheable_rewrites_->Add(1);
      return true;
    }
    VLOG(2) << "CanInPlaceRewrite false, since J/I/C resource is not cacheable."
            << " Url: " << resource_->url();
  }
  return false;
}

InPlaceRewriteContext::InPlaceRewriteContext(RewriteDriver* driver,
                                       const StringPiece& url)
    : SingleRewriteContext(driver, NULL, new ResourceContext),
      url_(url.data(), url.size()),
      is_rewritten_(true),
      proxy_mode_(true) {
  set_notify_driver_on_fetch_done(true);
  const RewriteOptions* options = Options();
  set_rewrite_uncacheable(
      options->rewrite_uncacheable_resources() &&
      options->in_place_wait_for_optimized());
}

InPlaceRewriteContext::~InPlaceRewriteContext() {}

void InPlaceRewriteContext::InitStats(Statistics* statistics) {
  statistics->AddVariable(kInPlaceOversizedOptStream);
  statistics->AddVariable(kInPlaceUncacheableRewrites);
}

int64 InPlaceRewriteContext::GetRewriteDeadlineAlarmMs() const {
  if (Options()->in_place_wait_for_optimized()) {
    return Driver()->options()->in_place_rewrite_deadline_ms();
  }
  return RewriteContext::GetRewriteDeadlineAlarmMs();
}

void InPlaceRewriteContext::Harvest() {
  if (num_nested() == 1) {
    RewriteContext* const nested_context = nested(0);
    if (nested_context->num_slots() == 1 && num_output_partitions() == 1 &&
        nested_context->slot(0)->was_optimized()) {
      ResourcePtr nested_resource = nested_context->slot(0)->resource();
      CachedResult* partition = output_partition(0);
      CachedResult* nested_partition = nested_context->output_partition(0);
      VLOG(1) << "In-place rewrite succeeded for " << url_
              << " and the rewritten resource is "
              << nested_resource->url();
      partition->set_url(nested_resource->url());
      partition->set_optimizable(true);
      CHECK(nested_partition != NULL);
      // TODO(jmaessen): Does any more state need to find its way into the
      // enclosing CachedResult from the nested one?
      if (nested_partition->has_minimal_webp_support()) {
        partition->set_minimal_webp_support(
            nested_partition->minimal_webp_support());
      }
      if (partitions()->other_dependency_size() == 1) {
        // If there is only one other dependency, then the InputInfo is
        // already covered in the first partition. We're clearing this here
        // since freshens only update the partitions and not the other
        // dependencies.
        partitions()->clear_other_dependency();
      }
      if (!FetchContextDetached() &&
          Options()->in_place_wait_for_optimized()) {
        // If we're waiting for the optimized version before responding,
        // prepare the output here. Most of this is translated from
        // RewriteContext::FetchContext::FetchDone
        output_resource_->response_headers()->CopyFrom(
            *(nested_resource->response_headers()));
        Writer* writer = output_resource_->BeginWrite(
            Driver()->message_handler());
        writer->Write(nested_resource->contents(),
                      Driver()->message_handler());
        output_resource_->EndWrite(Driver()->message_handler());

        is_rewritten_ = true;
        // EndWrite updated the hash in output_resource_.
        output_resource_->full_name().hash().CopyToString(&rewritten_hash_);

        // Use the most conservative Cache-Control considering the input.
        // TODO(jkarlin): Is ApplyInputCacheControl needed here?
        ResourceVector rv(1, input_resource_);
        FindServerContext()->ApplyInputCacheControl(
            rv, output_resource_->response_headers());
      }
      RewriteDone(kRewriteOk, 0);
      return;
    }
  }
  VLOG(1) << "In-place rewrite failed for " << url_;
  RewriteDone(kRewriteFailed, 0);
}

void InPlaceRewriteContext::FetchTryFallback(const GoogleString& url,
                                          const StringPiece& hash) {
  const char* request_etag = async_fetch()->request_headers()->Lookup1(
      HttpAttributes::kIfNoneMatch);
  if (request_etag != NULL && !hash.empty() &&
      (HTTPCache::FormatEtag(StrCat(id(), "-", hash)) == request_etag)) {
    // Serve out a 304.
    async_fetch()->response_headers()->Clear();
    async_fetch()->response_headers()->SetStatusAndReason(
        HttpStatus::kNotModified);
    async_fetch()->Done(true);
    Driver()->FetchComplete();
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

void InPlaceRewriteContext::FixFetchFallbackHeaders(
    const CachedResult& cached_result, ResponseHeaders* headers) {
  if (is_rewritten_) {
    if (!rewritten_hash_.empty()) {
      headers->Replace(HttpAttributes::kEtag, HTTPCache::FormatEtag(StrCat(
                                                  id(), "-", rewritten_hash_)));
    }
    headers->set_implicit_cache_ttl_ms(Options()->implicit_cache_ttl_ms());
    headers->set_min_cache_ttl_ms(Options()->min_cache_ttl_ms());
    headers->ComputeCaching();
    int64 expire_at_ms = kint64max;
    int64 date_ms = kint64max;
    if (partitions()->other_dependency_size() > 0) {
      UpdateDateAndExpiry(partitions()->other_dependency(), &date_ms,
                          &expire_at_ms);
    } else {
      UpdateDateAndExpiry(output_partition(0)->input(), &date_ms,
                          &expire_at_ms);
    }
    int64 now_ms = FindServerContext()->timer()->NowMs();
    if (expire_at_ms == kint64max) {
      // If expire_at_ms is not set, set the cache ttl to the implicit ttl value
      // specified in the response headers.
      expire_at_ms = now_ms + headers->implicit_cache_ttl_ms();
    } else if (stale_rewrite()) {
      // If we are serving a stale rewrite, set the cache ttl to the minimum of
      // the implicit cache TTL and the original ttl.
      // TODO(matterbury): Consider a better way to handle stale resources.
      // Let's say that we get some requests over time for a resource:
      // 1. At 00:00:00 we get a request and the resource has a TTL of 10:00.
      //    We will return it with a max-age of 600 (10 minutes).
      // 2. At 00:09:00 we get a request for the same resource.
      //    We will return it with a max-age of 60 (1 minute) because of the
      //    'expire_at_ms - now_ms' below as we don't trigger either condition
      //    that changes expire_at_ms.
      // 3. At 00:11:00 we get a request for the same resource.
      //    It is now stale because its max age has expired but it's still
      //    within the options()->metadata_cache_staleness_threshold_ms(), or
      //    so we shall assume.
      // In this case, we need to pick a reasonable max age. One possibility is
      // "however much of the cache_staleness_threshold is left", but what do
      // we do if that's none?
      // Currently we set it as the lesser of our implicit cache TTL and the
      // original resource's TTL since that seems to be a reasonable value.
      expire_at_ms = now_ms + std::min(headers->implicit_cache_ttl_ms(),
                                       expire_at_ms - date_ms);
    }
    headers->SetDateAndCaching(now_ms, expire_at_ms - now_ms);
    AddVaryIfRequired(cached_result, headers);
  }
}

void InPlaceRewriteContext::UpdateDateAndExpiry(
    const protobuf::RepeatedPtrField<InputInfo>& inputs,
    int64* date_ms,
    int64* expire_at_ms) {
  for (int j = 0, m = inputs.size(); j < m; ++j) {
    const InputInfo& dependency = inputs.Get(j);
    if (dependency.has_expiration_time_ms() && dependency.has_date_ms()) {
      *date_ms = std::min(*date_ms, dependency.date_ms());
      *expire_at_ms = std::min(*expire_at_ms, dependency.expiration_time_ms());
    }
  }
}

void InPlaceRewriteContext::FetchCallbackDone(bool success) {
  if (is_rewritten_ && num_output_partitions() == 1) {
    // In-place rewrites always have a single output partition.
    // Freshen the resource if possible. Note that since is_rewritten_ is true,
    // we got a metadata cache hit and a hit on the rewritten resource in cache.
    // TODO(nikhilmadan): Freshening is broken for inplace rewrites on css,
    // since we don't update the other dependencies.
    Freshen();
  }
  RewriteContext::FetchCallbackDone(success);
}

RewriteFilter* InPlaceRewriteContext::GetRewriteFilter(
    const ContentType& type) {
  const RewriteOptions* options = Driver()->options();
  if (type.type() == ContentType::kCss &&
      options->Enabled(RewriteOptions::kRewriteCss)) {
    return Driver()->FindFilter(RewriteOptions::kCssFilterId);
  }
  if (type.IsJs() &&
      options->Enabled(RewriteOptions::kRewriteJavascriptExternal)) {
    return Driver()->FindFilter(RewriteOptions::kJavascriptMinId);
  }
  if (type.IsImage() && options->ImageOptimizationEnabled()) {
    // TODO(nikhilmadan): This converts one image format to another. We
    // shouldn't do inter-conversion since we can't change the file extension.
    return Driver()->FindFilter(RewriteOptions::kImageCompressionId);
  }
  return NULL;
}

void InPlaceRewriteContext::RewriteSingle(const ResourcePtr& input,
                                          const OutputResourcePtr& output) {
  input_resource_ = input;
  output_resource_ = output;
  input->DetermineContentType();
  if (input->type() != NULL && input->IsSafeToRewrite(rewrite_uncacheable())) {
    const ContentType* type = input->type();
    RewriteFilter* filter = GetRewriteFilter(*type);
    if (filter != NULL) {
      ResourceSlotPtr in_place_slot(
          new InPlaceRewriteResourceSlot(slot(0)->resource()));
      RewriteContext* context = filter->MakeNestedRewriteContext(
          this, in_place_slot);
      if (context != NULL) {
        AddNestedContext(context);
        // Propagate the uncacheable resource rewriting settings.
        context->set_rewrite_uncacheable(rewrite_uncacheable());
        if (!is_rewritten_ && !rewritten_hash_.empty()) {
          // The in-place metadata was found but the rewritten resource is not.
          // Hence, make the nested rewrite skip the metadata and force a
          // rewrite.
          context->set_force_rewrite(true);
        } else if (Options()->in_place_wait_for_optimized()) {
          // The nested rewrite might just return a URL and not the content
          // unless we set this. This would happen if another rewriter just
          // wrote the optimized version to cache (race condition).
          // TODO(jkarlin): Instead of forcing a rewrite we could check the
          // cache.
          context->set_force_rewrite(true);
        }
        StartNestedTasks();
        return;
      } else {
        LOG(ERROR) << "Filter (" << filter->id() << ") does not support "
                   << "nested contexts.";
        in_place_slot.clear();
      }
    }
  }
  // Give up on the rewrite.
  RewriteDone(kRewriteFailed, 0);
  // TODO(nikhilmadan): If the resource is not cacheable, cache this in the
  // metadata so that the fetcher can skip reading from the cache.
}

bool InPlaceRewriteContext::DecodeFetchUrls(
    const OutputResourcePtr& output_resource,
    MessageHandler* message_handler,
    GoogleUrlStarVector* url_vector) {
  GoogleUrl* url = new GoogleUrl(url_);
  url_vector->push_back(url);
  return true;
}

namespace {

// Callback class used to asynchronously load a non-http resource into
// a RecordingFetch.  There are two types of non-http resources in
// this context: FileInputResource and DataUrlInputResource, but our
// concern for now is FileInputResource.  We do not want to use the
// HTTPCache for such input resources, so the code is forked where
// this is constructed.
//
// TODO(jmarantz): I think we should consider whether it makes sense
// to use CacheFetcher for this; it might make more sense to put
// the decision to use the HTTPCache into UrlInputResource, and
// then this callback would be used in all flows.
class NonHttpResourceCallback : public Resource::AsyncCallback {
 public:
  NonHttpResourceCallback(const ResourcePtr& resource,
                          bool proxy_mode,
                          RewriteContext* context,
                          RecordingFetch* fetch,
                          MessageHandler* handler)
      : AsyncCallback(resource),
        proxy_mode_(proxy_mode),
        context_(context),
        async_fetch_(fetch),
        message_handler_(handler) {
  }

  virtual void Done(bool lock_failure, bool resource_ok) {
    if (!lock_failure && resource_ok) {
      async_fetch_->response_headers()->CopyFrom(
          *resource()->response_headers());
      async_fetch_->Write(resource()->contents(), message_handler_);
      async_fetch_->Done(true);
    } else {
      // TODO(jmarantz): If we're in proxy mode, we must always
      // produce the result.  If we're in origin mode, it's OK to fail.
      // But we'll never use load-from-file when acting as a proxy.
      // It would be better to enforce that formally.
      //
      // TODO(jmarantz): We might have to pass stuff through even on lock
      // failure.  Consider the error cases.

      DCHECK(!proxy_mode_) << "Failed to fetch url: " << resource()->url();
      async_fetch_->Done(false);
    }
    delete this;
  }

 private:
  bool proxy_mode_;
  RewriteContext* context_;
  RecordingFetch* async_fetch_;
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(NonHttpResourceCallback);
};

}  // namespace

void InPlaceRewriteContext::StartFetchReconstruction() {
  // The in-place metadata or the rewritten resource was not found in cache.
  // Fetch the original resource and trigger an asynchronous rewrite.
  if (num_slots() == 1) {
    ResourcePtr resource(slot(0)->resource());
    // If we get here, the resource must not have been rewritten.
    is_rewritten_ = false;
    RecordingFetch* fetch =
        new RecordingFetch(proxy_mode_, async_fetch(), resource, this,
                           fetch_message_handler());
    if (resource->UseHttpCache()) {
      if (proxy_mode_) {
        cache_fetcher_.reset(Driver()->CreateCacheFetcher());
        // Since we are proxying resources to user, we want to fetch it even
        // if there is a kRecentFetchNotCacheable message in the cache.
        cache_fetcher_->set_ignore_recent_fetch_failed(true);
      } else {
        cache_fetcher_.reset(Driver()->CreateCacheOnlyFetcher());
        // Since we are not proxying resources to user, we can respect
        // kRecentFetchNotCacheable messages.
        cache_fetcher_->set_ignore_recent_fetch_failed(false);
      }
      cache_fetcher_->Fetch(url_, fetch_message_handler(), fetch);
    } else {
      ServerContext* server_context = resource->server_context();
      MessageHandler* handler = server_context->message_handler();
      NonHttpResourceCallback* callback = new NonHttpResourceCallback(
          resource, proxy_mode_, this, fetch, handler);
      resource->LoadAsync(Resource::kLoadEvenIfNotCacheable,
                          Driver()->request_context(), callback);
    }
  } else {
    LOG(ERROR) << "Expected one resource slot, but found " << num_slots()
               << ".";
    delete this;
  }
}

void InPlaceRewriteContext::StartFetchReconstructionParent() {
  RewriteContext::StartFetchReconstruction();
}

bool InPlaceRewriteContext::InPlaceOptimizeForBrowserEnabled() const {
  return Options()->Enabled(RewriteOptions::kInPlaceOptimizeForBrowser) &&
      (Options()->Enabled(RewriteOptions::kConvertJpegToWebp) ||
       Options()->Enabled(RewriteOptions::kSquashImagesForMobileScreen));
}

// TODO(jmaessen): Sharpen this up.  Mark CSS vary:User-Agent because it doesn't
// see the Accept:image/webp header; we can skip this if all its images will be
// IPRO'd.  We don't need to mark non-webp-eligible images, which may require
// some fiddly options checking.  We need to treat webp lossless differently, so
// we can't just look at the extension and content type; right now we just
// disable lossless.
void InPlaceRewriteContext::AddVaryIfRequired(
    const CachedResult& cached_result, ResponseHeaders* headers) const {
  if (!InPlaceOptimizeForBrowserEnabled() || num_output_partitions() != 1) {
    // No browser-dependent rewrites => no need for vary
    return;
  }
  const ContentType* type = headers->DetermineContentType();
  // Returns true if we may return different rewritten content based
  // on the user agent.
  const char* new_vary = NULL;
  if (type->IsImage()) {
    // If it's an image, conservatively assume we might convert to webp.
    // Fix this up if we discover that this can't happen.
    new_vary = HttpAttributes::kAccept;
    if (Options()->Enabled(RewriteOptions::kSquashImagesForMobileScreen)) {
      new_vary = HttpAttributes::kUserAgent;
    } else if (!Options()->Enabled(RewriteOptions::kConvertJpegToWebp)) {
      // Lossy webp conversion won't happen, so no need to vary.
      new_vary = NULL;
    } else if (cached_result.minimal_webp_support() !=
               ResourceContext::LIBWEBP_LOSSY_ONLY) {
      // Can't do a lossy-only conversion, so we won't convert to webp in
      // place.
      new_vary = NULL;
    }
  } else if (type->IsCss()) {
    // If it's CSS, constituent images can be rewritten in a UA-dependent
    // manner.  But we don't necessarily see Accept:image/webp on the request,
    // so we must Vary: User-Agent.
    if (Options()->Enabled(RewriteOptions::kRewriteCss) &&
        (Options()->Enabled(RewriteOptions::kConvertJpegToWebp) ||
         Options()->Enabled(RewriteOptions::kConvertToWebpLossless))) {
      new_vary = HttpAttributes::kUserAgent;
    }
  }
  if (new_vary == NULL) {
    return;
  }
  if (Options()->private_not_vary_for_ie() &&
      Driver()->user_agent_matcher()->IsIe(Driver()->user_agent())) {
    // IE stores Vary: Accept resources in its cache, but must revalidate them
    // every single time they're fetched (except for older IE, which doesn't
    // cache them at all).  To avoid the re-validation cost (which imposes load
    // on the server unless a proxy cache deals with it) we by default serve
    // these resource cache-control: private to IE.  This will invalidate all
    // Vary: capable proxy caches along the way, though.  In practice this is
    // usually not be a big deal: few proxies handle Vary: Accept, though some
    // CDNs do, and none we've heard of handle Vary: User-Agent without special
    // configuration.
    headers->Add(HttpAttributes::kCacheControl, HttpAttributes::kPrivate);
    return;
  }
  ConstStringStarVector varies;
  if (headers->Lookup(HttpAttributes::kVary, &varies)) {
    // Need to add to the existing Vary header.  But first, check that the vary
    // header doesn't already encompass new_vary.
    for (int i = 0, s = varies.size(); i < s; ++i) {
      StringPiece vary(*varies[i]);
      if (StringPiece("*") == vary ||
          StringCaseEqual(HttpAttributes::kUserAgent, vary) ||
          (type->IsImage() &&
           StringCaseEqual(HttpAttributes::kAccept, vary))) {
        // Current Vary: header captures necessary vary information.
        return;
      }
    }
  }
  headers->Add(HttpAttributes::kVary, new_vary);
}

GoogleString InPlaceRewriteContext::UserAgentCacheKey(
    const ResourceContext* resource_context) const {
  if (InPlaceOptimizeForBrowserEnabled() && resource_context != NULL) {
    return ImageUrlEncoder::CacheKeyFromResourceContext(*resource_context);
  }
  return "";
}

// We risk intentionally increasing metadata cache fragmentation when request
// URL extensions are wrong or inconclusive.
// For a known extension, we optimistically think it tells us the
// correct resource type like image, css, etc. For images, we don't care about
// the actual image format (JPEG or PNG, for example). If the type derived
// from extension is wrong, we either lose the opportunity to optimize the
// resource based on user agent context (e.g., an image with .txt extension)
// or fragment the metadata cache unnecessarily (e.g., an HTML with .png
// extension)
// In case of an unknown extension or no extension in the URL, we encode
// all supported user agent capacities so that it will work for both image and
// CSS at the cost of unnecessary fragmentation of metadata cache.
void InPlaceRewriteContext::EncodeUserAgentIntoResourceContext(
    ResourceContext* context) {
  if (!InPlaceOptimizeForBrowserEnabled()) {
    return;
  }
  // TODO(jmaessen): filter->EncodeUserAgentIntoResourceContext(context)
  // actually calls the same method twice here.  In both cases we are also
  // dealing with possible mobile user agents and SetUserAgentScreenResolution,
  // which requires a different set of vary: headers.
  const ContentType* type = NameExtensionToContentType(url_);
  if (type == NULL) {
    // Get ImageRewriteFilter with any image type.
    RewriteFilter* filter = GetRewriteFilter(kContentTypeJpeg);
    if (filter != NULL) {
      filter->EncodeUserAgentIntoResourceContext(context);
    }
    filter = GetRewriteFilter(kContentTypeCss);
    if (filter != NULL) {
      filter->EncodeUserAgentIntoResourceContext(context);
    }
  } else if (type->IsImage() || type->IsCss()) {
    RewriteFilter* filter = GetRewriteFilter(*type);
    if (filter != NULL) {
      filter->EncodeUserAgentIntoResourceContext(context);
    }
  }
  // If we care about generating webp images, the above calls will have set
  // context->libwebp_level() != LIBWEBP_NONE.  InPlaceRewriteContext can only
  // serve webp resources if the browser sends the Accept: image/webp header
  // since the url cannot change based on content type.  This doesn't permit us
  // to serve webp lossless / alpha, as some older Opera versions send this
  // header but do not include such support.  This also means that some
  // webp-capable browsers (such as the stock Android browser) that don't send
  // Accept: image/webp will receive jpeg images.  Thus, if we thought WEBP
  // images were a possibility we disable webp generation unless we actually saw
  // the Accept: header, and we disable webp lossless entirely (falling back to
  // webp).
  // TODO(jmaessen): When non-webp-lossless capable versions of Opera are old
  // enough, enable lossless encoding if it was requested.  But note similar
  // nonsense will required for other new webp features such as animated webp.
  if (context->libwebp_level() != ResourceContext::LIBWEBP_NONE) {
    if (Driver()->request_properties()->SupportsWebpInPlace()) {
      context->set_libwebp_level(ResourceContext::LIBWEBP_LOSSY_ONLY);
    } else {
      context->set_libwebp_level(ResourceContext::LIBWEBP_NONE);
    }
  }
}

}  // namespace net_instaweb
