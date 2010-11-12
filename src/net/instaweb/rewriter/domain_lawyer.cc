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
  if (domain_name.empty()) {
    handler->Message(kWarning, "Empty domain passed to AddDomain");
    return false;
  }

  // Ensure that the following specifications are treated identically:
  //     www.google.com
  //     http://www.google.com
  //     www.google.com/
  //     http://www.google.com/
  // all come out the same.
  std::string domain_name_str;
  if (domain_name.find("://") == std::string::npos) {
    domain_name_str = StrCat("http://", domain_name);
  } else {
    domain_name.CopyToString(&domain_name_str);
  }
  if (!domain_name.ends_with("/")) {
    domain_name_str += "/";
  }
  Domain* domain = new Domain(domain_name_str);
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
  GURL original_origin = original_request.GetOrigin();
  GURL resolved = original_origin.Resolve(url_str);
  bool ret = false;
  // At present we're not sure about appropriate resource
  // policies for https: etc., so we only permit http resources
  // to be rewritten.
  // TODO(jmaessen): Figure out if this is appropriate.
  if (resolved.is_valid() && resolved.SchemeIs("http")) {
    GURL resolved_origin = resolved.GetOrigin();
    std::string resolved_domain = GoogleUrl::Spec(resolved_origin);

    if (resolved_origin == original_origin) {
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
