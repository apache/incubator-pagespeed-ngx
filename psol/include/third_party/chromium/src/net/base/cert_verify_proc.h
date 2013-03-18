// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CERT_VERIFY_PROC_H_
#define NET_BASE_CERT_VERIFY_PROC_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/base/x509_cert_types.h"

namespace net {

class CertVerifyResult;
class CRLSet;
class X509Certificate;

// Class to perform certificate path building and verification for various
// certificate uses. All methods of this class must be thread-safe, as they
// may be called from various non-joinable worker threads.
class NET_EXPORT CertVerifyProc
    : public base::RefCountedThreadSafe<CertVerifyProc> {
 public:
  // Creates and returns the default CertVerifyProc.
  static CertVerifyProc* CreateDefault();

  // Verifies the certificate against the given hostname as an SSL server
  // certificate. Returns OK if successful or an error code upon failure.
  //
  // The |*verify_result| structure, including the |verify_result->cert_status|
  // bitmask, is always filled out regardless of the return value. If the
  // certificate has multiple errors, the corresponding status flags are set in
  // |verify_result->cert_status|, and the error code for the most serious
  // error is returned.
  //
  // |flags| is bitwise OR'd of VerifyFlags:
  //
  // If VERIFY_REV_CHECKING_ENABLED is set in |flags|, online certificate
  // revocation checking is performed (i.e. OCSP and downloading CRLs). CRLSet
  // based revocation checking is always enabled, regardless of this flag, if
  // |crl_set| is given.
  //
  // If VERIFY_EV_CERT is set in |flags| too, EV certificate verification is
  // performed.
  //
  // |crl_set| points to an optional CRLSet structure which can be used to
  // avoid revocation checks over the network.
  int Verify(X509Certificate* cert,
             const std::string& hostname,
             int flags,
             CRLSet* crl_set,
             CertVerifyResult* verify_result);

 protected:
  friend class base::RefCountedThreadSafe<CertVerifyProc>;
  FRIEND_TEST_ALL_PREFIXES(CertVerifyProcTest, DigiNotarCerts);

  CertVerifyProc();
  virtual ~CertVerifyProc();

 private:
  // Performs the actual verification using the desired underlying
  // cryptographic library.
  virtual int VerifyInternal(X509Certificate* cert,
                             const std::string& hostname,
                             int flags,
                             CRLSet* crl_set,
                             CertVerifyResult* verify_result) = 0;

  // Returns true if |cert| is explicitly blacklisted.
  static bool IsBlacklisted(X509Certificate* cert);

  // IsPublicKeyBlacklisted returns true iff one of |public_key_hashes| (which
  // are hashes of SubjectPublicKeyInfo structures) is explicitly blocked.
  static bool IsPublicKeyBlacklisted(const HashValueVector& public_key_hashes);
};

}  // namespace net

#endif  // NET_BASE_CERT_VERIFY_PROC_H_
