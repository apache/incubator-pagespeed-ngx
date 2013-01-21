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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_AJAX_REWRITE_CONTEXT_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_AJAX_REWRITE_CONTEXT_H_

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/http_value_writer.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class InputInfo;
class MessageHandler;
class RewriteDriver;
class RewriteFilter;
class Statistics;
class UrlAsyncFetcher;
class Variable;

// A resource-slot created for an ajax rewrite. This has an empty render method.
// Note that this class is usually used as a RefCountedPtr and gets deleted when
// there are no references remaining.
class AjaxRewriteResourceSlot : public ResourceSlot {
 public:
  explicit AjaxRewriteResourceSlot(const ResourcePtr& resource);

  // Implements ResourceSlot::Render().
  virtual void Render();

  // Implements ResourceSlot::LocationString().
  virtual GoogleString LocationString() { return "ajax"; }

 protected:
  virtual ~AjaxRewriteResourceSlot();

 private:
  DISALLOW_COPY_AND_ASSIGN(AjaxRewriteResourceSlot);
};

// Context that is used for an ajax rewrite.
class AjaxRewriteContext : public SingleRewriteContext {
 public:
  // Stats variable name to keep track of how often in-place falls back to
  // stream (due to a large resource) when Options->in_place_wait_for_optimized
  // is true.
  static const char kInPlaceOversizedOptStream[];

  AjaxRewriteContext(RewriteDriver* driver, const StringPiece& url);
  virtual ~AjaxRewriteContext();

  // Implements SingleRewriteContext::RewriteSingle().
  virtual void RewriteSingle(const ResourcePtr& input,
                             const OutputResourcePtr& output);
  // Implements RewriteContext::id().
  virtual const char* id() const { return RewriteOptions::kAjaxRewriteId; }
  // Implements RewriteContext::kind().
  virtual OutputResourceKind kind() const { return kRewrittenResource; }
  // Implements RewriteContext::DecodeFetchUrls().
  virtual bool DecodeFetchUrls(const OutputResourcePtr& output_resource,
                               MessageHandler* message_handler,
                               GoogleUrlStarVector* url_vector);
  // Implements RewriteContext::StartFetchReconstruction().
  virtual void StartFetchReconstruction();

  static void InitStats(Statistics* statistics);

  bool perform_http_fetch() const { return perform_http_fetch_; }
  void set_perform_http_fetch(bool x) { perform_http_fetch_ = x; }

 private:
  friend class RecordingFetch;
  // Implements RewriteContext::Harvest().
  virtual void Harvest();
  void StartFetchReconstructionParent();
  // Implements RewriteContext::FixFetchFallbackHeaders().
  virtual void FixFetchFallbackHeaders(ResponseHeaders* headers);
  // Implements RewriteContext::FetchTryFallback().
  virtual void FetchTryFallback(const GoogleString& url,
                                const StringPiece& hash);
  // Implements RewriteContext::FetchCallbackDone().
  virtual void FetchCallbackDone(bool success);

  RewriteFilter* GetRewriteFilter(const ContentType& type);

  // Update the date and expiry time based on the InputInfo's.
  void UpdateDateAndExpiry(const protobuf::RepeatedPtrField<InputInfo>& inputs,
                           int64* date_ms, int64* expiry_ms);

  RewriteDriver* driver_;
  GoogleString url_;
  // Boolean indicating whether or not the resource was rewritten successfully.
  bool is_rewritten_;
  // The hash of the rewritten resource. Note that this should only be used if
  // is_rewritten_ is true. This may be empty.
  GoogleString rewritten_hash_;

  // Information needed for nested rewrites.
  ResourcePtr input_resource_;
  OutputResourcePtr output_resource_;

  scoped_ptr<UrlAsyncFetcher> cache_fetcher_;

  // Should we fetch the contents if cache lookup fails?
  bool perform_http_fetch_;

  DISALLOW_COPY_AND_ASSIGN(AjaxRewriteContext);
};

// Records the fetch into the provided resource and passes through events to the
// underlying writer, response headers and callback.
class RecordingFetch : public SharedAsyncFetch {
 public:
  RecordingFetch(AsyncFetch* async_fetch,
                 const ResourcePtr& resource,
                 AjaxRewriteContext* context,
                 MessageHandler* handler);

  virtual ~RecordingFetch();

  // Implements SharedAsyncFetch::HandleHeadersComplete().
  virtual void HandleHeadersComplete();
  // Implements SharedAsyncFetch::HandleWrite().
  virtual bool HandleWrite(const StringPiece& content, MessageHandler* handler);
  // Implements SharedAsyncFetch::HandleFlush().
  virtual bool HandleFlush(MessageHandler* handler);
  // Implements SharedAsyncFetch::HandleDone().
  virtual void HandleDone(bool success);

 private:
  void FreeDriver();

  bool CanAjaxRewrite();

  // By default RecordingFetch streams back the original content to the browser.
  // If this returns false then the RecordingFetch should cache the original
  // content but not stream it.
  bool ShouldStream();

  MessageHandler* handler_;
  ResourcePtr resource_;
  AjaxRewriteContext* context_;
  bool can_ajax_rewrite_;
  bool streaming_;
  HTTPValue cache_value_;
  HTTPValueWriter cache_value_writer_;
  ResponseHeaders saved_headers_;
  Variable* in_place_oversized_opt_stream_;
  DISALLOW_COPY_AND_ASSIGN(RecordingFetch);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_AJAX_REWRITE_CONTEXT_H_
