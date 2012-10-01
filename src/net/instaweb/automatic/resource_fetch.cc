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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/automatic/public/resource_fetch.h"

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/sync_fetcher_adapter_callback.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_pool.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

void ResourceFetch::ApplyFuriousOptions(const ServerContext* server_context,
                                        const GoogleUrl& url,
                                        RewriteDriverPool* driver_pool,
                                        RewriteOptions** custom_options) {
  const RewriteOptions* active_options;
  if (*custom_options == NULL) {
    active_options = driver_pool->TargetOptions();
  } else {
    active_options = *custom_options;
  }
  if (active_options->running_furious()) {
    // If we're running an experiment and this resource url specifies a
    // furious_spec, make sure the custom options have that experiment selected.
    ResourceNamer namer;
    namer.Decode(url.LeafSansQuery());
    if (namer.has_experiment()) {
      if (*custom_options == NULL) {
        *custom_options = active_options->Clone();
      }
      (*custom_options)->SetFuriousStateStr(namer.experiment());
      server_context->ComputeSignature(*custom_options);
    }
  }
}

RewriteDriver* ResourceFetch::GetDriver(
    const GoogleUrl& url, RewriteOptions* custom_options,
    RewriteDriverPool* driver_pool, bool using_spdy,
    ServerContext* server_context) {
  DCHECK((custom_options != NULL) ^ (driver_pool != NULL));
  ApplyFuriousOptions(server_context, url, driver_pool, &custom_options);
  RewriteDriver* driver = (custom_options == NULL)
      ? server_context->NewRewriteDriverFromPool(driver_pool)
      : server_context->NewCustomRewriteDriver(custom_options);
  // Note: this is reset in RewriteDriver::clear().
  driver->set_using_spdy(using_spdy);
  return driver;
}

void ResourceFetch::StartWithDriver(
    const GoogleUrl& url, ServerContext* manager, RewriteDriver* driver,
    AsyncFetch* async_fetch) {

  ResourceFetch* resource_fetch = new ResourceFetch(
      url, driver, manager->timer(), manager->message_handler(), async_fetch);

  driver->FetchResource(url.Spec(), resource_fetch);
}

void ResourceFetch::Start(const GoogleUrl& url,
                          RewriteOptions* custom_options,
                          bool using_spdy,
                          ServerContext* server_context,
                          AsyncFetch* async_fetch) {
  RewriteDriver* driver = GetDriver(
      url, custom_options,
      (custom_options != NULL) ?
          NULL : server_context->standard_rewrite_driver_pool(),
      using_spdy, server_context);
  StartWithDriver(url, server_context, driver, async_fetch);
}

bool ResourceFetch::BlockingFetch(const GoogleUrl& url,
                                  ServerContext* manager,
                                  RewriteDriver* driver,
                                  SyncFetcherAdapterCallback* callback) {
  StartWithDriver(url, manager, driver, callback);

  // Wait for resource fetch to complete.
  if (!callback->done()) {
    int64 max_ms = driver->options()->blocking_fetch_timeout_ms();
    for (int64 start_ms = manager->timer()->NowMs(), now_ms = start_ms;
         !callback->done() && now_ms - start_ms < max_ms;
         now_ms = manager->timer()->NowMs()) {
      int64 remaining_ms = max_ms - (now_ms - start_ms);

      driver->BoundedWaitFor(RewriteDriver::kWaitForCompletion, remaining_ms);
    }
  }

  MessageHandler* message_handler = manager->message_handler();
  bool ok = false;
  if (callback->done()) {
    if (callback->success()) {
      ok = true;
    } else {
      message_handler->Message(kWarning, "Fetch failed for %s, status=%d",
                               url.spec_c_str(),
                               callback->response_headers()->status_code());
    }
  } else {
    message_handler->Message(kWarning, "Fetch timed out for %s",
                             url.spec_c_str());
  }

  return ok;
}

ResourceFetch::ResourceFetch(const GoogleUrl& url,
                             RewriteDriver* driver,
                             Timer* timer,
                             MessageHandler* handler,
                             AsyncFetch* async_fetch)
    : SharedAsyncFetch(async_fetch),
      driver_(driver),
      timer_(timer),
      message_handler_(handler),
      start_time_ms_(timer->NowMs()),
      redirect_count_(0) {
  resource_url_.Reset(url);
}

ResourceFetch::~ResourceFetch() {
}

void ResourceFetch::HandleHeadersComplete() {
  // We do not want any cookies (or other person information) in pagespeed
  // resources. They shouldn't be here anyway, but we assure that.
  ConstStringStarVector v;
  DCHECK(!response_headers()->Lookup(HttpAttributes::kSetCookie, &v));
  DCHECK(!response_headers()->Lookup(HttpAttributes::kSetCookie2, &v));
  response_headers()->RemoveAll(HttpAttributes::kSetCookie);
  response_headers()->RemoveAll(HttpAttributes::kSetCookie2);

  // "Vary: Accept-Encoding" for all resources that are transmitted compressed.
  // Server ought to set these, I suppose.
  // response_headers()->Add(HttpAttributes::kVary, "Accept-Encoding");

  response_headers()->Add(kPageSpeedHeader,
                          driver_->options()->x_header_value());
  base_fetch()->HeadersComplete();
}

void ResourceFetch::HandleDone(bool success) {
  if (success) {
    LOG(INFO) << "Resource " << resource_url_.Spec()
              << " : " << response_headers()->status_code() ;
  } else {
    // This is a fetcher failure, like connection refused, not just an error
    // status code.
    LOG(WARNING) << "Fetch failed for resource url " << resource_url_.Spec();
    if (!response_headers()->headers_complete()) {
      response_headers()->SetStatusAndReason(HttpStatus::kNotFound);
    }
  }
  RewriteStats* stats = driver_->server_context()->rewrite_stats();
  stats->fetch_latency_histogram()->Add(timer_->NowMs() - start_time_ms_);
  stats->total_fetch_count()->IncBy(1);
  driver_->Cleanup();
  base_fetch()->Done(success);
  delete this;
}

}  // namespace net_instaweb
