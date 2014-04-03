/*
 * Copyright 2013 Google Inc.
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

// Author: anupama@google.com (Anupama Dutta)

#include "net/instaweb/rewriter/public/downstream_cache_purger.h"

#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

namespace {

class StringAsyncFetchWithAsyncCountUpdates : public StringAsyncFetch {
 public:
  StringAsyncFetchWithAsyncCountUpdates(const RequestContextPtr& ctx,
                                        RewriteDriver* driver)
      : StringAsyncFetch(ctx),
        driver_(driver) {
    driver_->increment_async_events_count();
  }

  virtual ~StringAsyncFetchWithAsyncCountUpdates() { }

  virtual void HandleDone(bool success) {
    if (response_headers()->status_code() == HttpStatus::kOK) {
      driver_->server_context()->rewrite_stats()->
          successful_downstream_cache_purges()->Add(1);
    }
    StringAsyncFetch::HandleDone(success);
    driver_->decrement_async_events_count();
    delete this;
  }

 private:
  RewriteDriver* driver_;

  DISALLOW_COPY_AND_ASSIGN(StringAsyncFetchWithAsyncCountUpdates);
};

}  // namespace

DownstreamCachePurger::DownstreamCachePurger(RewriteDriver* driver)
    : driver_(driver),
      made_downstream_purge_attempt_(false) {
}

DownstreamCachePurger::~DownstreamCachePurger() {}

void DownstreamCachePurger::Clear() {
  purge_url_.clear();
  purge_method_.clear();
  made_downstream_purge_attempt_ = false;
}

bool DownstreamCachePurger::GeneratePurgeRequestParameters(
    const GoogleUrl& page_url) {
  purge_url_ = StrCat(
      driver_->options()->downstream_cache_purge_location_prefix(),
      page_url.PathAndLeaf());
  purge_method_ = driver_->options()->downstream_cache_purge_method();
  return (!purge_url_.empty() && !purge_method_.empty());
}

// This function uses a few variables gaurded by rewrite_mutex() without locking
// it, but we should not have concurrent responses at this point so thread
// safety analysis is disabled.
bool DownstreamCachePurger::ShouldPurgeRewrittenResponse(
    const GoogleUrl& google_url) NO_THREAD_SAFETY_ANALYSIS {
  if (!driver_->options()->IsDownstreamCacheIntegrationEnabled()) {
    // Downstream caching is not enabled.
    return false;
  }
  if (driver_->num_initiated_rewrites() == 0) {
    // No rewrites were initiated. Could happen if the rewriters
    // enabled don't apply on the page, or apply instantly (e.g.
    // collapse whitespace).
    return false;
  }
  // Figure out what percentage of the rewriting was done before the
  // response was served out, so that we can initiate a cache purge if there
  // was significant amount of rewriting remaining to be done.
  float served_rewritten_percentage =
      ((driver_->num_initiated_rewrites() - driver_->num_detached_rewrites()) *
       100.0) /
       driver_->num_initiated_rewrites();
  if (served_rewritten_percentage <
      driver_->options()->downstream_cache_rewritten_percentage_threshold()) {
    driver_->message_handler()->Message(
        kInfo,
        "Should purge \"%s\" which was served with only %d%% rewriting done.",
        google_url.spec_c_str(),
        static_cast<int>(served_rewritten_percentage));
    return true;
  }
  return false;
}

void DownstreamCachePurger::PurgeDownstreamCache() {
  // TODO(anupama): Use purge_method actually.
  StringAsyncFetchWithAsyncCountUpdates* dummy_fetch =
      new StringAsyncFetchWithAsyncCountUpdates(driver_->request_context(),
                                                driver_);
  // Add a purge-related header so that the purge request does not
  // get us into a loop.
  dummy_fetch->request_headers()->CopyFrom(*driver_->request_headers());
  dummy_fetch->request_headers()->Add(kPsaPurgeRequest, "1");
  if (purge_method_ == "PURGE") {
    dummy_fetch->request_headers()->set_method(RequestHeaders::kPurge);
  }
  // Record the fact that a purge attempt has been made so that we do not
  // issue multiple purges using the same RewriteDriver object.
  made_downstream_purge_attempt_ = true;

  driver_->message_handler()->Message(kInfo,
                                      "Purge url is %s", purge_url_.c_str());
  driver_->async_fetcher()->Fetch(purge_url_, driver_->message_handler(),
                                  dummy_fetch);
}

bool DownstreamCachePurger::MaybeIssuePurge(const GoogleUrl& google_url) {
  // If any of the following conditions are satisfied, we do not issue a purge:
  // a) a purge attempt has already been made
  // b) request headers have not been set
  // c) this is a looped back purge request
  // d) rewritten response is not optimized enough to warrant a purge
  // e) valid purge URL or method are unavailable
  if (!made_downstream_purge_attempt_ &&
      driver_->request_headers() != NULL &&
      driver_->request_headers()->Lookup1(kPsaPurgeRequest) == NULL &&
      driver_->request_headers()->method() == RequestHeaders::kGet &&
      google_url.IsWebValid() &&
      ShouldPurgeRewrittenResponse(google_url) &&
      GeneratePurgeRequestParameters(google_url)) {
    driver_->server_context()->rewrite_stats()->
         downstream_cache_purge_attempts()->Add(1);
    // Purge old version from cache since we will have a better rewritten
    // version available on the next request. The purge request will
    // use the same request headers as the request (and hence the same
    // UserAgent etc.).
    PurgeDownstreamCache();
    return true;
  }
  return false;
}

}  // namespace net_instaweb
