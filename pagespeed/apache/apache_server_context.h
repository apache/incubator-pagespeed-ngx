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

#ifndef PAGESPEED_APACHE_APACHE_SERVER_CONTEXT_H_
#define PAGESPEED_APACHE_APACHE_SERVER_CONTEXT_H_

#include <memory>

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "pagespeed/apache/apache_config.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/system/system_server_context.h"

struct request_rec;
struct server_rec;

namespace net_instaweb {

class ApacheRewriteDriverFactory;
class ApacheRequestContext;
class MeasurementProxyUrlNamer;
class ProxyFetchFactory;
class RewriteDriverPool;
class RewriteDriver;
class Statistics;
class Variable;

// Creates an Apache-specific ServerContext.  This differs from base class
// that it incorporates by adding per-VirtualHost configuration, including:
//    - file-cache path & limits
//    - default RewriteOptions.
// Additionally, there are startup semantics for apache's prefork model
// that require a phased initialization.
class ApacheServerContext : public SystemServerContext {
 public:
  // Prefix for ProxyInterface stats (active in proxy_all_requests_mode() only).
  static const char kProxyInterfaceStatsPrefix[];

  ApacheServerContext(ApacheRewriteDriverFactory* factory,
                      server_rec* server,
                      const StringPiece& version);
  virtual ~ApacheServerContext();

  // This must be called for every statistics object in use before using this.
  static void InitStats(Statistics* statistics);

  ApacheRewriteDriverFactory* apache_factory() { return apache_factory_; }
  ApacheConfig* global_config();
  const ApacheConfig* global_config() const;
  bool InitPath(const GoogleString& path);

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

  // This should be called after all configuration parsing is done to collapse
  // configuration inside the config overlays into actual ApacheConfig objects.
  // It will also compute signatures when done.
  virtual void CollapseConfigOverlaysAndComputeSignatures();

  // Called on notification from Apache on child exit. Returns true
  // if this is the last ServerContext that exists.
  bool PoolDestroyed();

  const server_rec* server() const { return server_rec_; }

  ProxyFetchFactory* proxy_fetch_factory() {
    return proxy_fetch_factory_.get();
  }

  void InitProxyFetchFactory();

  // We only proxy external HTML from mod_pagespeed in Apache using the
  // ProxyFetch flow if proxy_all_requests_mode() is on in config.  In the usual
  // case, we handle HTML as an Apache filter, letting something like mod_proxy
  // (or one of our own test modes like slurp) do the fetching.
  virtual bool ProxiesHtml() const {
    return global_config()->proxy_all_requests_mode();
  }

  ApacheRequestContext* NewApacheRequestContext(request_rec* request);

  // Reports an error status to the HTTP resource request, and logs
  // the error as a Warning to the log file, and bumps a stat as
  // needed.
  void ReportResourceNotFound(StringPiece message, request_rec* request) {
    ReportNotFoundHelper(kWarning, message, request,
                         rewrite_stats()->resource_404_count());
  }

  // Reports an error status to the HTTP statistics request, and logs
  // the error as a Warning to the log file, and bumps a stat as
  // needed.
  void ReportStatisticsNotFound(StringPiece message, request_rec* request) {
    ReportNotFoundHelper(kWarning, message, request, statistics_404_count());
  }

  // Reports an error status to the HTTP slurp request, and logs
  // the error as a Warning to the log file, and bumps a stat as
  // needed.
  void ReportSlurpNotFound(StringPiece message, request_rec* request) {
    ReportNotFoundHelper(kInfo, message, request,
                         rewrite_stats()->slurp_404_count());
  }

  virtual GoogleString FormatOption(StringPiece option_name, StringPiece args);

 private:
  void ChildInit(SystemRewriteDriverFactory* factory) override;

  void ReportNotFoundHelper(MessageType message_type,
                            StringPiece url,
                            request_rec* request,
                            Variable* error_count);

  ApacheRewriteDriverFactory* apache_factory_;
  server_rec* server_rec_;
  GoogleString version_;

  // May be NULL. Constructed once we see things in config files that should
  // be stored in these.
  scoped_ptr<ApacheConfig> spdy_config_overlay_;
  scoped_ptr<ApacheConfig> non_spdy_config_overlay_;

  // May be NULL. Only constructed in measurement proxy mode.
  std::unique_ptr<MeasurementProxyUrlNamer> measurement_url_namer_;

  scoped_ptr<ProxyFetchFactory> proxy_fetch_factory_;

  DISALLOW_COPY_AND_ASSIGN(ApacheServerContext);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_APACHE_APACHE_SERVER_CONTEXT_H_
