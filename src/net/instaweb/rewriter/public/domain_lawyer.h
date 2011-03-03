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
//
// This class manages the relationships between domains and resources.
//
// The Lawyer keeps track of which domains we are allowed to rewrite, including
// whether multiple resources can be bundled together.
//
// The Lawyer keeps track of domain mappings to move resources onto a CDN or
// onto a cookieless domain.
//
// The Lawyer keeps track of domain sharding, for distributing resources across
// equivalent domains to improve browser download parallelism.
//
// The class here holds state based on the configuration files
// (e.g. Apache .conf).

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DOMAIN_LAWYER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DOMAIN_LAWYER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/google_url.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;

class DomainLawyer {
 public:
  DomainLawyer() {}
  ~DomainLawyer();

  // Determines whether a resource can be rewritten, and returns the domain
  // that it should be written to.  Only the domain of the resolved request
  // is considered.  If the resource_url is relative (has no domain) then
  // the resource can always be written, and will share the domain of the
  // original request.
  //
  // The resource_url is considered relative to original_request.  Generally
  // it is always accessible to rewrite resources in the same domain as the
  // original.
  //
  // TODO(jmarantz): the mapped domain name will not incorporate any sharding.
  // This must be handled by another mapping function which has not yet
  // been implemented.
  //
  // The returned mapped_domain_name will always end with a slash on success.
  // The returned resolved_request incorporates rewrite-domain mapping and
  // the original URL.
  //
  // Returns false on failure.
  bool MapRequestToDomain(const GURL& original_request,
                          const StringPiece& resource_url,
                          std::string* mapped_domain_name,
                          GoogleUrl* resolved_request,
                          MessageHandler* handler) const;

  // Maps an origin resource; just prior to fetching it.  This fails
  // if the input URL is not valid.  It succeeds even if there is no
  // mapping done.  You must compare 'in' to 'out' to determine if
  // mapping was done.
  bool MapOrigin(const StringPiece& in, std::string* out) const;

  // The methods below this comment are intended only to be run only
  // at configuration time.

  // Adds a simple domain to the set that can be rewritten.  No
  // mapping or sharding will be performed.  Returns false if the
  // domain syntax was not acceptable.  Wildcards (*, ?) may be used in
  // the domain_name.   Careless use of wildcards can expose the user to
  // XSS attacks.
  bool AddDomain(const StringPiece& domain_name, MessageHandler* handler);

  // Adds a domain mapping, to assist with serving resources from
  // cookieless domains or CDNs.  This implicitly calls AddDomain(to_domain)
  // and AddDomain(from_domain) if necessary.  If either 'to' or 'from' has
  // invalid syntax then this function returns false and has no effect.
  //
  // Wildcards may not be used in the to_domain, but they can be used
  // in the from_domains.
  //
  // This routine can be called multiple times for the same to_domain.  If
  // the 'from' domains overlap due to wildcards, this will not be detected.
  bool AddRewriteDomainMapping(const StringPiece& to_domain,
                               const StringPiece& comma_separated_from_domains,
                               MessageHandler* handler);

  // Adds a domain mapping, to assist with fetching resources from locally
  // signficant names/ip-addresses.
  //
  // Wildcards may not be used in the to_domain, but they can be used
  // in the from_domains.
  //
  // This routine can be called multiple times for the same to_domain.  If
  // the 'from' domains overlap due to wildcards, this will not be detected.
  bool AddOriginDomainMapping(const StringPiece& to_domain,
                              const StringPiece& comma_separated_from_domains,
                              MessageHandler* handler);

  // Specifies domain-sharding.  This implicitly calls AddDomain(to_domain).
  //
  // Wildcards may not be used in the to_domain or the from_domain.
  bool AddShard(const StringPiece& to_domain,
                const StringPiece& comma_separated_shards,
                MessageHandler* handler);

  // Computes a domain shard based on a passed-in hash, returning true
  // if the domain was sharded.  Output argument 'shard' is only updated
  // if when the return value is true.
  //
  // The hash is an explicit uint32 so that we get the same shard for a
  // resource, whether the server is 32-bit or 64-bit.  If we have
  // 5 shards and used size_t for hashes, then we'd wind up with different
  // shards on 32-bit and 64-bit machines and that would reduce cacheability
  // of the sharded resources.
  bool ShardDomain(const StringPiece& domain_name, uint32 hash,
                   std::string* shard) const;

  // Merge the domains declared in src into this.  There are no exclusions, so
  // this is really just aggregating the mappings and authorizations declared in
  // both domains.  When the same domain is mapped in 'this' and 'src', 'src'
  // wins.
  void Merge(const DomainLawyer& src);

  // Determines whether a resource of the given domain name is going
  // to change due to RewriteDomain mapping or domain sharding.  Note
  // that this does not account for the actual domain shard selected.
  bool WillDomainChange(const StringPiece& domain_name) const;

 private:
  class Domain;
  typedef bool (Domain::*SetDomainFn)(Domain* domain, MessageHandler* handler);

  static std::string NormalizeDomainName(const StringPiece& domain_name);

  bool MapDomainHelper(
      const StringPiece& to_domain_name,
      const StringPiece& comma_separated_from_domains,
      SetDomainFn set_domain_fn,
      bool allow_wildcards,
      bool authorize,
      MessageHandler* handler);

  Domain* AddDomainHelper(const StringPiece& domain_name,
                          bool warn_on_duplicate,
                          bool authorize,
                          MessageHandler* handler);
  Domain* CloneAndAdd(const Domain* src);

  Domain* FindDomain(const std::string& domain_name) const;

  typedef std::map<std::string, Domain*> DomainMap;
  DomainMap domain_map_;
  typedef std::vector<Domain*> DomainVector;
  DomainVector wildcarded_domains_;
  // If you add more fields here, please be sure to update Merge().

  DISALLOW_COPY_AND_ASSIGN(DomainLawyer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DOMAIN_LAWYER_H_
