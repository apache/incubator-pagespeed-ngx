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

// Authors: mmohabey@google.com (Megha Mohabey)
//          pulkitg@google.com (Pulkit Goyal)

#ifndef NET_INSTAWEB_AUTOMATIC_PUBLIC_CACHE_HTML_FLOW_H_
#define NET_INSTAWEB_AUTOMATIC_PUBLIC_CACHE_HTML_FLOW_H_

#include "net/instaweb/rewriter/cache_html_info.pb.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class AbstractLogRecord;
class AsyncFetch;
class FallbackPropertyPage;
class MessageHandler;
class PropertyPage;
class ProxyFetchPropertyCallbackCollector;
class ProxyFetchFactory;
class ServerContext;
class RewriteOptions;
class RewriteDriver;
class Statistics;
class TimedVariable;

// CacheHtmlFlow manages the flow for an html request where we can flush a
// cached html to the client before receiving a response from the origin server.
// In order to flush the html early before we start getting bytes back from the
// fetcher, we lookup property cache for CacheHtmlInfo. If found, we flush
// cached html out (with the non cacheable parts removed) and then trigger the
// normal ProxyFetch flow which extracts cookies and non cacheable parts from
// the page and sends it out. If CacheHtmlInfo is not found in cache, we pass
// this request through normal ProxyFetch flow buffering the html. In the
// background we create a driver to parse it, remove the non-cacheable parts,
// compute CacheHtmlInfo and store it into the property cache.
class CacheHtmlFlow {
 public:
  class LogHelper;

  // Identifies the sync-point for reproducing races between foreground
  // serving request and background cache html computation requests in tests.
  static const char kBackgroundComputationDone[];

  static void Start(const GoogleString& url,
                    AsyncFetch* base_fetch,
                    RewriteDriver* driver,
                    ProxyFetchFactory* factory,
                    ProxyFetchPropertyCallbackCollector* property_callback);

  virtual ~CacheHtmlFlow();

  static void InitStats(Statistics* statistics);

  static const char kNumCacheHtmlHits[];
  static const char kNumCacheHtmlMisses[];
  static const char kNumCacheHtmlMatches[];
  static const char kNumCacheHtmlMismatches[];
  static const char kNumCacheHtmlMismatchesCacheDeletes[];
  static const char kNumCacheHtmlSmartdiffMatches[];
  static const char kNumCacheHtmlSmartdiffMismatches[];

 private:
  CacheHtmlFlow(const GoogleString& url,
                AsyncFetch* base_fetch,
                RewriteDriver* driver,
                ProxyFetchFactory* factory,
                ProxyFetchPropertyCallbackCollector* property_callback);

  void CacheHtmlLookupDone();

  void Cancel();

  // Callback that is invoked after we rewrite the cached html.
  void CacheHtmlRewriteDone(bool flushed_split_js);

  // Serves the cached html content to the client and triggers the proxy fetch
  // for non cacheable content.
  // TODO(pulkitg): Change the function GetHtmlCriticalImages to take
  // AbstractPropertyPage so that dependency on FallbackPropertyPage can be
  // removed.
  void CacheHtmlHit(FallbackPropertyPage* page);

  // Serves the request in passthru mode and triggers a background request to
  // compute CacheHtmlInfo.
  void CacheHtmlMiss();

  // Triggers proxy fetch.
  void TriggerProxyFetch();

  // Populates the cache html info from the property cache to cache_html_info_.
  // It also determines whether this info is stale or not.
  void PopulateCacheHtmlInfo(PropertyPage* page);

  GoogleString url_;
  GoogleUrl google_url_;
  AsyncFetch* base_fetch_;
  // Cache Html Flow needs its own log record since it needs to log even after
  // the main log record is written out when the request processing is finished.
  scoped_ptr<AbstractLogRecord> cache_html_log_record_;
  RewriteDriver* rewrite_driver_;
  const RewriteOptions* options_;
  ProxyFetchFactory* factory_;
  ServerContext* server_context_;
  ProxyFetchPropertyCallbackCollector* property_cache_callback_;
  MessageHandler* handler_;
  CacheHtmlInfo cache_html_info_;
  scoped_ptr<LogHelper> cache_html_log_helper_;

  TimedVariable* num_cache_html_misses_;
  TimedVariable* num_cache_html_hits_;

  DISALLOW_COPY_AND_ASSIGN(CacheHtmlFlow);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_AUTOMATIC_PUBLIC_CACHE_HTML_FLOW_H_
