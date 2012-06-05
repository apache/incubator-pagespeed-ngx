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

#include <map>
#include <set>
#include <utility>  // for std::pair
#include <vector>

#include "base/logging.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
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
  const GoogleString& name() const { return name_; }

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

  GoogleString Signature() const {
    GoogleString signature;
    StrAppend(&signature, name_, "_",
              authorized_ ? "_a" : "_n", "_");
    // Assuming that there will be no cycle of Domains like Domain A has a
    // rewrite domain to domain B which in turn have the original domain as A.
    if (rewrite_domain_ != NULL) {
      StrAppend(&signature, "R:", rewrite_domain_->name(), "_");
    }
    if (origin_domain_ != NULL) {
      StrAppend(&signature, "O:", origin_domain_->name(), "_");
    }
    for (int index = 0; index < num_shards(); ++index) {
      if (shards_[index] != NULL) {
        StrAppend(&signature, "S:", shards_[index]->name(), "_");
      }
    }
    return signature;
  }

  GoogleString ToString() const {
    GoogleString output = name_;

    if (authorized_) {
      StrAppend(&output, " Auth");
    }

    if (rewrite_domain_ != NULL) {
      StrAppend(&output, " RewriteDomain:", rewrite_domain_->name());
    }

    if (origin_domain_ != NULL) {
      StrAppend(&output, " OriginDomain:", origin_domain_->name());
    }

    if (!shards_.empty()) {
      StrAppend(&output, " Shards:{");
      for (int i = 0, n = shards_.size(); i < n; ++i) {
        StrAppend(&output, (i == 0 ? "" : ", "), shards_[i]->name());
      }
      StrAppend(&output, "}");
    }

    return output;
  }

 private:
  bool authorized_;
  Wildcard wildcard_;
  GoogleString name_;

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

