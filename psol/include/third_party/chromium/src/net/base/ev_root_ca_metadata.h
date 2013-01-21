// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_EV_ROOT_CA_METADATA_H_
#define NET_BASE_EV_ROOT_CA_METADATA_H_
#pragma once

#include "build/build_config.h"

#if defined(USE_NSS)
#include <secoidt.h>
#endif

#include <map>
#include <vector>

#include "net/base/x509_certificate.h"

namespace base {
template <typename T>
struct DefaultLazyInstanceTraits;
}  // namespace base

namespace net {

// A singleton.  This class stores the meta data of the root CAs that issue
// extended-validation (EV) certificates.
class EVRootCAMetadata {
 public:
#if defined(USE_NSS)
  typedef SECOidTag PolicyOID;
#else
  typedef const char* PolicyOID;
#endif

  static EVRootCAMetadata* GetInstance();

  // If the root CA cert has an EV policy OID, returns true and stores the
  // policy OID in *policy_oid.  Otherwise, returns false.
  bool GetPolicyOID(const SHA1Fingerprint& fingerprint,
                    PolicyOID* policy_oid) const;

  const PolicyOID* GetPolicyOIDs() const { return &policy_oids_[0]; }
#if defined(OS_WIN)
  int NumPolicyOIDs() const { return num_policy_oids_; }
#else
  int NumPolicyOIDs() const { return policy_oids_.size(); }
#endif

  // Returns true if policy_oid is an EV policy OID of some root CA.
  bool IsEVPolicyOID(PolicyOID policy_oid) const;

  // Returns true if the root CA with the given certificate fingerprint has
  // the EV policy OID policy_oid.
  bool HasEVPolicyOID(const SHA1Fingerprint& fingerprint,
                      PolicyOID policy_oid) const;

 private:
  friend struct base::DefaultLazyInstanceTraits<EVRootCAMetadata>;

  typedef std::map<SHA1Fingerprint, PolicyOID,
                   SHA1FingerprintLessThan> PolicyOidMap;

  EVRootCAMetadata();
  ~EVRootCAMetadata();

  static bool PolicyOIDsAreEqual(PolicyOID a, PolicyOID b);

  // Maps an EV root CA cert's SHA-1 fingerprint to its EV policy OID.
  PolicyOidMap ev_policy_;

#if defined(OS_WIN)
  static const PolicyOID policy_oids_[];
  int num_policy_oids_;
#else
  std::vector<PolicyOID> policy_oids_;
#endif

  DISALLOW_COPY_AND_ASSIGN(EVRootCAMetadata);
};

}  // namespace net

#endif  // NET_BASE_EV_ROOT_CA_METADATA_H_
