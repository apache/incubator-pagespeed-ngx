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
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_hash.h"
#include "net/instaweb/util/public/string_util.h"  // for StrCat, etc

namespace net_instaweb {

UrlNamer::UrlNamer() {
}

UrlNamer::~UrlNamer() {
}

// Moved from OutputResource::url()
GoogleString UrlNamer::Encode(const RewriteOptions* rewrite_options,
                              const OutputResource& output_resource) const {
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
    if (domain_lawyer->ShardDomain(domain, int_hash, &sharded_domain)) {
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

void UrlNamer::DecodeOptions(const GoogleUrl& request_url,
                             const RequestHeaders& request_headers,
                             Callback* callback,
                             MessageHandler* handler) const {
  callback->Done(NULL);
}

void UrlNamer::PrepareRequest(const RewriteOptions* rewrite_options,
                              GoogleString* url,
                              RequestHeaders* request_headers,
                              bool* success,
                              Function* func,
                              MessageHandler* handler) {
  *success = false;
  if (rewrite_options == NULL) {
    *success = true;
  } else {
    GoogleString url_copy = *url;
    GoogleUrl gurl(url_copy);
    if (gurl.is_valid()) {
      request_headers->Replace(HttpAttributes::kHost, gurl.Host());
      const DomainLawyer* domain_lawyer = rewrite_options->domain_lawyer();
      if (domain_lawyer->MapOrigin(url_copy, url)) {
        *success = true;
      }
    }
  }
  func->CallRun();
}

UrlNamer::Callback::~Callback() {
}

}  // namespace net_instaweb
