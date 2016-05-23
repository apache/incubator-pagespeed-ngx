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

// Author: matterbury@google.com (Matt Atterbury),
//         morlovich@google.com (Maksim Orlovich)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_MEASUREMENT_PROXY_URL_NAMER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_MEASUREMENT_PROXY_URL_NAMER_H_

#include "net/instaweb/rewriter/public/url_namer.h"

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class GoogleUrl;
class RewriteOptions;

// Implements a naming scheme that lets a proxy handle multiple domains.
// Suitable only for measurements/experiments, not regular use, as normally
// this would break the entire same origin model. This also assumes that there
// is something altering every request from the page to use our encoding
// before fetching via us (as this doesn't implement Encode() itself).
class MeasurementProxyUrlNamer : public UrlNamer {
 public:
  MeasurementProxyUrlNamer(const GoogleString& top_origin,
                           const GoogleString& password);
  ~MeasurementProxyUrlNamer() override;

  bool Decode(const GoogleUrl& request_url,
              const RewriteOptions* rewrite_options,
              GoogleString* decoded) const override;

  static bool DecodePathDetails(const GoogleUrl& request_url,
                                StringPiece* config,
                                StringPiece* config_domain,
                                StringPiece* password,
                                GoogleString* res_url);

  bool IsAuthorized(const GoogleUrl& request_url,
                    const RewriteOptions& options) const override {
    // We want to fetch everything.
    return true;
  }

  ProxyExtent ProxyMode() const override { return ProxyExtent::kInputOnly; }
  bool IsProxyEncoded(const GoogleUrl& url) const override;

 private:
  GoogleString top_origin_;
  GoogleString password_;

  DISALLOW_COPY_AND_ASSIGN(MeasurementProxyUrlNamer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_MEASUREMENT_PROXY_URL_NAMER_H_
