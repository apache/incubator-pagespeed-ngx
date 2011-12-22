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
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class UrlAsyncFetcher;

void ResourceFetch::Start(ResourceManager* manager,
                          const GoogleUrl& url,
                          AsyncFetch* async_fetch,
                          RewriteOptions* custom_options,
                          const GoogleString& version) {
  RewriteDriver* driver = (custom_options == NULL)
      ? manager->NewRewriteDriver()
      : manager->NewCustomRewriteDriver(custom_options);
  LOG(INFO) << "Fetch with RewriteDriver " << driver;
  ResourceFetch* resource_fetch = new ResourceFetch(
      url, async_fetch, manager->message_handler(), driver,
      manager->url_async_fetcher(), manager->timer(), version);
  // TODO(sligocki): This will currently fail us on all non-pagespeed
  // resource requests. We should move the check somewhere else.
  driver->FetchResource(url.Spec().as_string(), resource_fetch);
}

ResourceFetch::ResourceFetch(const GoogleUrl& url,
                             AsyncFetch* async_fetch,
                             MessageHandler* handler,
                             RewriteDriver* driver,
                             UrlAsyncFetcher* fetcher,
                             Timer* timer,
                             const GoogleString& version)
    : SharedAsyncFetch(async_fetch),
      fetcher_(fetcher),
      message_handler_(handler),
      driver_(driver),
      timer_(timer),
      version_(version),
      start_time_us_(timer->NowUs()),
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

  response_headers()->Add(kPageSpeedHeader, version_);
  base_fetch()->HeadersComplete();
}

void ResourceFetch::HandleDone(bool success) {
  if (success) {
    LOG(INFO) << "Resource " << resource_url_.Spec()
              << " : " << response_headers()->status_code() ;
  } else {
    // This is a fetcher failure, like connection refused, not just an error
    // status code.
    LOG(ERROR) << "Fetch failed for resource url " << resource_url_.Spec();
    if (!response_headers()->headers_complete()) {
      response_headers()->SetStatusAndReason(HttpStatus::kNotFound);
    }
  }
  RewriteStats* stats = driver_->resource_manager()->rewrite_stats();
  stats->fetch_latency_histogram()->Add(
      (timer_->NowUs() - start_time_us_) / 1000.0);
  stats->total_fetch_count()->IncBy(1);
  driver_->Cleanup();
  base_fetch()->Done(success);
  delete this;
}

}  // namespace net_instaweb
