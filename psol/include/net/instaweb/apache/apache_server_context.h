// Copyright 2011 Google Inc.
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
// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_APACHE_APACHE_SERVER_CONTEXT_H_
#define NET_INSTAWEB_APACHE_APACHE_SERVER_CONTEXT_H_

#include "net/instaweb/apache/apache_config.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/system/public/system_server_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

struct server_rec;

namespace net_instaweb {

class ApacheRewriteDriverFactory;
class Histogram;
class ProxyFetchFactory;
class RewriteDriverPool;
class RewriteDriver;
class RewriteStats;
class SharedMemStatistics;
class Statistics;
class UrlAsyncFetcherStats;

// Creates an Apache-specific ServerContext.  This differs from base class
// that it incorporates by adding per-VirtualHost configuration, including:
//    - file-cache path & limits
//    - default RewriteOptions.
// Additionally, there are startup semantics for apache's prefork model
// that require a phased initialization.
class ApacheServerContext : public SystemServerContext {
 public:
  ApacheServerContext(ApacheRewriteDriverFactory* factory,
                      server_rec* server,
                      const StringPiece& version);
  virtual ~ApacheServerContext();

  GoogleString hostname_identifier() { return hostname_identifier_; }
  ApacheRewriteDriverFactory* apache_factory() { return apache_factory_; }
  ApacheConfig* config();
  bool InitFileCachePath();

  // These return configuration objects that hold settings from
  // <ModPagespeedIf spdy> and <ModPagespeedIf !spdy> sections of configuration.
  // They initialize lazily, so are not thread-safe; however they are only
  // meant to be used during configuration parsing. These methods should be
  // called only if there is actually a need to put something in them, since
  // otherwise we may end up constructing separate SPDY vs. non-SPDY
  // configurations needlessly.
  ApacheConfig* SpdyConfigOverlay();
  ApacheConfig* NonSpdyConfigOverlay();

  // These return true if the given overlays were constructed (in response
  // to having something in config files to put in them).
  bool has_spdy_config_overlay() const {
    return spdy_config_overlay_.get() != NULL;
  }

  bool has_non_spdy_config_overlay() const {
    return non_spdy_config_overlay_.get() != NULL;
  }

  // These two take ownership of their parameters.
  void set_spdy_config_overlay(ApacheConfig* x) {
    spdy_config_overlay_.reset(x);
  }

  void set_non_spdy_config_overlay(ApacheConfig* x) {
    non_spdy_config_overlay_.reset(x);
  }

  // Returns special configuration that should be used for SPDY sessions
  // instead of config(). Returns NULL if config() should be used instead.
  ApacheConfig* SpdyConfig() { return spdy_specific_config_.get(); }

  // Pool to pass to NewRewriteDriverFromPool to get a RewriteDriver configured
  // with SPDY-specific options. May be NULL in case there is no spdy-specific
  // configuration.
  RewriteDriverPool* spdy_driver_pool() { return spdy_driver_pool_; }

  // This should be called after all configuration parsing is done to collapse
  // configuration inside the config overlays into actual ApacheConfig objects.
  // It will also compute signatures when done.
  void CollapseConfigOverlaysAndComputeSignatures();

  // Initialize this ServerContext to have its own statistics domain.
  // Must be called after global_statistics has been created and had
  // ::Initialize called on it.
  void CreateLocalStatistics(Statistics* global_statistics);

  // Should be called after the child process is forked.
  void ChildInit();

  bool initialized() const { return initialized_; }

  // Called on notification from Apache on child exit. Returns true
  // if this is the last ServerContext that exists.
  bool PoolDestroyed();

  // Accumulate in a histogram the amount of time spent rewriting HTML.
  // TODO(sligocki): Remove in favor of RewriteStats::rewrite_latency_histogram.
  void AddHtmlRewriteTimeUs(int64 rewrite_time_us);

  static void InitStats(Statistics* statistics);

  const server_rec* server() const { return server_rec_; }

  virtual RewriteDriverPool* SelectDriverPool(bool using_spdy);

  virtual void ApplySessionFetchers(const RequestContextPtr& req,
                                    RewriteDriver* driver);

  ProxyFetchFactory* proxy_fetch_factory() {
    return proxy_fetch_factory_.get();
  }

  void InitProxyFetchFactory();

  // We do not proxy external HTML from mod_pagespeed in Apache using the
  // ProxyFetch flow.  Currently we must rely on a separate module to
  // let mod_pagespeed behave as an origin fetcher.
  virtual bool ProxiesHtml() const { return false; }

 private:
  virtual bool UpdateCacheFlushTimestampMs(int64 timestamp_ms);

  ApacheRewriteDriverFactory* apache_factory_;
  server_rec* server_rec_;
  GoogleString version_;

  // hostname_identifier_ equals to "server_hostname:port" of Apache,
  // it's used to distinguish the name of shared memory,
  // so that each vhost has its own SharedCircularBuffer.
  GoogleString hostname_identifier_;

  bool initialized_;

  // Non-NULL if we have per-vhost stats.
  scoped_ptr<Statistics> split_statistics_;

  // May be NULL. Owned by *split_statistics_.
  SharedMemStatistics* local_statistics_;

  // These are non-NULL if we have per-vhost stats.
  scoped_ptr<RewriteStats> local_rewrite_stats_;
  scoped_ptr<UrlAsyncFetcherStats> stats_fetcher_;

  // May be NULL. Constructed once we see things in config files that should
  // be stored in these.
  scoped_ptr<ApacheConfig> spdy_config_overlay_;
  scoped_ptr<ApacheConfig> non_spdy_config_overlay_;

  // May be NULL if we don't have any special settings for when using SPDY.
  scoped_ptr<ApacheConfig> spdy_specific_config_;

  // Owned by ServerContext via a call to ManageRewriteDriverPool.
  // May be NULL if we don't have a spdy-specific configuration.
  RewriteDriverPool* spdy_driver_pool_;

  Histogram* html_rewrite_time_us_histogram_;

  scoped_ptr<ProxyFetchFactory> proxy_fetch_factory_;

  DISALLOW_COPY_AND_ASSIGN(ApacheServerContext);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_SERVER_CONTEXT_H_