GoogleString DomainLawyer::NormalizeDomainName(const StringPiece& domain_name) {
  // Ensure that the following specifications are treated identically:
  //     www.google.com
  //     http://www.google.com
  //     www.google.com/
  //     http://www.google.com/
  //     WWW.GOOGLE.COM/
  // all come out the same.
  GoogleString domain_name_str;
  if (domain_name.find("://") == GoogleString::npos) {
    domain_name_str = StrCat("http://", domain_name);
  } else {
    domain_name.CopyToString(&domain_name_str);
  }
  EnsureEndsInSlash(&domain_name_str);
  LowerString(&domain_name_str);
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

  // TODO(matterbury): need better data structures to eliminate the O(N) logic:
  // 1) Use a trie for domain_map_ as we need to find the domain whose trie
  //    path matches the beginning of the given domain_name since we no longer
  //    match just the domain name.
  // 2) Use a better lookup structure for wildcard searching.
  GoogleString domain_name_str = NormalizeDomainName(domain_name);
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
DomainLawyer::Domain* DomainLawyer::FindDomain(const GoogleUrl& gurl) const {
  // First do a quick lookup on the domain name only, since that's the most
  // common case. Failing that, try searching for domain + path.
  // TODO(matterbury): see AddDomainHelper for speed issues.
  GoogleString domain_name;
  gurl.Origin().CopyToString(&domain_name);
  EnsureEndsInSlash(&domain_name);
  DomainMap::const_iterator p = domain_map_.find(domain_name);
  if (p == domain_map_.end() && gurl.has_path()) {
    StringPiece domain_spec(gurl.Spec());
    for (p = domain_map_.begin(); p != domain_map_.end(); ++p) {
      Domain* src_domain = p->second;
      if (!src_domain->IsWildcarded() &&
          HasPrefixString(domain_spec, src_domain->name())) {
        break;
      }
    }
  }

  Domain* domain = NULL;
  if (p != domain_map_.end()) {
    domain = p->second;
  } else {
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
    GoogleString* mapped_domain_name,
    GoogleUrl* resolved_request,
    MessageHandler* handler) const {
  CHECK(original_request.is_valid());
  GoogleUrl original_origin(original_request.Origin());
  resolved_request->Reset(original_request, resource_url);

  bool ret = false;
  // We can map a request to/from http/https.
  if (resolved_request->is_valid()) {
    GoogleUrl resolved_origin(resolved_request->Origin());

    // Looks at the resolved domain name + path from the original request
    // and the resource_url (which might override the original request).
    // Gets the Domain* object out of that.
    Domain* resolved_domain = FindDomain(*resolved_request);

    // The origin domain is authorized by default.
    if (resolved_origin == original_origin) {
      resolved_origin.Spec().CopyToString(mapped_domain_name);
      ret = true;
    } else if (resolved_domain != NULL && resolved_domain->authorized()) {
      if (resolved_domain->IsWildcarded()) {
        // This is a sharded domain. We do not do the sharding in this function.
        resolved_origin.Spec().CopyToString(mapped_domain_name);
      } else {
        *mapped_domain_name = resolved_domain->name();
      }
      ret = true;
    }

    // If we actually got a Domain* out of the lookups so far, then a
    // mapping to a different rewrite_domain may be contained there.  This
    // helps move resources to CDNs or cookieless domains.
    //
    // Note that at this point, we are not really caring where we fetch
    // from.  We are only concerned here with what URLs we will write into
    // HTML files.  See MapOrigin below which is used to redirect fetch
    // requests to a different domain (e.g. localhost).
    if (ret && resolved_domain != NULL) {
      Domain* mapped_domain = resolved_domain->rewrite_domain();
      if (mapped_domain != NULL) {
        CHECK(!mapped_domain->IsWildcarded());
        *mapped_domain_name = mapped_domain->name();
        GoogleUrl mapped_domain_url(*mapped_domain_name);
        // mapped_domain_url can have a path part after the domain, which is
        // lost if we join it with an absolute path (which is what PathAndLeaf
        // returns), so remove the leading slash to make it relative so
        // domain of http://domain.com/path/ + path of [/]root/dir/leaf
        // gives http://domain.com/path/root/dir/leaf.
        //
        // TODO(sligocki): Note, this will technically fail if path starts
        // with "//", which is technically legal, but I've never seen it before
        // in the wild.
        resolved_request->Reset(mapped_domain_url,
                                resolved_request->PathAndLeaf().substr(1));
      }
    }
  }
  return ret;
}

bool DomainLawyer::IsDomainAuthorized(const GoogleUrl& original_request,
                                      const GoogleUrl& domain_to_check) const {
  bool ret = false;
  if (domain_to_check.is_valid()) {
    if (original_request.is_valid() &&
        (original_request.Origin() == domain_to_check.Origin())) {
      ret = true;
    } else {
      Domain* path_domain = FindDomain(domain_to_check);
      ret = (path_domain != NULL) && path_domain->authorized();
    }
  }
  return ret;
}

bool DomainLawyer::MapOrigin(const StringPiece& in, GoogleString* out) const {
  GoogleUrl gurl(in);
  return MapOriginUrl(gurl, out);
}

bool DomainLawyer::MapOriginUrl(const GoogleUrl& gurl,
                                GoogleString* out) const {
  bool ret = false;
  // We can map an origin TO http only, but FROM http or https.
  if (gurl.is_valid()) {
    ret = true;
    gurl.Spec().CopyToString(out);
    Domain* domain = FindDomain(gurl);
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
  bool result = MapDomainHelper(to_domain_name, comma_separated_from_domains,
                                &Domain::SetRewriteDomain,
                                true /* allow_wildcards */,
                                true /* allow_map_to_https */,
                                true /* authorize */,
                                handler);
  can_rewrite_domains_ |= result;
  return result;
}

bool DomainLawyer::DomainNameToTwoProtocols(
    const StringPiece& domain_name,
    GoogleString* http_url, GoogleString* https_url) {
  *http_url = NormalizeDomainName(domain_name);
  StringPiece http_url_piece(*http_url);
  if (!http_url_piece.starts_with("http:")) {
    return false;
  }
  *https_url = StrCat("https", http_url_piece.substr(4));
  return true;
}

bool DomainLawyer::TwoProtocolDomainHelper(
      const StringPiece& to_domain_name,
      const StringPiece& from_domain_name,
      SetDomainFn set_domain_fn,
      bool authorize,
      MessageHandler* handler) {
  GoogleString http_to_url, http_from_url, https_to_url, https_from_url;
  if (!DomainNameToTwoProtocols(to_domain_name, &http_to_url, &https_to_url)) {
    return false;
  }
  if (!DomainNameToTwoProtocols(from_domain_name,
                                &http_from_url, &https_from_url)) {
    return false;
  }
  if (!MapDomainHelper(http_to_url, http_from_url,
                       set_domain_fn,
                       false, /* allow_wildcards */
                       false, /* allow_map_to_https */
                       authorize, handler)) {
    return false;
  }
  if (!MapDomainHelper(https_to_url, https_from_url,
                       set_domain_fn,
                       false, /* allow_wildcards */
                       true, /* allow_map_to_https */
                       authorize, handler)) {
    // Note that we still retain the http domain mapping in this case.
    return false;
  }
  return true;
}

bool DomainLawyer::AddTwoProtocolRewriteDomainMapping(
    const StringPiece& to_domain_name,
    const StringPiece& from_domain_name,
    MessageHandler* handler) {
  bool result = TwoProtocolDomainHelper(to_domain_name, from_domain_name,
                                        &Domain::SetRewriteDomain,
                                        true /*authorize */, handler);
  can_rewrite_domains_ |= result;
  return result;
}

bool DomainLawyer::AddOriginDomainMapping(
    const StringPiece& to_domain_name,
    const StringPiece& comma_separated_from_domains,
    MessageHandler* handler) {
  return MapDomainHelper(to_domain_name, comma_separated_from_domains,
                         &Domain::SetOriginDomain,
                         true /* allow_wildcards */,
                         false /* allow_map_to_https */,
                         false /* authorize */,
                         handler);
}

bool DomainLawyer::AddTwoProtocolOriginDomainMapping(
    const StringPiece& to_domain_name,
    const StringPiece& from_domain_name,
    MessageHandler* handler) {
  return TwoProtocolDomainHelper(to_domain_name, from_domain_name,
                                 &Domain::SetOriginDomain,
                                 false /*authorize */, handler);
}

bool DomainLawyer::AddShard(
    const StringPiece& shard_domain_name,
    const StringPiece& comma_separated_shards,
    MessageHandler* handler) {
  bool result = MapDomainHelper(shard_domain_name, comma_separated_shards,
                                &Domain::SetShardFrom,
                                false /* allow_wildcards */,
                                true /* allow_map_to_https */,
                                true /* authorize */,
                                handler);
  can_rewrite_domains_ |= result;
  return result;
}

bool DomainLawyer::IsSchemeSafeToMapTo(const StringPiece& domain_name,
                                       bool allow_https_scheme) {
  // The scheme defaults to http so that's the same as explicitly saying http.
  return (domain_name.find("://") == GoogleString::npos ||
          domain_name.starts_with("http://") ||
          (allow_https_scheme && domain_name.starts_with("https://")));
}

bool DomainLawyer::MapDomainHelper(
    const StringPiece& to_domain_name,
    const StringPiece& comma_separated_from_domains,
    SetDomainFn set_domain_fn,
    bool allow_wildcards,
    bool allow_map_to_https,
    bool authorize_to_domain,
    MessageHandler* handler) {
  if (!IsSchemeSafeToMapTo(to_domain_name, allow_map_to_https)) {
    return false;
  }
  Domain* to_domain = AddDomainHelper(to_domain_name, false,
                                      authorize_to_domain, handler);
  if (to_domain == NULL) {
    return false;
  }

  bool ret = false;
  bool mapped_a_domain = false;
  if (to_domain->IsWildcarded()) {
    handler->Message(kError, "Cannot map to a wildcarded domain: %s",
                     to_domain_name.as_string().c_str());
  } else {
    GoogleUrl to_url(to_domain->name());
    StringPieceVector domains;
    SplitStringPieceToVector(comma_separated_from_domains, ",", &domains, true);
    ret = true;
    for (int i = 0, n = domains.size(); i < n; ++i) {
      const StringPiece& domain_name = domains[i];
      Domain* from_domain = AddDomainHelper(domain_name, false, true, handler);
      if (from_domain != NULL) {
        GoogleUrl from_url(from_domain->name());
        if (to_url.Origin() == from_url.Origin()) {
          // Ignore requests to map to the same scheme://hostname:port/.
        } else if (!allow_wildcards && from_domain->IsWildcarded()) {
          handler->Message(kError, "Cannot map from a wildcarded domain: %s",
                           to_domain_name.as_string().c_str());
          ret = false;
        } else {
          bool ok = (from_domain->*set_domain_fn)(to_domain, handler);
          ret &= ok;
          mapped_a_domain |= ok;
        }
      }
    }
  }
  return (ret && mapped_a_domain);
}

DomainLawyer::Domain* DomainLawyer::CloneAndAdd(const Domain* src) {
  return AddDomainHelper(src->name(), false, src->authorized(), NULL);
}

void DomainLawyer::Merge(const DomainLawyer& src) {
  int num_existing_wildcards = num_wildcarded_domains();
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

  // Remove the wildcards we just added in map order, and instead add them
  // in the order they were in src.wildcarded_domains.
  wildcarded_domains_.resize(num_existing_wildcards);
  std::set<Domain*> dup_detector(wildcarded_domains_.begin(),
                                 wildcarded_domains_.end());
  for (int i = 0, n = src.wildcarded_domains_.size(); i < n; ++i) {
    Domain* src_domain = src.wildcarded_domains_[i];
    DomainMap::const_iterator p = domain_map_.find(src_domain->name());
    if (p == domain_map_.end()) {
      LOG(DFATAL) << "Domain " << src_domain->name() << " not found in dst";
    } else {
      Domain* dst_domain = p->second;
      if (dup_detector.find(dst_domain) == dup_detector.end()) {
        wildcarded_domains_.push_back(dst_domain);
      }
    }
  }

  can_rewrite_domains_ |= src.can_rewrite_domains_;
}

bool DomainLawyer::ShardDomain(const StringPiece& domain_name,
                               uint32 hash,
                               GoogleString* sharded_domain) const {
  GoogleUrl domain_gurl(NormalizeDomainName(domain_name));
  Domain* domain = FindDomain(domain_gurl);
  bool sharded = false;
  if (domain != NULL) {
    if (domain->num_shards() != 0) {
      int shard_index = hash % domain->num_shards();
      domain = domain->shard(shard_index);
      *sharded_domain = domain->name();
      sharded = true;
    }
  }
  return sharded;
}

bool DomainLawyer::WillDomainChange(const StringPiece& domain_name) const {
  GoogleUrl domain_gurl(NormalizeDomainName(domain_name));
  Domain* domain = FindDomain(domain_gurl), *mapped_domain = domain;
  if (domain != NULL) {
    // First check a mapping based on AddRewriteDomainMapping.
    mapped_domain = domain->rewrite_domain();
    if (mapped_domain == NULL)  {
      // Even if there was no AddRewriteDomainMapping for this domain, there
      // may still have been shards.
      mapped_domain = domain;
    }

    // Now check mappings from the shard.
    if (mapped_domain->num_shards() != 0) {
      if (mapped_domain->num_shards() == 1) {
        // Usually we don't expect exactly one shard, but if there is,
        // we know exactly what it will be.
        mapped_domain = mapped_domain->shard(0);
      } else {
        // We don't have enough data in this function to determine what
        // the shard index will be, so we assume pessimistically that
        // the domain will change.
        //
        // TODO(jmarantz): rename this method to MayDomainChange, or
        // pass in the sharding index.
        mapped_domain = NULL;
      }
    }
  }
  return domain != mapped_domain;
}

bool DomainLawyer::DoDomainsServeSameContent(
    const StringPiece& domain1_name, const StringPiece& domain2_name) const {
  GoogleUrl domain1_gurl(NormalizeDomainName(domain1_name));
  Domain* domain1 = FindDomain(domain1_gurl);
  GoogleUrl domain2_gurl(NormalizeDomainName(domain2_name));
  Domain* domain2 = FindDomain(domain2_gurl);
  if ((domain1 == NULL) || (domain2 == NULL)) {
    return false;
  }
  if (domain1 == domain2) {
    return true;
  }
  Domain* rewrite1 = domain1->rewrite_domain();
  Domain* rewrite2 = domain2->rewrite_domain();
  if ((rewrite1 == domain2) || (rewrite2 == domain1)) {
    return true;
  }
  if ((rewrite1 != NULL) && (rewrite1 == rewrite2)) {
    return true;
  }
  return false;
}

GoogleString DomainLawyer::Signature() const {
  GoogleString signature;

  for (DomainMap::const_iterator iterator = domain_map_.begin();
      iterator != domain_map_.end(); ++iterator) {
    StrAppend(&signature, "D:", iterator->second->Signature(), "-");
  }

  return signature;
}

GoogleString DomainLawyer::ToString(StringPiece line_prefix) const {
  GoogleString output;
  for (DomainMap::const_iterator iterator = domain_map_.begin();
      iterator != domain_map_.end(); ++iterator) {
    StrAppend(&output, line_prefix, iterator->second->ToString(), "\n");
  }
  return output;
}

}  // namespace net_instaweb
