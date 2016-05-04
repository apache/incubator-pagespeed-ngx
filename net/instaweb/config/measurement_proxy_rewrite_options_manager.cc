/*
 * Copyright 2016 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)
//
//
// This looks at URLs in syntax of MeasurementProxyUrlNamer, and produces
// a configuration appropriate for them, including whether:
// 1) The URL should be served at all (password, syntax correctness)
// 2) There should be any rewriting happening, given site and resource
//    domains.
// 3) The rewriting should be blocking.


#include "net/instaweb/config/measurement_proxy_rewrite_options_manager.h"

#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/measurement_proxy_url_namer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/domain_registry.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/request_headers.h"

namespace net_instaweb {

MeasurementProxyRewriteOptionsManager::MeasurementProxyRewriteOptionsManager(
    const ServerContext* server_context,
    const GoogleString& root_domain,
    const GoogleString& password)
    : server_context_(server_context), root_domain_(root_domain),
      password_(password) {
}

void MeasurementProxyRewriteOptionsManager::GetRewriteOptions(
    const GoogleUrl& url,
    const RequestHeaders& headers,
    OptionsCallback* done) {
  RewriteOptions* options = server_context_->global_options()->Clone();

  StringPiece config, config_domain, password;
  GoogleString res_url;
  if (url.Origin() != root_domain_) {
    Force403(options);
  } else if (MeasurementProxyUrlNamer::DecodePathDetails(
                 url, &config, &config_domain, &password, &res_url)) {
    if (password != password_) {
      Force403(options);
    } else {
      GoogleUrl decoded_url(res_url);
      ApplyConfig(decoded_url, config, config_domain, options);
    }
  } else {
    Force403(options);
  }
  done->Run(options);
}

void MeasurementProxyRewriteOptionsManager::Force403(RewriteOptions* options) {
  options->set_reject_blacklisted(true);
  options->Disallow("*");
}

void MeasurementProxyRewriteOptionsManager::ApplyConfig(
    const GoogleUrl& decoded_url, StringPiece config,
    StringPiece config_domain, RewriteOptions* options) {
  if (!decoded_url.IsWebValid()) {
    Force403(options);
    return;
  }

  // We don't have config spec yet, so this will declare that 'b' means
  // blocking, and everything else will get ignored.
  if (config == "b") {
    options->set_rewrite_deadline_ms(-1);
    options->set_in_place_wait_for_optimized(true);
    options->set_in_place_rewrite_deadline_ms(-1);
  }

  // Setup permissions to only rewrite things related to the config_domain.
  // Note that this not meant to be a security measure; we are just making
  // a guess as to what resources are likely to be also optimizable by the
  // owner of the config_domain site.
  GoogleString config_domain_suffix =
      domain_registry::MinimalPrivateSuffix(config_domain).as_string();
  GoogleString actual_domain_suffix =
      domain_registry::MinimalPrivateSuffix(decoded_url.Host()).as_string();
  LowerString(&config_domain_suffix);
  LowerString(&actual_domain_suffix);
  if (config_domain_suffix == actual_domain_suffix) {
    options->WriteableDomainLawyer()->AddDomain(
        StrCat("http*://", config_domain_suffix),
               server_context_->message_handler());
    options->WriteableDomainLawyer()->AddDomain(
        StrCat("http*://*.", config_domain_suffix),
        server_context_->message_handler());
  } else {
    // Note: ProxyFetch gets paranoid about disallow * when a namer is in
    // use, so we just turn off all the filters instead.
    options->SetRewriteLevel(RewriteOptions::kPassThrough);
  }
}

}  // namespace net_instaweb
