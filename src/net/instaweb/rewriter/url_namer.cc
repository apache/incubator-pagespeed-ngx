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

UrlNamer::UrlNamer() {
}

UrlNamer::~UrlNamer() {
}

// Moved from OutputResource::url()
GoogleString UrlNamer::Encode(const RewriteOptions* rewrite_options,
                              const OutputResource& output_resource) {
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
    GoogleString shard;
    if (domain_lawyer->ShardDomain(domain, int_hash, &shard)) {
      // The Path has a leading "/", and shard has a trailing "/".  So
      // we need to perform some StringPiece substring arithmetic to
      // make them all fit together.  Note that we could have used
      // string's substr method but that would have made another temp
      // copy, which seems like a waste.
      encoded_path = StrCat(shard, gurl.PathAndLeaf().substr(1));
    } else {
      encoded_path = output_resource.resolved_base();
    }
  }
  return StrCat(encoded_path, encoded_leaf);
}

GoogleString UrlNamer::Decode(const GoogleUrl& request_url,
                              const RequestHeaders& request_headers,
                              MessageHandler* handler) {
  return "";
}

RewriteOptions* UrlNamer::DecodeOptions(const GoogleUrl& request_url,
                                        const RequestHeaders& request_headers,
                                        MessageHandler* handler) {
  return NULL;
}

}  // namespace net_instaweb
