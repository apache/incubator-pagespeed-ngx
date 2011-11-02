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

// Author: matterbury@google.com (Matt Atterbury)

#include "net/instaweb/rewriter/public/test_url_namer.h"

#include <cstddef>                     // for size_t

#include "base/logging.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_util.h"        // for StringPiece

namespace net_instaweb {

const char kTopDomain[] = "http://cdn.com";

bool TestUrlNamer::use_normal_encoding_ = false;

TestUrlNamer::TestUrlNamer() {
  use_normal_encoding_ = false;  // reset whenever a new one is constructed.
}

TestUrlNamer::~TestUrlNamer() {
}

GoogleString TestUrlNamer::Encode(const RewriteOptions* rewrite_options,
                                  const OutputResource& output_resource) const {
  // Some test requires us to use normal encoding, so off we go!
  if (use_normal_encoding_) {
    return UrlNamer::Encode(rewrite_options, output_resource);
  }

  DCHECK(rewrite_options != NULL);
  GoogleUrl base_gurl(output_resource.resolved_base());

  // If there is any sharding or rewriting enabled then various tests don't
  // work if we rewrite the domain or path, so in that case revert to normal.
  if (rewrite_options->domain_lawyer()->can_rewrite_domains()) {
    return UrlNamer::Encode(rewrite_options, output_resource);
  }

  // TEST only handles http/https schemes, so bail if it's anything else.
  if (base_gurl.Scheme() != "http" && base_gurl.Scheme() != "https") {
    return UrlNamer::Encode(rewrite_options, output_resource);
  }

  // The base might already be the encoded, such as when the resource is
  // created from a cached output by RewriteContext, and we must not encode
  // an already encoded URL.
  if (IsPathEncoded(base_gurl) && IsOriginEncoded(base_gurl)) {
    return StrCat(kTopDomain,
                  base_gurl.PathSansLeaf(),
                  output_resource.full_name().Encode());
  } else {
    return EncodeUrl(output_resource.original_base(),
                     output_resource.unmapped_base(),
                     base_gurl.PathAndLeaf(),
                     output_resource.full_name());
  }
}

bool TestUrlNamer::Decode(const GoogleUrl& request_url,
                         GoogleUrl* owner_domain,
                         GoogleString* decoded) const {
  if (!IsPathEncoded(request_url)) {
    return false;
  }

  StringPiece request_path = request_url.PathSansLeaf();
  StringPieceVector path_vector;
  SplitStringPieceToVector(request_path, "/", &path_vector, false);

  *decoded = StrCat(path_vector[3], "://", path_vector[4]);
  for (size_t i = 5, n = path_vector.size() - 1; i < n; ++i) {
    StrAppend(decoded, "/", path_vector[i]);
  }
  StrAppend(decoded, "/", request_url.LeafWithQuery());

  return true;
}

GoogleString TestUrlNamer::EncodeUrl(const StringPiece& original_base,
                                     const StringPiece& unmapped_base,
                                     const StringPiece& resolved_path,
                                     const ResourceNamer& leaf_details) {
  GoogleUrl   original_base_gurl(original_base);
  StringPiece original_base_scheme(original_base_gurl.Scheme());
  StringPiece original_base_host(original_base_gurl.HostAndPort());
  GoogleUrl   unmapped_base_gurl(unmapped_base);
  StringPiece unmapped_base_scheme(unmapped_base_gurl.Scheme());
  StringPiece unmapped_base_host(unmapped_base_gurl.HostAndPort());

  return StrCat(kTopDomain,
                StrCat("/", original_base_scheme,
                       "/", original_base_host,
                       "/", unmapped_base_scheme,
                       "/", unmapped_base_host),
                resolved_path,
                leaf_details.Encode());
}

bool TestUrlNamer::IsOriginEncoded(const GoogleUrl& url) const {
  GoogleString url_origin = url.Origin().as_string();
  if (url_origin == kTopDomain) {
    return true;
  }
  return false;
}

bool TestUrlNamer::IsPathEncoded(const GoogleUrl& url) const {
  StringPieceVector path_vector;
  SplitStringPieceToVector(url.PathSansLeaf(), "/", &path_vector, false);

  // original-scheme/original-domain/unmapped-scheme/unmapped-domain/...
  if (path_vector.size() < 5) {
    return false;
  }
  if (path_vector[1] != "http" && path_vector[1] != "https") {
    return false;
  }
  if (path_vector[3] != "http" && path_vector[3] != "https") {
    return false;
  }

  return true;
}

}  // namespace net_instaweb
