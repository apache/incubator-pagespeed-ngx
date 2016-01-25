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
// Author: xqyin@google.com (XiaoQian Yin)

#ifndef PAGESPEED_SYSTEM_ADMIN_SITE_H_
#define PAGESPEED_SYSTEM_ADMIN_SITE_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class AsyncFetch;
class CacheInterface;
class GoogleUrl;
class HTTPCache;
class MessageHandler;
class PropertyCache;
class QueryParams;
class RewriteOptions;
class ServerContext;
class StaticAssetManager;
class Statistics;
class SystemCachePath;
class SystemCaches;
class SystemRewriteOptions;
class Timer;
class Writer;

// Implements the /pagespeed_admin pages.
class AdminSite {
 public:
  // Identifies whether the user arrived at an admin page from a
  // /pagespeed_admin handler or a /*_pagespeed_statistics handler.
  // The main difference between these is that the _admin site might in the
  // future grant more privileges than the statistics site did, such as flushing
  // cache.  But it also affects the syntax of the links created to sub-pages
  // in the top navigation bar.
  enum AdminSource { kPageSpeedAdmin, kStatistics, kOther };

  AdminSite(StaticAssetManager* static_asset_manager, Timer* timer,
            MessageHandler* message_handler);

  ~AdminSite() {
  }

  // Handler which serves PSOL console.
  // Note: ConsoleHandler always succeeds.
  void ConsoleHandler(const SystemRewriteOptions& global_options,
                      const RewriteOptions& options, AdminSource source,
                      const QueryParams& query_params, AsyncFetch* fetch,
                      Statistics* statistics);

  // Displays recent Info/Warning/Error messages.: public ServerContext
  void MessageHistoryHandler(const RewriteOptions& options,
                             AdminSource source, AsyncFetch* fetch);

  // Deprecated handler for graphs in the PSOL console.
  void StatisticsGraphsHandler(
      Writer* writer, SystemRewriteOptions* global_system_rewrite_options);

  // Handle a request for /pagespeed_admin/*, which is a launching
  // point for all the administrator pages including stats,
  // message-histogram, console, etc.
  void AdminPage(bool is_global, const GoogleUrl& stripped_gurl,
                 const QueryParams& query_params,
                 const RewriteOptions* options,
                 SystemCachePath* cache_path,
                 AsyncFetch* fetch, SystemCaches* system_caches,
                 CacheInterface* filesystem_metadata_cache,
                 HTTPCache* http_cache, CacheInterface* metadata_cache,
                 PropertyCache* page_property_cache,
                 ServerContext* server_context, Statistics* statistics,
                 Statistics* stats,
                 SystemRewriteOptions* global_system_rewrite_options);

  // Handle a request for the legacy /*_pagespeed_statistics page, which also
  // serves as a launching point for a subset of the admin pages.  Because the
  // admin pages are not uniformly sensitive, an existing PageSpeed user might
  // have granted public access to /mod_pagespeed_statistics, but we don't
  // want that to automatically imply access to the server cache.
  void StatisticsPage(bool is_global, const QueryParams& query_params,
                      const RewriteOptions* options, AsyncFetch* fetch,
                      SystemCaches* system_caches,
                      CacheInterface* filesystem_metadata_cache,
                      HTTPCache* http_cache, CacheInterface* metadata_cache,
                      PropertyCache* page_property_cache,
                      ServerContext* server_context, Statistics* statistics,
                      Statistics* stats,
                      SystemRewriteOptions* global_system_rewrite_options);

  // Returns JSON used by the PageSpeed Console JavaScript.
  void ConsoleJsonHandler(const QueryParams& params, AsyncFetch* fetch,
                          Statistics* statistics);

  // Handler for /mod_pagespeed_statistics and
  // /ngx_pagespeed_statistics, as well as
  // /...pagespeed__global_statistics.  If the latter,
  // is_global_request should be true.
  void StatisticsHandler(const RewriteOptions& options, AdminSource source,
                         AsyncFetch* fetch, Statistics* stats);

  // Responds to 'fetch' with data used on statistics page and graphs page
  // in JSON format.
  void StatisticsJsonHandler(AsyncFetch* fetch, Statistics* stats);

  // Display various charts on graphs page.
  // TODO(xqyin): Integrate this into console page.
  void GraphsHandler(const RewriteOptions& options, AdminSource source,
                     const QueryParams& query_params, AsyncFetch* fetch,
                     Statistics* stats);;

  // Print details for configuration.
  void PrintConfig(AdminSource source, AsyncFetch* fetch,
                   SystemRewriteOptions* global_system_rewrite_options);

  // Print statistics about the caches.  In the future this will also
  // be a launching point for examining cache entries and purging them.
  void PrintCaches(bool is_global, AdminSource source,
                   const GoogleUrl& stripped_gurl,
                   const QueryParams& query_params,
                   const RewriteOptions* options,
                   SystemCachePath* cache_path,
                   AsyncFetch* fetch, SystemCaches* system_caches,
                   CacheInterface* filesystem_metadata_cache,
                   HTTPCache* http_cache, CacheInterface* metadata_cache,
                   PropertyCache* page_property_cache,
                   ServerContext* server_context);

  // Print histograms showing the dynamics of server activity.
  void PrintHistograms(AdminSource source, AsyncFetch* fetch,
                       Statistics* stats);

  void PurgeHandler(StringPiece url, SystemCachePath* cache_path,
                    AsyncFetch* fetch);

  // Return the message handler for debugging use.
  MessageHandler* MessageHandlerForTesting() { return message_handler_; }

 private:
  MessageHandler* message_handler_;
  StaticAssetManager* static_asset_manager_;
  Timer* timer_;
  DISALLOW_COPY_AND_ASSIGN(AdminSite);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_ADMIN_SITE_H_
