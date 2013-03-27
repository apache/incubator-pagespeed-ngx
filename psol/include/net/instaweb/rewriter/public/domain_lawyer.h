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

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
class GoogleUrl;
class MessageHandler;

class DomainLawyer {
 public:
  DomainLawyer() : can_rewrite_domains_(false) {}
  ~DomainLawyer();

  // Determines whether a resource can be rewritten, and returns the domain
  // that it should be written to.  The domain and the path of the resolved
  // request are considered - first just the domain, then the domain plus the
  // root of the path, and so on down the path until a match is found or the
  // path is exhausted; this is done because we can map to a domain plus a
  // path and we want to retain the previous behavior of 'working' when a
  // mapped-to domain was provided.  If the resource_url is relative (has no
  // domain) then the resource can always be written, and will share the domain
  // of the original request.
  //
  // The resource_url is considered relative to original_request.  Generally
  // it is always accessible to rewrite resources in the same domain as the
  // original.
  //
  // Note: The mapped domain name will not incorporate any sharding.
  // This is handled by ShardDomain().
  //
  // The returned mapped_domain_name will always end with a slash on success.
  // The returned resolved_request incorporates rewrite-domain mapping and
  // the original URL.
  //
  // Returns false on failure.
  //
  // This is used both for domain authorization and domain rewriting,
  // but not domain sharding.
  //
  // See also IsDomainAuthorized, which can be used to determine
  // domain authorization without performing a mapping.
  bool MapRequestToDomain(const GoogleUrl& original_request,
                          const StringPiece& resource_url,
                          GoogleString* mapped_domain_name,
                          GoogleUrl* resolved_request,
                          MessageHandler* handler) const;

  // Given the context of an HTTP request to 'original_request',
  // checks whether 'domain_to_check' is authorized for rewriting.
  //
  // For example, if we are rewriting http://www.myhost.com/index.html,
  // then all resources from www.myhost.com are implicitly authorized
  // for rewriting.  Additionally, any domains specified via
  // AddDomain() are also authorized.
  bool IsDomainAuthorized(const GoogleUrl& original_request,
                          const GoogleUrl& domain_to_check) const;


  // Returns true if the given origin (domain:port) is one that we were
  // explicitly told about in any form --- e.g. as a rewrite domain, origin
  // domain, simple domain, or a shard.
  //
  // Note that this method returning true does not mean that resources from the
  // given domain should be rewritten.
  bool IsOriginKnown(const GoogleUrl& domain_to_check) const;

  // Maps an origin resource; just prior to fetching it.  This fails
  // if the input URL is not valid.  It succeeds even if there is no
  // mapping done.  You must compare 'in' to 'out' to determine if
  // mapping was done.
  //
  // *is_proxy is set to true if the origin-domain was established via
  // AddProxyDomainMapping.
  bool MapOrigin(const StringPiece& in, GoogleString* out,
                 bool* is_proxy) const;
  bool MapOriginUrl(const GoogleUrl& gurl, GoogleString* out,
                    bool* is_proxy) const;

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

  // Adds domain mappings that handle both http and https urls for the given
  // from_domain_name.  No wildcards may be used in either domain, and both
  // must be protocol-free and should not have port numbers.
  //
  // This routine can be called multiple times for the same to_domain.
  bool AddTwoProtocolRewriteDomainMapping(const StringPiece& to_domain_name,
                                          const StringPiece& from_domain_name,
                                          MessageHandler* handler);

  // Adds a domain mapping, to assist with fetching resources from locally
  // signficant names/ip-addresses.
  //
  // Wildcards may not be used in the to_domain, but they can be used
  // in the from_domains.
  //
  // This routine can be called multiple times for the same to_domain.  If
  // the 'from' domains overlap due to wildcards, this will not be detected.
  //
  // It is invalid to use the same origin_domain in AddProxyDomainMapping
  // and as the to_domain of AddOriginDomainMapping.  The latter requires
  // a Host: request-header on fetches, whereas the former will not get one.
  bool AddOriginDomainMapping(const StringPiece& to_domain,
                              const StringPiece& comma_separated_from_domains,
                              MessageHandler* handler);

  // Adds a mapping to enable proxying & optimizing resources hosted
  // on a domain we do not control, going back to the origin to
  // fetch them.
  //
  // Wildcards may not be used in the proxy_domain or origin_domain.
  //
  // Subdirectories should normally be used in the proxy_domain, the
  // origin_domain, and to_domain. This is a not a strict requirement. If you
  // fully control the entire origin domain and are dedicating a proxy domain
  // for the sole use of that origin domain then subdirectories are not needed.
  //
  // The proxy_domain must be running mod_pagespeed and configured
  // consistently.  The resources will be referenced from this domain
  // in CSS and HTML files.
  //
  // The origin_domain does not need to run mod_pagespeed; it is used
  // to fetch the resources.
  //
  // If to_domain is provided then resources are rewritten to to_domain instead
  // of proxy_domain.  This is useful for rewriting to a CDN.
  //
  // It is invalid to use the same origin_domain in AddProxyDomainMapping
  // and to_domain of AddOriginDomainMapping.  The latter requires
  // a overriding the Host: request-header on fetches.
  bool AddProxyDomainMapping(const StringPiece& proxy_domain,
                             const StringPiece& origin_domain,
                             const StringPiece& to_domain_name,
                             MessageHandler* handler);

