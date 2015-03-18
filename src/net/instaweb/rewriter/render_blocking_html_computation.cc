/*
 * Copyright 2015 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/rewriter/public/render_blocking_html_computation.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"

namespace net_instaweb {

class RenderBlockingHtmlComputation::ResourceCallback
    : public Resource::AsyncCallback {
 public:
  ResourceCallback(RewriteDriver* parent_driver,
                   const ResourcePtr& resource,
                   RenderBlockingHtmlComputation* computation)
        : AsyncCallback(resource), parent_driver_(parent_driver),
          computation_(computation) {}

  virtual void Done(bool lock_failure, bool resource_ok) {
    // Shouldn't be enforcing locks on this anyway.
    DCHECK(!lock_failure);
    if (!resource_ok) {
      computation_->ReportResult(false);
      delete this;
    } else {
      parent_driver_->rewrite_worker()->Add(
          MakeFunction(this, &ResourceCallback::ParseAndFilter));
    }
  }

  void ParseAndFilter() {
    bool ok = false;
    scoped_ptr<RewriteDriver> child_driver(
        parent_driver_->server_context()->NewUnmanagedRewriteDriver(
            NULL /* no pool */,
            parent_driver_->options()->Clone(),
            parent_driver_->request_context()));
    // Keep it alive beyond auto-cleanup so client's Done can get info out of
    // filters.
    child_driver->set_externally_managed(true);

    computation_->SetupFilters(child_driver.get());
    if (!child_driver->StartParse(resource()->url())) {
      LOG(DFATAL) << "Huh? StartParse doesn't like URL, but resource fetched:"
                  << resource()->url();
      child_driver->Cleanup();
    } else {
      child_driver->ParseText(resource()->contents());
      child_driver->FinishParse();
      ok = true;
    }

    computation_->ReportResult(ok);
    delete this;
  }

 private:
  RewriteDriver* parent_driver_;
  RenderBlockingHtmlComputation* computation_;
};

RenderBlockingHtmlComputation::RenderBlockingHtmlComputation(
    RewriteDriver* parent_driver) : parent_driver_(parent_driver) {}

void RenderBlockingHtmlComputation::Compute(const GoogleString& url) {
  parent_driver_->IncrementRenderBlockingAsyncEventsCount();

  GoogleUrl gurl(url);
  if (!gurl.IsWebValid()) {
    ReportResult(false);
    return;
  }

  bool is_authorized = false;
  ResourcePtr resource(
      parent_driver_->CreateInputResource(gurl, &is_authorized));
  if (resource.get() == NULL) {
    ReportResult(false);
    return;
  }

  // Don't cancel us willy-nilly. (Cancellation due to e.g. shutdown
  // will just look like failures to us and will be passed on to our
  // client).
  resource->set_is_background_fetch(false);
  ResourceCallback* resource_callback =
      new ResourceCallback(parent_driver_, resource, this);
  resource->LoadAsync(Resource::kLoadEvenIfNotCacheable,
                      parent_driver_->request_context(),
                      resource_callback);
}

RenderBlockingHtmlComputation::~RenderBlockingHtmlComputation() {}

void RenderBlockingHtmlComputation::ReportResult(bool success) {
  Done(success);
  parent_driver_->DecrementRenderBlockingAsyncEventsCount();
  delete this;
}

}  // namespace net_instaweb
