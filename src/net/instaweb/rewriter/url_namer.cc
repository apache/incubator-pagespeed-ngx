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

#include "net/instaweb/rewriter/public/url_namer.h"

#include "base/logging.h"               // for COMPACT_GOOGLE_LOG_FATAL, etc
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_hash.h"
#include "net/instaweb/util/public/string_util.h"  // for StrCat, etc

namespace net_instaweb {

UrlNamer::UrlNamer()
    : proxy_domain_("") {
}

UrlNamer::~UrlNamer() {
}

// Moved from OutputResource::url()
GoogleString UrlNamer::Encode(const RewriteOptions* rewrite_options,
                              const OutputResource& output_resource,
                              EncodeOption encode_option) const {
  GoogleString encoded_leaf(output_resource.full_name().Encode());
  GoogleString encoded_path;
  if (rewrite_options == NULL) {
    encoded_path = output_resource.resolved_base();
  } else {
    StringPiece hash = output_resource.full_name().hash();
    DCHECK(!hash.empty());
    uint32 int_hash = HashString<CasePreserve, uint32>(hash.data(),
                                                       hash.size());
    const DomainLawyer* domain_lawyer = rewrite_options->domain_lawyer();
    GoogleUrl gurl(output_resource.resolved_base());
    GoogleString domain = StrCat(gurl.Origin(), "/");
    GoogleString sharded_domain;
    if ((encode_option == kSharded) &&
        domain_lawyer->ShardDomain(domain, int_hash, &sharded_domain)) {
      // The Path has a leading "/", and sharded_domain has a trailing "/".
      // So we need to perform some StringPiece substring arithmetic to
      // make them all fit together.  Note that we could have used
      // string's substr method but that would have made another temp
      // copy, which seems like a waste.
      encoded_path = StrCat(sharded_domain, gurl.PathAndLeaf().substr(1));
    } else {
      encoded_path = output_resource.resolved_base();
    }
  }
  return StrCat(encoded_path, encoded_leaf);
}

bool UrlNamer::Decode(const GoogleUrl& request_url,
                      GoogleUrl* owner_domain,
                      GoogleString* decoded) const {
  return false;
}

bool UrlNamer::IsAuthorized(const GoogleUrl& request_url,
                            const RewriteOptions& options) const {
  GoogleUrl invalid_request;
  const DomainLawyer* lawyer = options.domain_lawyer();
  return lawyer->IsDomainAuthorized(invalid_request, request_url);
}



bool UrlNamer::ResolveToOriginUrl(const RewriteOptions& options,
                                  const StringPiece& referer_url_str,
                                  GoogleUrl* url) const {
  if (!url->IsWebValid() || IsProxyEncoded(*url)) {
    return false;
  }

  const DomainLawyer* domain_lawyer = options.domain_lawyer();
  GoogleString referer_origin_url;
  GoogleString origin_url_str;
  bool is_proxy = false;
  // Resolve request url to origin url.
  if (domain_lawyer->MapOriginUrl(*url, &origin_url_str, &is_proxy) &&
      url->Spec() != origin_url_str) {
    GoogleUrl temp_url(origin_url_str);
    url->Swap(&temp_url);
    return true;
  } else {
    // Find the origin url for the referer.
    GoogleUrl referer_url(referer_url_str);
    if (domain_lawyer->MapOriginUrl(
            referer_url, &referer_origin_url, &is_proxy) &&
        referer_url_str != referer_origin_url) {
      // Referer has a origin url, resolve the request path w.r.t
      // to origin domain of the referer. This is needed as we are
      // rewriting request urls early, js generated urls might break otherwise.
      GoogleUrl temp_url(referer_origin_url);
      GoogleUrl final_url(temp_url,
          StrCat(url->PathAndLeaf(), url->AllAfterQuery()));
      if (final_url.IsWebValid()) {
        url->Swap(&final_url);
        return true;
      }
    }
  }
  return false;
}

}  // namespace net_instaweb