  // Adds domain mappings that handle fetches on both http and https for the
  // given from_domain.  No wildcards may be used in either domain, and both
  // must be protocol-free and should not have port numbers.
  //
  // This routine can be called multiple times for the same to_domain.
  bool AddTwoProtocolOriginDomainMapping(const StringPiece& to_domain_name,
                                         const StringPiece& from_domain_name,
                                         MessageHandler* handler);

  // Specifies domain-sharding.  This implicitly calls AddDomain(to_domain).
  //
  // Wildcards may not be used in the to_domain or the from_domain.
  bool AddShard(const StringPiece& to_domain,
                const StringPiece& comma_separated_shards,
                MessageHandler* handler);

  // Computes a domain shard based on a passed-in hash, returning true
  // if the domain was sharded.  Output argument 'sharded_domain' is
  // only updated if when the return value is true.
  //
  // The hash is an explicit uint32 so that we get the same shard for a
  // resource, whether the server is 32-bit or 64-bit.  If we have
  // 5 shards and used size_t for hashes, then we'd wind up with different
  // shards on 32-bit and 64-bit machines and that would reduce cacheability
  // of the sharded resources.
  bool ShardDomain(const StringPiece& domain_name, uint32 hash,
                   GoogleString* sharded_domain) const;

  // Merge the domains declared in src into this.  There are no exclusions, so
  // this is really just aggregating the mappings and authorizations declared in
  // both domains.  When the same domain is mapped in 'this' and 'src', 'src'
  // wins.
  void Merge(const DomainLawyer& src);

  // Determines whether a resource of the given domain name is going
  // to change due to RewriteDomain mapping or domain sharding.  Note
  // that this does not account for the actual domain shard selected.
  bool WillDomainChange(const StringPiece& domain_name) const;

  // Determines whether any resources might be domain-mapped, either
  // via sharding or rewriting.
  bool can_rewrite_domains() const { return can_rewrite_domains_; }

  // Visible for testing.
  int num_wildcarded_domains() const { return wildcarded_domains_.size(); }

  // Determines whether two domains have been declared as serving the same
  // content by the user, via Rewrite or Shard mapping.
  bool DoDomainsServeSameContent(const StringPiece& domain1,
                                 const StringPiece& domain2) const;

  // Finds domains rewritten to this domain. Includes only non-wildcarded
  // domains. comma_separated_from_domains is empty if no mapping found.
  void FindDomainsRewrittenTo(
      const GoogleUrl& domain_name,
      ConstStringStarVector* from_domains) const;

  // Computes a signature for the DomainLawyer object including containing
  // classes (Domain).
  GoogleString Signature() const;

  // Computes a string representation meant for debugging purposes only.
  // (The format might change in unpredictable ways and is not meant for
  //  machine consumption).
  // Each domain will appear on a separate line, and each line will be prefixed
  // with 'line_prefix'.
  GoogleString ToString(StringPiece line_prefix) const;

  // Version that's easier to call from debugger.
  GoogleString ToString() const { return ToString(StringPiece()); }

 private:
  class Domain;

  typedef bool (Domain::*SetDomainFn)(Domain* domain, MessageHandler* handler);

  static GoogleString NormalizeDomainName(const StringPiece& domain_name);

  static bool IsSchemeSafeToMapTo(const StringPiece& domain_name,
                                  bool allow_https_scheme);

  bool MapDomainHelper(
      const StringPiece& to_domain_name,
      const StringPiece& comma_separated_from_domains,
      SetDomainFn set_domain_fn,
      bool allow_wildcards,
      bool allow_map_to_https,
      bool authorize,
      MessageHandler* handler);

  bool MapUrlHelper(const Domain& from_domain,
                    const Domain& to_domain,
                    const GoogleUrl& gurl,
                    GoogleUrl* mapped_gurl) const;

  bool DomainNameToTwoProtocols(const StringPiece& domain_name,
                                GoogleString* http_url,
                                GoogleString* https_url);

  bool TwoProtocolDomainHelper(
      const StringPiece& to_domain_name,
      const StringPiece& from_domain_name,
      SetDomainFn set_domain_fn,
      bool authorize,
      MessageHandler* handler);

  Domain* AddDomainHelper(const StringPiece& domain_name,
                          bool warn_on_duplicate,
                          bool authorize,
                          bool is_proxy,
                          MessageHandler* handler);
  Domain* CloneAndAdd(const Domain* src);

  Domain* FindDomain(const GoogleUrl& gurl) const;

  // Map-order is important as ordering is taken into consideration while
  // constructing the signature of the domain lawyer.
  typedef std::map<GoogleString, Domain*> DomainMap;  // see AddDomainHelper
  DomainMap domain_map_;
  typedef std::vector<Domain*> DomainVector;          // see AddDomainHelper
  DomainVector wildcarded_domains_;
  bool can_rewrite_domains_;
  // If you add more fields here, please be sure to update Merge().

  DISALLOW_COPY_AND_ASSIGN(DomainLawyer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DOMAIN_LAWYER_H_
