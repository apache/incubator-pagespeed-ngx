/*
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

#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/wildcard.h"

namespace net_instaweb {

class DomainLawyer::Domain {
 public:
  explicit Domain(const StringPiece& name)
      : authorized_(false),
        wildcard_(name),
        name_(name.data(), name.size()),
        rewrite_domain_(NULL),
        origin_domain_(NULL),
        cycle_breadcrumb_(false) {
  }

  bool IsWildcarded() const { return !wildcard_.IsSimple(); }
  bool Match(const StringPiece& domain) { return wildcard_.Match(domain); }
  Domain* rewrite_domain() const { return rewrite_domain_; }
  Domain* origin_domain() const { return origin_domain_; }
  StringPiece name() const { return name_; }

  // When multiple domains are mapped to the same rewrite-domain, they
  // should have consistent origins.  If they don't, we print an error
  // message but we keep rolling.  This is because we don't want to
  // introduce an incremental change that would invalidate existing
  // pagespeed.conf files.
  //
  void MergeOrigin(Domain* origin_domain, MessageHandler* handler) {
    if (cycle_breadcrumb_) {
      // See DomainLawyerTest.RewriteOriginCycle
      return;
    }
    cycle_breadcrumb_ = true;
    if ((origin_domain != origin_domain_) && (origin_domain != NULL)) {
      if (origin_domain_ != NULL) {
        if (handler != NULL) {
          handler->Message(kError,
                           "RewriteDomain %s has conflicting origins %s and "
                           "%s, overriding to %s",
                           name_.c_str(),
                           origin_domain_->name_.c_str(),
                           origin_domain->name_.c_str(),
                           origin_domain->name_.c_str());
        }
      }
      origin_domain_ = origin_domain;
      for (int i = 0; i < num_shards(); ++i) {
        shards_[i]->MergeOrigin(origin_domain, handler);
      }
      if (rewrite_domain_ != NULL) {
        rewrite_domain_->MergeOrigin(origin_domain, handler);
      }
    }
    cycle_breadcrumb_ = false;
  }

  // handler==NULL means this is happening from a 'merge' so we will
  // silently let the new rewrite_domain win.
  bool SetRewriteDomain(Domain* rewrite_domain, MessageHandler* handler) {
    rewrite_domain_ = rewrite_domain;
    rewrite_domain->MergeOrigin(origin_domain_, handler);
    return true;  // don't break old configs on this new consistency check.
  }

  // handler==NULL means this is happening from a 'merge' so we will
  // silently let the new origin_domain win.
  bool SetOriginDomain(Domain* origin_domain, MessageHandler* handler) {
    MergeOrigin(origin_domain, handler);
    if (rewrite_domain_ != NULL) {
      rewrite_domain_->MergeOrigin(origin_domain_, handler);
    }
    return true;  // don't break old configs on this new consistency check.
  }

  // handler==NULL means this is happening from a 'merge' so we will
  // silently let the new rewrite_domain win.
  bool SetShardFrom(Domain* rewrite_domain, MessageHandler* handler) {
    if ((rewrite_domain_ != rewrite_domain) && (rewrite_domain_ != NULL)) {
      if (handler != NULL) {
        // We only treat this as an error when the handler is non-null.  We
        // use a null handler during merges, and will do the best we can
        // to get correct behavior.
        handler->Message(kError,
                         "Shard %s has conflicting rewrite_domain %s and %s",
                         name_.c_str(),
                         rewrite_domain_->name_.c_str(),
                         rewrite_domain->name_.c_str());
        return false;
      }
    }
    MergeOrigin(rewrite_domain->origin_domain_, handler);
    rewrite_domain->shards_.push_back(this);
    rewrite_domain_ = rewrite_domain;
    return true;
  }

  void set_authorized(bool authorized) { authorized_ = authorized; }

  int num_shards() const { return shards_.size(); }

  // Indicates whether this domain is authorized when found in URLs
  // HTML files are as direct requests to the web server.  Domains
  // get authorized by mentioning them in ModPagespeedDomain,
  // ModPagespeedMapRewriteDomain, ModPagespeedShardDomain, and as
  // the from-list in ModPagespeedMapOriginDomain.  However, the target
  // of ModPagespeedMapOriginDomain is not implicitly authoried --
  // that may be 'localhost'.
  bool authorized() const { return authorized_; }

  Domain* shard(int shard_index) const { return shards_[shard_index]; }

 private:
  bool authorized_;
  Wildcard wildcard_;
  std::string name_;

  // The rewrite_domain, if non-null, gives the location of where this
  // Domain should be rewritten.  This can be used to move resources onto
  // a CDN or onto a cookieless domain.  We also use this pointer to
  // get from shards back to the domain they were sharded from.
  Domain* rewrite_domain_;

  // The origin_domain, if non-null, gives the location of where
  // resources should be fetched from by mod_pagespeed, in lieu of how
  // it is specified in the HTML.  This allows, for example, a CDN to
  // fetch content from an origin domain, or an origin server behind a
  // load-balancer to specify localhost or an IP address of a host to
  // go to directly, skipping DNS resolution and reducing outbound
  // traffic.
  Domain* origin_domain_;

  // A rewrite_domain keeps track of all its shards.
  DomainVector shards_;

  // This boolean helps us prevent spinning through a cycle in the
  // graph that can be expressed between shards and rewrite domains, e.g.
  //   ModPagespeedMapOriginDomain a b
  //   ModPagespeedMapRewriteDomain b c
  //   ModPagespeedAddShard b c
  bool cycle_breadcrumb_;
};

DomainLawyer::~DomainLawyer() {
  STLDeleteValues(&domain_map_);
}

bool DomainLawyer::AddDomain(const StringPiece& domain_name,
                             MessageHandler* handler) {
  return (AddDomainHelper(domain_name, true, true, handler) != NULL);
}

std::string DomainLawyer::NormalizeDomainName(const StringPiece& domain_name) {
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
  EnsureEndsInSlash(&domain_name_str);
  return domain_name_str;
}

DomainLawyer::Domain* DomainLawyer::AddDomainHelper(
    const StringPiece& domain_name, bool warn_on_duplicate,
    bool authorize, MessageHandler* handler) {
  if (domain_name.empty()) {
    // handler will be NULL only when called from Merge, which should
    // only have pre-validated (non-empty) domains.  So it should not
    // be possible to get here from Merge.
    if (handler != NULL) {
      handler->Message(kWarning, "Empty domain passed to AddDomain");
    }
    return NULL;
  }

  std::string domain_name_str = NormalizeDomainName(domain_name);
  Domain* domain = NULL;
  std::pair<DomainMap::iterator, bool> p = domain_map_.insert(
      DomainMap::value_type(domain_name_str, domain));
  DomainMap::iterator iter = p.first;
  if (p.second) {
    domain = new Domain(domain_name_str);
    iter->second = domain;
    if (domain->IsWildcarded()) {
      wildcarded_domains_.push_back(domain);
    }
    iter->second = domain;
  } else {
    domain = iter->second;
    if (warn_on_duplicate && (authorize == domain->authorized())) {
      handler->Message(kWarning, "AddDomain of domain already in map: %s",
                       domain_name_str.c_str());
      domain = NULL;
    }
  }
  if (authorize && (domain != NULL)) {
    domain->set_authorized(true);
  }
  return domain;
}

// Looks up the Domain* object by name.  From the Domain object
// we can tell if it's wildcarded, in which case it cannot be
// the 'to' field for a map, and whether resources from it should
// be mapped to a different domain, either for rewriting or for
// fetching.
DomainLawyer::Domain* DomainLawyer::FindDomain(
    const std::string& domain_name) const {
  DomainMap::const_iterator p = domain_map_.find(domain_name);
  Domain* domain = NULL;
  if (p != domain_map_.end()) {
    domain = p->second;
  } else {
    // TODO(jmarantz): use a better lookup structure for this
    for (int i = 0, n = wildcarded_domains_.size(); i < n; ++i) {
      domain = wildcarded_domains_[i];
      if (domain->Match(domain_name)) {
        break;
      } else {
        domain = NULL;
      }
    }
  }
  return domain;
}

bool DomainLawyer::MapRequestToDomain(
    const GoogleUrl& original_request,
    const StringPiece& resource_url,  // relative to original_request
    std::string* mapped_domain_name,
    GoogleUrl* resolved_request,
    MessageHandler* handler) const {
  CHECK(original_request.is_valid());
  GoogleUrl original_origin(original_request.Origin());
  GoogleUrl tmp_request(original_request, resource_url);
  resolved_request->Swap(&tmp_request);
  if (!resolved_request->is_valid()) {
    return false;
  }
  bool ret = false;
  // At present we're not sure about appropriate resource
  // policies for https: etc., so we only permit http resources
  // to be rewritten.
  // TODO(jmaessen): Figure out if this is appropriate.
  if (resolved_request->is_valid() && resolved_request->SchemeIs("http")) {
    GoogleUrl resolved_origin(resolved_request->Origin());
    std::string resolved_domain_name;
    resolved_origin.Spec().CopyToString(&resolved_domain_name);

    // Looks at the resovled domain name from the original request and
    // the resource_url (which might override the original request).
    // Gets the Domain* object out of that.
    Domain* resolved_domain = FindDomain(resolved_domain_name);

    // The origin domain is authorized by default.
    if ((resolved_origin == original_origin) ||
        ((resolved_domain != NULL) && resolved_domain->authorized())) {
      *mapped_domain_name = resolved_domain_name;
      ret = true;

      // If we actually got a Domain* out of the lookups so far, then a
      // mapping to a different rewrite_domain may be contained there.  This
      // helps move resources to CDNs or cookieless domains.
      //
      // Note that at this point, we are not really caring where we fetch
      // from.  We are only concerned here with what URLs we will write into
      // HTML files.  See MapOrigin below which is used to redirect fetch
      // requests to a different domain (e.g. localhost).
      if (resolved_domain != NULL) {
        Domain* mapped_domain = resolved_domain->rewrite_domain();
        if (mapped_domain != NULL) {
          CHECK(!mapped_domain->IsWildcarded());
          mapped_domain->name().CopyToString(mapped_domain_name);
          GoogleUrl mapped_domain_url(*mapped_domain_name);
          GoogleUrl tmp(mapped_domain_url, resolved_request->PathAndLeaf());
          resolved_request->Swap(&tmp);
        }
      }
    }
  }
  return ret;
}

bool DomainLawyer::MapOrigin(const StringPiece& in, std::string* out) const {
  bool ret = false;
  GoogleUrl gurl(in);
  // At present we're not sure about appropriate resource
  // policies for https: etc., so we only permit http resources
  // to be rewritten.
  if (gurl.is_valid() && gurl.SchemeIs("http")) {
    ret = true;
    in.CopyToString(out);
    GoogleUrl origin(gurl.Origin());
    std::string origin_name = origin.Spec().as_string();
    Domain* domain = FindDomain(origin_name);
    if (domain != NULL) {
      Domain* origin_domain = domain->origin_domain();
      if (origin_domain != NULL) {
        CHECK(!origin_domain->IsWildcarded());
        GoogleUrl original_domain_url(origin_domain->name());
        GoogleUrl mapped_gurl(original_domain_url, gurl.PathAndLeaf());
        if (mapped_gurl.is_valid()) {
          mapped_gurl.Spec().CopyToString(out);
        }
      }
    }
  }
  return ret;
}

bool DomainLawyer::AddRewriteDomainMapping(
    const StringPiece& to_domain_name,
    const StringPiece& comma_separated_from_domains,
    MessageHandler* handler) {
  return MapDomainHelper(to_domain_name, comma_separated_from_domains,
                         &Domain::SetRewriteDomain, true, true, handler);
}

bool DomainLawyer::AddOriginDomainMapping(
    const StringPiece& to_domain_name,
    const StringPiece& comma_separated_from_domains,
    MessageHandler* handler) {
  return MapDomainHelper(to_domain_name, comma_separated_from_domains,
                         &Domain::SetOriginDomain, true, false, handler);
}

bool DomainLawyer::AddShard(
    const StringPiece& shard_domain_name,
    const StringPiece& comma_separated_shards,
    MessageHandler* handler) {
  return MapDomainHelper(shard_domain_name, comma_separated_shards,
                         &Domain::SetShardFrom, false, true, handler);
}

bool DomainLawyer::MapDomainHelper(
    const StringPiece& to_domain_name,
    const StringPiece& comma_separated_from_domains,
    SetDomainFn set_domain_fn,
    bool allow_wildcards,
    bool authorize_to_domain,
    MessageHandler* handler) {
  Domain* to_domain = AddDomainHelper(to_domain_name, false,
                                      authorize_to_domain, handler);
  bool ret = false;
  if (to_domain->IsWildcarded()) {
    handler->Message(kError, "Cannot map to a wildcarded domain: %s",
                     to_domain_name.as_string().c_str());
  } else if (to_domain != NULL) {
    std::vector<StringPiece> domains;
    SplitStringPieceToVector(comma_separated_from_domains, ",", &domains, true);
    ret = true;
    for (int i = 0, n = domains.size(); i < n; ++i) {
      const StringPiece& domain_name = domains[i];
      Domain* from_domain = AddDomainHelper(domain_name, false, true, handler);
      if (from_domain != NULL) {
        if (!allow_wildcards && from_domain->IsWildcarded()) {
          handler->Message(kError, "Cannot map from a wildcarded domain: %s",
                           to_domain_name.as_string().c_str());
          ret = false;
        } else {
          ret &= (from_domain->*set_domain_fn)(to_domain, handler);
        }
      }
    }
  }
  return ret;
}

DomainLawyer::Domain* DomainLawyer::CloneAndAdd(const Domain* src) {
  return AddDomainHelper(src->name(), false, src->authorized(), NULL);
}

void DomainLawyer::Merge(const DomainLawyer& src) {
  for (DomainMap::const_iterator
           p = src.domain_map_.begin(),
           e = src.domain_map_.end();
       p != e; ++p) {
    Domain* src_domain = p->second;
    Domain* dst_domain = CloneAndAdd(src_domain);
    Domain* src_rewrite_domain = src_domain->rewrite_domain();
    if (src_rewrite_domain != NULL) {
      dst_domain->SetRewriteDomain(CloneAndAdd(src_rewrite_domain), NULL);
    }
    Domain* src_origin_domain = src_domain->origin_domain();
    if (src_origin_domain != NULL) {
      dst_domain->SetOriginDomain(CloneAndAdd(src_origin_domain), NULL);
    }
    for (int i = 0; i < src_domain->num_shards(); ++i) {
      Domain* src_shard = src_domain->shard(i);
      Domain* dst_shard = CloneAndAdd(src_shard);
      dst_shard->SetShardFrom(dst_domain, NULL);
    }
  }
}

bool DomainLawyer::ShardDomain(const StringPiece& domain_name,
                               uint32 hash,
                               std::string* shard) const {
  Domain* domain = FindDomain(
      std::string(domain_name.data(), domain_name.size()));
  bool sharded = false;
  if (domain != NULL) {
    if (domain->num_shards() != 0) {
      int shard_index = hash % domain->num_shards();
      domain = domain->shard(shard_index);
      domain->name().CopyToString(shard);
      sharded = true;
    }
  }
  return sharded;
}

bool DomainLawyer::WillDomainChange(const StringPiece& domain_name) const {
  std::string domain_name_str = NormalizeDomainName(domain_name);
  Domain* domain = FindDomain(domain_name_str);
  if (domain != NULL) {
    if (domain->num_shards() != 0) {
      return true;
    }
    if (domain->rewrite_domain() != NULL) {
      if (domain->rewrite_domain() != domain) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace net_instaweb
