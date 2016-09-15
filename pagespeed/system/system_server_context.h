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

#ifndef PAGESPEED_SYSTEM_SYSTEM_SERVER_CONTEXT_H_
#define PAGESPEED_SYSTEM_SYSTEM_SERVER_CONTEXT_H_

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/system/admin_site.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/writer.h"
#include "pagespeed/kernel/util/copy_on_write.h"

namespace net_instaweb {

class AsyncFetch;
class GoogleUrl;
class Histogram;
class QueryParams;
class PurgeSet;
class RewriteDriver;
class RewriteDriverFactory;
class RewriteOptions;
class RewriteStats;
class SharedMemStatistics;
class Statistics;
class SystemCachePath;
class SystemCaches;
class SystemRewriteDriverFactory;
class SystemRewriteOptions;
class UpDownCounter;
class UrlAsyncFetcherStats;
class Variable;

// A server context with features specific to a PSOL port on a unix system.
class SystemServerContext : public ServerContext {
 public:
  // Identifies whether the user arrived at an admin page from a
  // /pagespeed_admin handler or a /*_pagespeed_statistics handler.
  // The main difference between these is that the _admin site might in the
  // future grant more privileges than the statistics site did, such as flushing
  // cache.  But it also affects the syntax of the links created to sub-pages
  // in the top navigation bar.

  SystemServerContext(RewriteDriverFactory* factory,
                      StringPiece hostname, int port);
  virtual ~SystemServerContext();

  void SetCachePath(SystemCachePath* cache_path);

  // Implementations should call this method on every request, both for html and
  // resources, to avoid serving stale resources.
  //
  // TODO(jmarantz): allow a URL-based mechanism to flush cache, even if
  // we implement it by simply writing the cache.flush file so other
  // servers can see it.  Note that using shared-memory is not a great
  // plan because we need the cache-invalidation to persist across server
  // restart.
  void FlushCacheIfNecessary();

  SystemRewriteOptions* global_system_rewrite_options();
  GoogleString hostname_identifier() { return hostname_identifier_; }

  // Updates the PurgeSet with a new version.  This is called when the system
  // picks up (by polling or API) a new version of the cache.purge file.
  void UpdateCachePurgeSet(const CopyOnWrite<PurgeSet>& purge_set);

  // Initialize this SystemServerContext to set up its admin site.
  virtual void PostInitHook();

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
  //  - Custom fetch headers: before continuing with the fetch we want to add
  //    request headers.
  // Session fetchers allow us to make these decisions.  Here we may update
  // driver->async_fetcher() to be a special fetcher just for this request.
  virtual void ApplySessionFetchers(const RequestContextPtr& req,
                                    RewriteDriver* driver);

  // Accumulate in a histogram the amount of time spent rewriting HTML.
  // TODO(sligocki): Remove in favor of RewriteStats::rewrite_latency_histogram.
  void AddHtmlRewriteTimeUs(int64 rewrite_time_us);

  SystemCachePath* cache_path() { return cache_path_; }

  // Hook called after all configuration parsing is done to support implementers
  // like ApacheServerContext that need to collapse configuration inside the
  // config overlays into actual RewriteOptions objects.  It will also compute
  // signatures when done, and by default that's the only thing it does.
  virtual void CollapseConfigOverlaysAndComputeSignatures();

  // Handler which serves PSOL console.
  // Note: ConsoleHandler always succeeds.
  void ConsoleHandler(const SystemRewriteOptions& options,
                      AdminSite::AdminSource source,
                      const QueryParams& query_params, AsyncFetch* fetch);

  // Displays recent Info/Warning/Error messages.
  void MessageHistoryHandler(const RewriteOptions& options,
                             AdminSite::AdminSource source,
                             AsyncFetch* fetch);

  // Deprecated handler for graphs in the PSOL console.
  void StatisticsGraphsHandler(Writer* writer);

  // Handle a request for /pagespeed_admin/*, which is a launching
  // point for all the administrator pages including stats,
  // message-histogram, console, etc.
  void AdminPage(bool is_global, const GoogleUrl& stripped_gurl,
                 const QueryParams& query_params, const RewriteOptions* options,
                 AsyncFetch* fetch);

  // Handle a request for the legacy /*_pagespeed_statistics page, which also
  // serves as a launching point for a subset of the admin pages.  Because the
  // admin pages are not uniformly sensitive, an existing PageSpeed user might
  // have granted public access to /mod_pagespeed_statistics, but we don't
  // want that to automatically imply access to the server cache.
  void StatisticsPage(bool is_global, const QueryParams& query_params,
                      const RewriteOptions* options,
                      AsyncFetch* fetch);

  AdminSite* admin_site() { return admin_site_.get(); }

 protected:
  // Flush the cache by updating the cache flush timestamp in the global
  // options.  This will change its signature, which is part of the cache key,
  // and so all previously cached entries will be unreachable.
  //
  // Returns true if it actually updated the timestamp, false if the existing
  // cache flush timestamp was newer or the same as the one provided.
  //
  // Subclasses which add additional configurations need to override this method
  // to additionally update the cache flush timestamp in those other
  // configurations.
  virtual bool UpdateCacheFlushTimestampMs(int64 timestamp_ms);

  // Returns JSON used by the PageSpeed Console JavaScript.
  void ConsoleJsonHandler(const QueryParams& params, AsyncFetch* fetch);

  // Handler for /mod_pagespeed_statistics and
  // /ngx_pagespeed_statistics, as well as
  // /...pagespeed__global_statistics.  If the latter,
  // is_global_request should be true.
  void StatisticsHandler(const RewriteOptions& options, bool is_global_request,
                         AdminSite::AdminSource source, AsyncFetch* fetch);

  // Print details for configuration.
  void PrintConfig(AdminSite::AdminSource source, AsyncFetch* fetch);

  // Print statistics about the caches.  In the future this will also
  // be a launching point for examining cache entries and purging them.
  void PrintCaches(bool is_global, AdminSite::AdminSource source,
                   const GoogleUrl& stripped_gurl,
                   const QueryParams& query_params,
                   const RewriteOptions* options,
                   AsyncFetch* fetch);

  // Print histograms showing the dynamics of server activity.
  void PrintHistograms(bool is_global_request,
                       AdminSite::AdminSource source,
                       AsyncFetch* fetch);

  Variable* statistics_404_count();

 private:
  // Checks the timestamp of cache.flush, updating the purge context as
  // needed.
  //
  // TODO(jmarantz): Consider removing this if we decide to turn
  // enable_cache_purge always-on.  If we do that, "touch cache.flush"
  // will no longer work, and that will force users to use the new purge API
  // instead.  We could consider checking both cache.flush and cache.purge,
  // but it would be confusing what our semantics should be if both are
  // present.
  void CheckLegacyGlobalCacheFlushFile();

  scoped_ptr<AdminSite> admin_site_;

  bool initialized_;
  bool use_per_vhost_statistics_;

  // State used to implement periodic polling of $FILE_PREFIX/cache.flush.
  // last_cache_flush_check_sec_ is ctor-initialized to 0 so the first
  // time we Poll we will read the file.
  scoped_ptr<AbstractMutex> cache_flush_mutex_;
  int64 last_cache_flush_check_sec_;  // seconds since 1970

  Variable* cache_flush_count_;
  UpDownCounter* cache_flush_timestamp_ms_;

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

  SystemCaches* system_caches_;

  SystemCachePath* cache_path_;

  DISALLOW_COPY_AND_ASSIGN(SystemServerContext);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_SYSTEM_SERVER_CONTEXT_H_
