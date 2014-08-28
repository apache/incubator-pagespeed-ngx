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

#include "net/instaweb/apache/apache_server_context.h"

#include "httpd.h"                  // NOLINT
#include "http_protocol.h"          // NOLINT
#include "base/logging.h"
#include "net/instaweb/apache/apache_config.h"
#include "net/instaweb/apache/apache_request_context.h"
#include "net/instaweb/apache/apache_rewrite_driver_factory.h"
#include "net/instaweb/apache/mod_spdy_fetcher.h"
#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_pool.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

class RewriteOptions;

namespace {

class SpdyOptionsRewriteDriverPool : public RewriteDriverPool {
 public:
  explicit SpdyOptionsRewriteDriverPool(ApacheServerContext* context)
      : apache_server_context_(context) {
  }

  virtual const RewriteOptions* TargetOptions() const {
    DCHECK(apache_server_context_->SpdyGlobalConfig() != NULL);
    return apache_server_context_->SpdyGlobalConfig();
  }

 private:
  ApacheServerContext* apache_server_context_;
};

}  // namespace

ApacheServerContext::ApacheServerContext(
    ApacheRewriteDriverFactory* factory,
    server_rec* server,
    const StringPiece& version)
    : SystemServerContext(factory, server->server_hostname, server->port),
      apache_factory_(factory),
      server_rec_(server),
      version_(version.data(), version.size()),
      spdy_driver_pool_(NULL) {
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
  SystemServerContext::InitStats(statistics);
  ModSpdyFetcher::InitStats(statistics);
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

ApacheConfig* ApacheServerContext::SpdyConfigOverlay() {
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
  if (spdy_config_overlay_.get() != NULL ||
      non_spdy_config_overlay_.get() != NULL) {
    // We need separate SPDY/non-SPDY configs if we have any
    // <IfModpagespeed spdy> or <IfModpagespeed !spdy> blocks.
    // We compute the SPDY one first since we need global_config() to be
    // the common config and not common + !spdy.
    spdy_specific_config_.reset(global_config()->Clone());
    spdy_specific_config_->set_cache_invalidation_timestamp_mutex(
        thread_system()->NewRWLock());
    if (spdy_config_overlay_.get() != NULL) {
      spdy_specific_config_->Merge(*spdy_config_overlay_);
    }
    ComputeSignature(spdy_specific_config_.get());
  }

  if (non_spdy_config_overlay_.get() != NULL) {
    global_config()->Merge(*non_spdy_config_overlay_);
  }

  SystemServerContext::CollapseConfigOverlaysAndComputeSignatures();

  if (spdy_specific_config_.get() != NULL) {
    spdy_driver_pool_ = new SpdyOptionsRewriteDriverPool(this);
    ManageRewriteDriverPool(spdy_driver_pool_);
  }
}

bool ApacheServerContext::PoolDestroyed() {
  ShutDownDrivers();
  return apache_factory_->PoolDestroyed(this);
}

bool ApacheServerContext::UpdateCacheFlushTimestampMs(int64 timestamp_ms) {
  bool flushed = SystemServerContext::UpdateCacheFlushTimestampMs(timestamp_ms);
  if (spdy_specific_config_.get() != NULL) {
    // We need to make sure to update the invalidation timestamp in the
    // SPDY configuration as well, so it also gets any cache flushes.
    flushed |=
        spdy_specific_config_->UpdateCacheInvalidationTimestampMs(timestamp_ms);
  }
  return flushed;
}

RewriteDriverPool* ApacheServerContext::SelectDriverPool(bool using_spdy) {
  if (using_spdy && (SpdyGlobalConfig() != NULL)) {
    return spdy_driver_pool();
  }
  return standard_rewrite_driver_pool();
}

void ApacheServerContext::MaybeApplySpdySessionFetcher(
    const RequestContextPtr& request, RewriteDriver* driver) {
  const ApacheConfig* conf = ApacheConfig::DynamicCast(driver->options());
  CHECK(conf != NULL);
  ApacheRequestContext* apache_request = ApacheRequestContext::DynamicCast(
      request.get());

  // This should have already been caught by the caller.
  CHECK(apache_request != NULL);

  if (conf->fetch_from_mod_spdy() &&
      apache_request->use_spdy_fetcher()) {
    driver->SetSessionFetcher(new ModSpdyFetcher(
        apache_factory_->mod_spdy_fetch_controller(), apache_request->url(),
        driver, apache_request->spdy_connection_factory()));
  }
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

}  // namespace net_instaweb
