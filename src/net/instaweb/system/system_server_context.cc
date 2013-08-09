// Copyright 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: jefftk@google.com (Jeff Kaufman)

#include "net/instaweb/system/public/system_server_context.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/system/public/add_headers_fetcher.h"
#include "net/instaweb/system/public/loopback_route_fetcher.h"
#include "net/instaweb/system/public/system_request_context.h"
#include "net/instaweb/system/public/system_rewrite_options.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

const char kCacheFlushCount[] = "cache_flush_count";
const char kCacheFlushTimestampMs[] = "cache_flush_timestamp_ms";

}  // namespace

SystemServerContext::SystemServerContext(RewriteDriverFactory* factory)
    : ServerContext(factory),
      cache_flush_mutex_(thread_system()->NewMutex()),
      last_cache_flush_check_sec_(0),
      cache_flush_count_(NULL),           // Lazy-initialized under mutex.
      cache_flush_timestamp_ms_(NULL) {   // Lazy-initialized under mutex.
}

SystemServerContext::~SystemServerContext() {
}

// If we haven't checked the timestamp of $FILE_PREFIX/cache.flush in the past
// cache_flush_poll_interval_sec_ seconds do so, and if the timestamp has
// expired then update the cache_invalidation_timestamp in global_options,
// thus flushing the cache.
void SystemServerContext::FlushCacheIfNecessary() {
  int64 cache_flush_poll_interval_sec =
      system_rewrite_options()->cache_flush_poll_interval_sec();
  if (cache_flush_poll_interval_sec > 0) {
    int64 now_sec = timer()->NowMs() / Timer::kSecondMs;
    bool check_cache_file = false;
    {
      ScopedMutex lock(cache_flush_mutex_.get());
      if (now_sec >= (last_cache_flush_check_sec_ +
                      cache_flush_poll_interval_sec)) {
        last_cache_flush_check_sec_ = now_sec;
        check_cache_file = true;
      }
      if (cache_flush_count_ == NULL) {
        cache_flush_count_ = statistics()->GetVariable(kCacheFlushCount);
        cache_flush_timestamp_ms_ = statistics()->GetVariable(
            kCacheFlushTimestampMs);
      }
    }

    if (check_cache_file) {
      GoogleString cache_flush_filename =
          system_rewrite_options()->cache_flush_filename();
      if (cache_flush_filename.empty()) {
        cache_flush_filename = "cache.flush";
      }
      if (cache_flush_filename[0] != '/') {
        // Implementations must ensure the file cache path is an absolute path.
        // mod_pagespeed checks in mod_instaweb.cc:pagespeed_post_config while
        // ngx_pagespeed checks in ngx_pagespeed.cc:ps_merge_srv_conf.
        DCHECK_EQ('/', system_rewrite_options()->file_cache_path()[0]);
        cache_flush_filename = StrCat(
            system_rewrite_options()->file_cache_path(), "/",
            cache_flush_filename);
      }
      int64 cache_flush_timestamp_sec;
      NullMessageHandler null_handler;
      if (file_system()->Mtime(cache_flush_filename,
                               &cache_flush_timestamp_sec,
                               &null_handler)) {
        int64 timestamp_ms = cache_flush_timestamp_sec * Timer::kSecondMs;

        bool flushed = UpdateCacheFlushTimestampMs(timestamp_ms);

        // The multiple child processes each must independently
        // discover a fresh cache.flush and update the options. However,
        // as shown in
        //     http://code.google.com/p/modpagespeed/issues/detail?id=568
        // we should only bump the flush-count and print a warning to
        // the log once per new timestamp.
        if (flushed &&
            (timestamp_ms !=
             cache_flush_timestamp_ms_->SetReturningPreviousValue(
                 timestamp_ms))) {
          int count = cache_flush_count_->Add(1);
          message_handler()->Message(kWarning, "Cache Flush %d", count);
        }
      }
    } else {
      // Check on every request whether another child process has updated the
      // statistic.
      int64 timestamp_ms = cache_flush_timestamp_ms_->Get();

      // Do the difference-check first because that involves only a
      // reader-lock, so we have zero contention risk when the cache is not
      // being flushed.
      if ((timestamp_ms > 0) &&
          (global_options()->cache_invalidation_timestamp() < timestamp_ms)) {
        UpdateCacheFlushTimestampMs(timestamp_ms);
      }
    }
  }
}

bool SystemServerContext::UpdateCacheFlushTimestampMs(int64 timestamp_ms) {
  return global_options()->UpdateCacheInvalidationTimestampMs(timestamp_ms);
}

SystemRewriteOptions* SystemServerContext::system_rewrite_options() {
  SystemRewriteOptions* out =
      dynamic_cast<SystemRewriteOptions*>(global_options());
  CHECK(out != NULL);
  return out;
}

void SystemServerContext::InitStats(Statistics* statistics) {
  statistics->AddVariable(kCacheFlushCount);
  statistics->AddVariable(kCacheFlushTimestampMs);
}

void SystemServerContext::ApplySessionFetchers(
    const RequestContextPtr& request, RewriteDriver* driver) {
  const SystemRewriteOptions* conf =
      SystemRewriteOptions::DynamicCast(driver->options());
  CHECK(conf != NULL);
  SystemRequestContext* system_request = SystemRequestContext::DynamicCast(
      request.get());
  if (system_request == NULL) {
    return;  // decoding_driver has a null RequestContext.
  }

  // Note that these fetchers are applied in the opposite order of how they are
  // added: the last one added here is the first one applied and vice versa.
  //
  // Currently, we want AddHeadersFetcher running first, then perhaps
  // SpdyFetcher and then LoopbackRouteFetcher (and then Serf).
  //
  // We want AddHeadersFetcher to run before the SpdyFetcher since we
  // want any headers it adds to be visible.
  //
  // We want SpdyFetcher to run before LoopbackRouteFetcher as it needs
  // to know the request hostname, which LoopbackRouteFetcher could potentially
  // rewrite to 127.0.0.1; and it's OK without the rewriting since it will
  // always talk to the local machine anyway.
  SystemRewriteOptions* options = system_rewrite_options();
  if (!options->disable_loopback_routing() &&
      !options->slurping_enabled() &&
      !options->test_proxy()) {
    // Note the port here is our port, not from the request, since
    // LoopbackRouteFetcher may decide we should be talking to ourselves.
    driver->SetSessionFetcher(new LoopbackRouteFetcher(
        driver->options(), system_request->local_ip(),
        system_request->local_port(), driver->async_fetcher()));
  }

  // Apache has experimental support for direct fetching from mod_spdy.  Other
  // implementations that support something similar would use this hook.
  MaybeApplySpdySessionFetcher(request, driver);

  if (driver->options()->num_custom_fetch_headers() > 0) {
    driver->SetSessionFetcher(new AddHeadersFetcher(driver->options(),
                                                    driver->async_fetcher()));
  }
}

}  // namespace net_instaweb
