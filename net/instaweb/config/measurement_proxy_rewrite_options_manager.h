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
// This looks at URLs in syntax of MeasurementProxyUrlNamer, and produces
// a configuration appropriate for them, including whether:
// 1) The URL should be served at all (password, syntax correctness)
// 2) There should be any rewriting happening, given site and resource
//    domains.
// 3) The rewriting should be blocking.


#ifndef NET_INSTAWEB_CONFIG_MEASUREMENT_PROXY_REWRITE_OPTIONS_MANAGER_H_
#define NET_INSTAWEB_CONFIG_MEASUREMENT_PROXY_REWRITE_OPTIONS_MANAGER_H_

#include "net/instaweb/config/rewrite_options_manager.h"

#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/request_headers.h"

namespace net_instaweb {

class ServerContext;

class MeasurementProxyRewriteOptionsManager : public RewriteOptionsManager {
 public:
  MeasurementProxyRewriteOptionsManager(const ServerContext* server_context,
                                        const GoogleString& root_domain,
                                        const GoogleString& password);
  ~MeasurementProxyRewriteOptionsManager() override {}

  void GetRewriteOptions(const GoogleUrl& url,
                         const RequestHeaders& headers,
                         OptionsCallback* done) override;

 private:
  void Force403(RewriteOptions* options);
  void ApplyConfig(const GoogleUrl& decoded_url, StringPiece config,
                   StringPiece config_domain, RewriteOptions* options);

  const ServerContext* server_context_;
  GoogleString root_domain_;
  GoogleString password_;

  DISALLOW_COPY_AND_ASSIGN(MeasurementProxyRewriteOptionsManager);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_CONFIG_MEASUREMENT_PROXY_REWRITE_OPTIONS_MANAGER_H_
