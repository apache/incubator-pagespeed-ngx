// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_EV_ROOT_CA_METADATA_H_
#define NET_BASE_EV_ROOT_CA_METADATA_H_

#include "build/build_config.h"

#if defined(USE_NSS) || defined(OS_IOS)
#include <secoidt.h>
#endif

#include <map>
#include <set>
#include <string>
#include <vector>

#include "net/base/net_export.h"
#include "net/base/x509_certificate.h"

namespace base {
template <typename T>
struct DefaultLazyInstanceTraits;
}  // namespace base

namespace net {

// A singleton.  This class stores the meta data of the root CAs that issue
// extended-validation (EV) certificates.
class NET_EXPORT_PRIVATE EVRootCAMetadata {
 public:
#if defined(USE_NSS) || defined(OS_IOS)
  typedef SECOidTag PolicyOID;
#elif defined(OS_WIN)
  typedef const char* PolicyOID;
#endif

  static EVRootCAMetadata* GetInstance();

#if defined(USE_NSS) || defined(OS_WIN) || defined(OS_IOS)
  // Returns true if policy_oid is an EV policy OID of some root CA.
  bool IsEVPolicyOID(PolicyOID policy_oid) const;

  // Returns true if the root CA with the given certificate fingerprint has
  // the EV policy OID policy_oid.
  bool HasEVPolicyOID(const SHA1HashValue& fingerprint,
                      PolicyOID policy_oid) const;
#endif

  // AddEVCA adds an EV CA to the list of known EV CAs with the given policy.
  // |policy| is expressed as a string of dotted numbers. It returns true on
  // success.
  bool AddEVCA(const SHA1HashValue& fingerprint, const char* policy);

  // RemoveEVCA removes an EV CA that was previously added by AddEVCA. It
  // returns true on success.
  bool RemoveEVCA(const SHA1HashValue& fingerprint);

 private:
  friend struct base::DefaultLazyInstanceTraits<EVRootCAMetadata>;

  EVRootCAMetadata();
  ~EVRootCAMetadata();

#if defined(USE_NSS) || defined(OS_IOS)
  typedef std::map<SHA1HashValue, std::vector<PolicyOID>,
                   SHA1HashValueLessThan> PolicyOIDMap;

  // RegisterOID registers |policy|, a policy OID in dotted string form, and
  // writes the memoized form to |*out|. It returns true on success.
  static bool RegisterOID(const char* policy, PolicyOID* out);

  PolicyOIDMap ev_policy_;
  std::set<PolicyOID> policy_oids_;
#elif defined(OS_WIN)
  typedef std::map<SHA1HashValue, std::string,
                   SHA1HashValueLessThan> ExtraEVCAMap;

  // extra_cas_ contains any EV CA metadata that was added at runtime.
  ExtraEVCAMap extra_cas_;
#endif

  DISALLOW_COPY_AND_ASSIGN(EVRootCAMetadata);
};

}  // namespace net

#endif  // NET_BASE_EV_ROOT_CA_METADATA_H_
