/**
 * Copyright 2010 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/domain_lawyer.h"

#include <string>
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/wildcard.h"

namespace net_instaweb {

class DomainLawyer::Domain {
 public:
  explicit Domain(const StringPiece& name)
      : wildcard_(name),
        num_shards_(0) {
  }

  bool IsWildcarded() const { return !wildcard_.IsSimple(); }
  bool Match(const StringPiece& domain) { return wildcard_.Match(domain); }

 private:
  Wildcard wildcard_;
  int num_shards_;
};

DomainLawyer::~DomainLawyer() {
  STLDeleteValues(&domain_map_);
}

bool DomainLawyer::AddDomain(const StringPiece& domain_name,
                             MessageHandler* handler) {
  Domain* domain = new Domain(domain_name);
  std::string domain_name_str(domain_name.data(), domain_name.size());
  std::pair<DomainMap::iterator, bool> p = domain_map_.insert(
      DomainMap::value_type(domain_name_str, domain));
  bool ret = p.second;
  if (ret) {
    DomainMap::iterator iter = p.first;
    iter->second = domain;
    if (domain->IsWildcarded()) {
      wildcarded_domains_.push_back(domain);
    }
  } else {
    delete domain;
    handler->Message(kWarning, "AddDomain of domain already in map: %s",
                     domain_name_str.c_str());
  }
  return ret;
}

bool DomainLawyer::MapRequestToDomain(
    const GURL& original_request,
    const StringPiece& resource_url,  // relative to original_request
    std::string* mapped_domain_name,
    MessageHandler* handler) const {
  std::string url_str(resource_url.data(), resource_url.size());
  CHECK(original_request.is_valid());
  GURL resolved = original_request.Resolve(url_str);
  bool ret = false;
  if (resolved.is_valid() && resolved.SchemeIs("http")) {
    // TODO(jmarantz): This domain-construction code should try to
    // re-use the GURL spec construction.  See GURL::GetOrigin(),
    // which might be just what we want.
    std::string resolved_domain = StrCat("http://", resolved.host().c_str());
    if (resolved.has_port()) {
      resolved_domain += StrCat(":", resolved.port().c_str());
    }
    if ((resolved.host() == original_request.host()) &&
        (resolved.port() == original_request.port())) {
      *mapped_domain_name = resolved_domain;
      ret = true;
    } else {
      DomainMap::const_iterator p = domain_map_.find(resolved_domain);
      Domain* domain = NULL;
      if (p != domain_map_.end()) {
        domain = p->second;
      } else {
        for (int i = 0, n = wildcarded_domains_.size(); i < n; ++i) {
          domain = wildcarded_domains_[i];
          if (domain->Match(resolved_domain)) {
            break;
          } else {
            domain = NULL;
          }
        }
      }
      if (domain != NULL) {
        *mapped_domain_name = resolved_domain;
        // TODO(jmarantz): find mapping from Domain* to support domain mapping.
        ret = true;
      }
    }
  }
  return ret;
}

}  // namespace net_instaweb
