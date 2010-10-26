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
  // Note the the returned mapped domain name cannot be directly used in a
  // URL.  A GURL shold be composed via ResolvePath, below, which will
  // take care of any sharding.
  bool MapRequestToDomain(const GURL& original_request,
                          const StringPiece& resource_url,
                          std::string* mapped_domain_name,
                          MessageHandler* handler) const;

  // Note that ResolvePath does not perform any validation on the
  // mapped domain name -- that's assumed to have been supplied
  // by MapRequestToDomain above.
  GURL ResolvePath(const StringPiece& mapped_domain_name,
                   const StringPiece& path) const;

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
  // the 'from' domains overlap due to wildcards, this will not be detected
  // and the results will not be determined until MapRequestToDomain runs,
  // which will report the ambiguity and return false.
  //
  // TODO(jmarantz): implement this
  bool AddDomainMapping(const StringPiece& to_domain,
                        const StringPiece& comma_separated_from_domains,
                        MessageHandler* handler);

  // Specifies domain-sharding.  This implicitly calls AddDomain(to_domain).
  // The shard_pattern must include exactly one '%d'.
  //
  // Wildcards may not be used in the to_domain or the from_domain.
  //
  // TODO(jmarantz): implement this
  bool ShardDomain(const StringPiece& to_domain,
                   const StringPiece& shard_pattern,
                   int num_shards, MessageHandler* handler);

 private:
  class Domain;

  typedef std::map<std::string, Domain*> DomainMap;
  DomainMap domain_map_;
  typedef std::vector<Domain*> DomainVector;
  DomainVector wildcarded_domains_;

  DISALLOW_COPY_AND_ASSIGN(DomainLawyer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DOMAIN_LAWYER_H_
