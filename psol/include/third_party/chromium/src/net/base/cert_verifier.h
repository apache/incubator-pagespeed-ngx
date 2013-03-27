// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CERT_VERIFIER_H_
#define NET_BASE_CERT_VERIFIER_H_

#include <string>

#include "base/basictypes.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"

namespace net {

class BoundNetLog;
class CertVerifyResult;
class CRLSet;
class X509Certificate;

// CertVerifier represents a service for verifying certificates.
//
// CertVerifiers can handle multiple requests at a time. A simpler alternative
// for consumers that only have 1 outstanding request at a time is to create a
// SingleRequestCertVerifier wrapper around CertVerifier (which will
// automatically cancel the single request when it goes out of scope).
class NET_EXPORT CertVerifier {
 public:
  // Opaque pointer type used to cancel outstanding requests.
  typedef void* RequestHandle;

  enum VerifyFlags {
    // If set, enables online revocation checking via CRLs and OCSP for the
    // certificate chain.
    VERIFY_REV_CHECKING_ENABLED = 1 << 0,

    // If set, and the certificate being verified may be an EV certificate,
    // attempt to verify the certificate according to the EV processing
    // guidelines. In order to successfully verify a certificate as EV,
    // either an online or offline revocation check must be successfully
    // completed. To ensure it's possible to complete a revocation check,
    // callers should also specify either VERIFY_REV_CHECKING_ENABLED or
    // VERIFY_REV_CHECKING_ENABLED_EV_ONLY (to enable online checks), and
    // VERIFY_CERT_IO_ENABLED (to enable network fetches for online checks).
    VERIFY_EV_CERT = 1 << 1,

    // If set, permits NSS to use the network when verifying certificates,
    // such as to fetch missing intermediates or to check OCSP or CRLs.
    // TODO(rsleevi): http://crbug.com/143300 - Define this flag for all
    // verification engines with well-defined semantics, rather than being
    // NSS only.
    VERIFY_CERT_IO_ENABLED = 1 << 2,

    // If set, enables online revocation checking via CRLs or OCSP, but only
    // for certificates which may be EV, and only when VERIFY_EV_CERT is also
    // set.
    VERIFY_REV_CHECKING_ENABLED_EV_ONLY = 1 << 3,
  };

  // When the verifier is destroyed, all certificate verification requests are
  // canceled, and their completion callbacks will not be called.
  virtual ~CertVerifier() {}

  // Verifies the given certificate against the given hostname as an SSL server.
  // Returns OK if successful or an error code upon failure.
  //
  // The |*verify_result| structure, including the |verify_result->cert_status|
  // bitmask, is always filled out regardless of the return value.  If the
  // certificate has multiple errors, the corresponding status flags are set in
  // |verify_result->cert_status|, and the error code for the most serious
  // error is returned.
  //
  // |flags| is bitwise OR'd of VerifyFlags.
  // If VERIFY_REV_CHECKING_ENABLED is set in |flags|, certificate revocation
  // checking is performed.
  //
  // If VERIFY_EV_CERT is set in |flags| too, EV certificate verification is
  // performed.  If |flags| is VERIFY_EV_CERT (that is,
  // VERIFY_REV_CHECKING_ENABLED is not set), EV certificate verification will
  // not be performed.
  //
  // |crl_set| points to an optional CRLSet structure which can be used to
  // avoid revocation checks over the network.
  //
  // |callback| must not be null.  ERR_IO_PENDING is returned if the operation
  // could not be completed synchronously, in which case the result code will
  // be passed to the callback when available.
  //
  // If |out_req| is non-NULL, then |*out_req| will be filled with a handle to
  // the async request. This handle is not valid after the request has
  // completed.
  //
  // TODO(rsleevi): Move CRLSet* out of the CertVerifier signature.
  virtual int Verify(X509Certificate* cert,
                     const std::string& hostname,
                     int flags,
                     CRLSet* crl_set,
                     CertVerifyResult* verify_result,
                     const CompletionCallback& callback,
                     RequestHandle* out_req,
                     const BoundNetLog& net_log) = 0;

  // Cancels the specified request. |req| is the handle returned by Verify().
  // After a request is canceled, its completion callback will not be called.
  virtual void CancelRequest(RequestHandle req) = 0;

  // Creates a CertVerifier implementation that verifies certificates using
  // the preferred underlying cryptographic libraries.
  static CertVerifier* CreateDefault();
};

}  // namespace net

#endif  // NET_BASE_CERT_VERIFIER_H_
