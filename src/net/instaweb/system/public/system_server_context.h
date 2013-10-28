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

#ifndef NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_SERVER_CONTEXT_H_
#define NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_SERVER_CONTEXT_H_

#include "net/instaweb/rewriter/public/server_context.h"

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

class AbstractMutex;
class Histogram;
class SharedMemStatistics;
class RewriteDriver;
class RewriteDriverFactory;
class RewriteStats;
class Statistics;
class SystemRewriteDriverFactory;
class SystemRewriteOptions;
class UrlAsyncFetcherStats;
class Variable;

// A server context with features specific to a PSOL port on a unix system.
class SystemServerContext : public ServerContext {
 public:
  SystemServerContext(RewriteDriverFactory* factory,
                      StringPiece hostname, int port);
  virtual ~SystemServerContext();

  // Implementations should call this method on every request, both for html and
  // resources, to avoid serving stale resources.
  //
  // TODO(jmarantz): allow a URL-based mechanism to flush cache, even if
  // we implement it by simply writing the cache.flush file so other
  // servers can see it.  Note that using shared-memory is not a great
  // plan because we need the cache-invalidation to persist across server
  // restart.
  void FlushCacheIfNecessary();

  SystemRewriteOptions* system_rewrite_options();
  GoogleString hostname_identifier() { return hostname_identifier_; }

  static void InitStats(Statistics* statistics);

  // Called by SystemRewriteDriverFactory::ChildInit.  See documentation there.
  virtual void ChildInit(SystemRewriteDriverFactory* factory);

  // Initialize this ServerContext to have its own statistics domain.  Must be
  // called after global_statistics has been created and had ::InitStats called
  // on it.
  void CreateLocalStatistics(Statistics* global_statistics,
                             SystemRewriteDriverFactory* factory);

  // Whether ChildInit() has been called yet.  Exposed so debugging code can
  // verify initialization proceeded properly.
  bool initialized() const { return initialized_; }

  // Normally we just fetch with the default UrlAsyncFetcher, generally serf,
  // but there are some cases where we need to do something more complex:
  //  - Local requests: requests for resources on this host should go directly
  //    to the local IP.
  //  - Fetches directly from other modules: in Apache we have an experimental
  //    pathway where we can make fetches directly from mod_spdy without going
  //    out to the network.
  //  - Custom fetch headers: before continuing with the fetch we want to add
  //    request headers.
  // Session fetchers allow us to make these decisions.  Here we may update
  // driver->async_fetcher() to be a special fetcher just for this request.
  virtual void ApplySessionFetchers(const RequestContextPtr& req,
                                    RewriteDriver* driver);

  // Accumulate in a histogram the amount of time spent rewriting HTML.
  // TODO(sligocki): Remove in favor of RewriteStats::rewrite_latency_histogram.
  void AddHtmlRewriteTimeUs(int64 rewrite_time_us);

  // Hook called after all configuration parsing is done to support implementers
  // like ApacheServerContext that need to collapse configuration inside the
  // config overlays into actual RewriteOptions objects.  It will also compute
  // signatures when done, and by default that's the only thing it does.
  virtual void CollapseConfigOverlaysAndComputeSignatures();

 protected:
  // Flush the cache by updating the cache flush timestamp in the global
  // options.  This will change its signature, which is part of the cache key,
  // and so all previously cached entries will be unreachable.
  //
  // Returns true if it actually updated the timestamp, false if the existing
  // cache flush timestamp was newer or the same as the one provided.
  //
  // Subclasses which add aditional configurations need to override this method
  // to additionally update the cache flush timestamp in those other
  // configurations.  See ApacheServerContext::UpdateCacheFlushTimestampMs where
  // the separate SpdyConfig that mod_pagespeed uses when using SPDY also needs
  // to have it's timestamp bumped.
  virtual bool UpdateCacheFlushTimestampMs(int64 timestamp_ms);

  // Hook for implementations to support fetching directly from the spdy module.
  virtual void MaybeApplySpdySessionFetcher(const RequestContextPtr& request,
                                            RewriteDriver* driver) {}

  Variable* statistics_404_count();

 private:
  bool initialized_;

  // State used to implement periodic polling of $FILE_PREFIX/cache.flush.
  // last_cache_flush_check_sec_ is ctor-initialized to 0 so the first
  // time we Poll we will read the file.
  scoped_ptr<AbstractMutex> cache_flush_mutex_;
  int64 last_cache_flush_check_sec_;  // seconds since 1970

  Variable* cache_flush_count_;
  Variable* cache_flush_timestamp_ms_;

  Histogram* html_rewrite_time_us_histogram_;

  // Non-NULL if we have per-vhost stats.
  scoped_ptr<Statistics> split_statistics_;

  // May be NULL. Owned by *split_statistics_.
  SharedMemStatistics* local_statistics_;

  // These are non-NULL if we have per-vhost stats.
  scoped_ptr<RewriteStats> local_rewrite_stats_;
  scoped_ptr<UrlAsyncFetcherStats> stats_fetcher_;

  // hostname_identifier_ equals to "server_hostname:port" of the server.  It's
  // used to distinguish the name of shared memory so that each vhost has its
  // own SharedCircularBuffer.
  GoogleString hostname_identifier_;

  DISALLOW_COPY_AND_ASSIGN(SystemServerContext);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_SERVER_CONTEXT_H_
