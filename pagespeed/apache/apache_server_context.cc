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

#include "pagespeed/apache/apache_server_context.h"

// http_protocol.h includes httpd.h. We need to include httpd_includes.h, which
// works around a conflicting definition of OK in gRPC.
#include "pagespeed/apache/apache_httpd_includes.h"
#include "http_protocol.h"          // NOLINT
#include "pagespeed/apache/apache_config.h"
#include "pagespeed/apache/apache_request_context.h"
#include "pagespeed/apache/apache_rewrite_driver_factory.h"
#include "pagespeed/automatic/proxy_fetch.h"
#include "pagespeed/automatic/proxy_interface.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/http/http_names.h"
#include "net/instaweb/config/measurement_proxy_rewrite_options_manager.h"
#include "net/instaweb/rewriter/public/measurement_proxy_url_namer.h"

namespace net_instaweb {

const char ApacheServerContext::kProxyInterfaceStatsPrefix[] =
    "proxy-all-mode-";

ApacheServerContext::ApacheServerContext(
    ApacheRewriteDriverFactory* factory,
    server_rec* server,
    const StringPiece& version)
    : SystemServerContext(factory, server->server_hostname, server->port),
      apache_factory_(factory),
      server_rec_(server),
      version_(version.data(), version.size()) {
  // We may need the message handler for error messages very early, before
  // we get to InitServerContext in ChildInit().
  set_message_handler(apache_factory_->message_handler());

  // Currently, mod_pagespeed always runs upstream of mod_headers when used as
  // an origin server.  Note that in a proxy application, this might not be the
  // case.  I'm not sure how we can detect this on a per-request basis so
  // that might require a small refactor.
  //
  // TODO(jmarantz): We'd like to change this for various reasons but are unsure
  // of the impact.
  set_response_headers_finalized(false);
}

ApacheServerContext::~ApacheServerContext() {
}

void ApacheServerContext::InitStats(Statistics* statistics) {
  ProxyInterface::InitStats(kProxyInterfaceStatsPrefix, statistics);
  SystemServerContext::InitStats(statistics);
}

bool ApacheServerContext::InitPath(const GoogleString& path) {
  if (file_system()->IsDir(path.c_str(), message_handler()).is_true()) {
    return true;
  }
  bool ok = file_system()->RecursivelyMakeDir(path, message_handler());
  if (ok) {
    apache_factory_->AddCreatedDirectory(path);
  }
  return ok;
}

ApacheConfig* ApacheServerContext::global_config() {
  return ApacheConfig::DynamicCast(global_options());
}

const ApacheConfig* ApacheServerContext::global_config() const {
  return ApacheConfig::DynamicCast(global_options());
}

ApacheConfig* ApacheServerContext::SpdyConfigOverlay() {
  // While we no longer actually use the spdy config overlay, it's still
  // useful for backwards compatibility during parsing.
  if (spdy_config_overlay_.get() == NULL) {
    spdy_config_overlay_.reset(new ApacheConfig(
        "spdy_overlay", thread_system()));
    // We want to copy any implicit rewrite level from the parent,
    // so we don't end up overriding it with passthrough. It's also OK
    // to forward explicit one to an implicit one here, since an implicit
    // will never override an explicit one (even if its different).
    spdy_config_overlay_->SetDefaultRewriteLevel(global_config()->level());
  }
  return spdy_config_overlay_.get();
}

ApacheConfig* ApacheServerContext::NonSpdyConfigOverlay() {
  if (non_spdy_config_overlay_.get() == NULL) {
    non_spdy_config_overlay_.reset(new ApacheConfig(
        "non_spdy_overlay", thread_system()));
    // See ::SpdyConfigOverlay for explanation.
    non_spdy_config_overlay_->SetDefaultRewriteLevel(global_config()->level());
  }
  return non_spdy_config_overlay_.get();
}

void ApacheServerContext::CollapseConfigOverlaysAndComputeSignatures() {
  // These days we ignore the spdy overlay and merge-in the non-spdy one
  // unconditionally.
  if (non_spdy_config_overlay_.get() != NULL) {
    global_config()->Merge(*non_spdy_config_overlay_);
  }

  SystemServerContext::CollapseConfigOverlaysAndComputeSignatures();

  spdy_config_overlay_.reset();
  non_spdy_config_overlay_.reset();
}

bool ApacheServerContext::PoolDestroyed() {
  DCHECK_EQ(num_active_rewrite_drivers(), 0);
  return apache_factory_->PoolDestroyed(this);
}

void ApacheServerContext::InitProxyFetchFactory() {
  proxy_fetch_factory_.reset(new ProxyFetchFactory(this));
}

ApacheRequestContext* ApacheServerContext::NewApacheRequestContext(
    request_rec* request) {
  return new ApacheRequestContext(
      thread_system()->NewMutex(),
      timer(),
      request);
}

void ApacheServerContext::ReportNotFoundHelper(MessageType message_type,
                                               StringPiece error_message,
                                               request_rec* request,
                                               Variable* error_count) {
  error_count->Add(1);
  request->status = HttpStatus::kNotFound;
  ap_send_error_response(request, 0);
  message_handler()->Message(message_type, "%s %s: not found (404)",
                             (error_message.empty() ? "(null)" :
                              error_message.as_string().c_str()),
                             error_count->GetName().as_string().c_str());
}

GoogleString ApacheServerContext::FormatOption(StringPiece option_name,
                                               StringPiece args) {
  return StrCat("ModPagespeed", option_name, " ", args);
}

void ApacheServerContext::ChildInit(SystemRewriteDriverFactory* f) {
  if (global_config()->proxy_all_requests_mode()) {
    apache_factory_->SetNeedSchedulerThread();
    if (global_config()->measurement_proxy_mode()) {
      measurement_url_namer_.reset(new MeasurementProxyUrlNamer(
          global_config()->measurement_proxy_root(),
          global_config()->measurement_proxy_password()));
      set_url_namer(measurement_url_namer_.get());
      SetRewriteOptionsManager(new MeasurementProxyRewriteOptionsManager(
          this,
          global_config()->measurement_proxy_root(),
          global_config()->measurement_proxy_password()));
    }
  }
  SystemServerContext::ChildInit(f);
}

}  // namespace net_instaweb
