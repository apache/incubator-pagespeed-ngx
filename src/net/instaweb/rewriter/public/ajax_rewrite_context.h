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

#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class MessageHandler;
class ResponseHeaders;
class RewriteDriver;

// A resource-slot created for an ajax rewrite. This has an empty render method.
class AjaxRewriteResourceSlot : public ResourceSlot {
 public:
  explicit AjaxRewriteResourceSlot(const ResourcePtr& resource)
      : ResourceSlot(resource) {}

  virtual void Render();
  virtual GoogleString LocationString() { return "ajax"; }

 protected:
  virtual ~AjaxRewriteResourceSlot();

 private:
  DISALLOW_COPY_AND_ASSIGN(AjaxRewriteResourceSlot);
};

// Context that is used for an ajax rewrite.
class AjaxRewriteContext : public SingleRewriteContext {
 public:
  AjaxRewriteContext(RewriteDriver* driver,
                     const GoogleString& url,
                     const RequestHeaders& request_headers)
      : SingleRewriteContext(driver, NULL, NULL),
        driver_(driver),
        url_(url) {
    request_headers_.CopyFrom(request_headers);
    set_notify_driver_on_fetch_done(true);
  }

  virtual ~AjaxRewriteContext();

  virtual void RewriteSingle(const ResourcePtr& input,
                             const OutputResourcePtr& output);
  virtual const char* id() const { return "aj"; }
  virtual OutputResourceKind kind() const { return kRewrittenResource; }

  virtual bool DecodeFetchUrls(const OutputResourcePtr& output_resource,
                               MessageHandler* message_handler,
                               GoogleUrlStarVector* url_vector);

  virtual void StartFetchReconstruction();

  virtual GoogleString CacheKey() const;

 private:
  friend class RecordingFetch;
  class RecordingFetch;

  virtual void Harvest();
  void StartFetchReconstructionParent();
  virtual void FixFetchFallbackHeaders(ResponseHeaders* headers);

  RewriteDriver* driver_;
  GoogleString url_;
  RequestHeaders request_headers_;
  bool success_;

  DISALLOW_COPY_AND_ASSIGN(AjaxRewriteContext);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_AJAX_REWRITE_CONTEXT_H_
