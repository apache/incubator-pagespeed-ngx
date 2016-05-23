// Copyright 2016 Google Inc.
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

// Author: matterbury@google.com (Matt Atterbury),
//         morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/rewriter/public/measurement_proxy_url_namer.h"

#include <cstddef>                     // for size_t

#include "base/logging.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {


MeasurementProxyUrlNamer::MeasurementProxyUrlNamer(
    const GoogleString& top_origin, const GoogleString& password)
    : top_origin_(top_origin), password_(password) {
  if (StringPiece(top_origin_).ends_with("/")) {
    top_origin_.resize(top_origin.size() - 1);
  }
}

MeasurementProxyUrlNamer::~MeasurementProxyUrlNamer() {
}

bool MeasurementProxyUrlNamer::Decode(
    const GoogleUrl& request_url, const RewriteOptions*,
    GoogleString* decoded) const {
  StringPiece config, config_domain, password;
  return DecodePathDetails(request_url, &config, &config_domain,
                           &password, decoded);
}

// Naming scheme:
// For cross-domain resources:
// https://top_domain_/code/config/password/config_host/resource_host/path
// For same-domain resources:
// https://top_domain_/code/config/password/resource_host/path
//
// Where code is:
//   h: same-domain http
//   x: cross-domain https
//   s: same-domain https
//   t: cross-domain https
bool MeasurementProxyUrlNamer::DecodePathDetails(
    const GoogleUrl& request_url,
    StringPiece* config,
    StringPiece* config_domain,
    StringPiece* password,
    GoogleString* res_url) {
  StringPiece request_path = request_url.PathSansLeaf();
  StringPieceVector path_vector;
  SplitStringPieceToVector(request_path, "/", &path_vector, false);

  // The leading slash results in path_vector[0] being "", and all the
  // other indices into it being one more than one would think.
  DCHECK(path_vector.empty() || path_vector[0] == "");

  if (path_vector.size() < 5) {
    return false;
  }

  StringPiece code = path_vector[1];
  *config = path_vector[2];
  *password = path_vector[3];

  StringPiece res_schema = "http";
  if (code == "s" || code == "t") {
    res_schema = "https";
  }

  size_t site_path_start;
  StringPiece res_domain;
  if (code == "h" || code == "s") {
    // Same domain.
    *config_domain = res_domain = path_vector[4];
    site_path_start = 5;
  } else if (code == "x" || code == "t") {
    if (path_vector.size() < 6) {
      return false;
    }
    *config_domain = path_vector[4];
    res_domain = path_vector[5];
    site_path_start = 6;
  } else {
    return false;
  }

  if (config_domain->empty() || res_domain.empty()) {
    return false;
  }

  *res_url = StrCat(res_schema, "://", res_domain);
  // There is also an empty fragment after the last directory, hence n
  // being one less than the size here.
  for (size_t i = site_path_start, n = path_vector.size() - 1; i < n; ++i) {
    StrAppend(res_url, "/", path_vector[i]);
  }
  StrAppend(res_url, "/", request_url.LeafWithQuery());
  return true;
}

bool MeasurementProxyUrlNamer::IsProxyEncoded(const GoogleUrl& url) const {
  StringPiece config, config_domain, password;
  GoogleString decoded;
  if (DecodePathDetails(url, &config, &config_domain, &password, &decoded)) {
    // Looks like the right syntax, but check to see if it's actually on our
    // host and not elsewhere.
    return (password == password_ && url.Origin() == top_origin_);
  } else {
    return false;
  }
}

}  // namespace net_instaweb
