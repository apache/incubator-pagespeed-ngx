// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SINGLE_REQUEST_CERT_VERIFIER_H_
#define NET_BASE_SINGLE_REQUEST_CERT_VERIFIER_H_

#include "net/base/cert_verifier.h"

namespace net {

// This class represents the task of verifying a certificate.  It wraps
// CertVerifier to verify only a single certificate at a time and cancels this
// request when going out of scope.
class SingleRequestCertVerifier {
 public:
  // |cert_verifier| must remain valid for the lifetime of |this|.
  explicit SingleRequestCertVerifier(CertVerifier* cert_verifier);

  // If a completion callback is pending when the verifier is destroyed, the
  // certificate verification is canceled, and the completion callback will
  // not be called.
  ~SingleRequestCertVerifier();

  // Verifies the given certificate, filling out the |verify_result| object
  // upon success. See CertVerifier::Verify() for details.
  int Verify(X509Certificate* cert,
             const std::string& hostname,
             int flags,
             CRLSet* crl_set,
             CertVerifyResult* verify_result,
             const CompletionCallback& callback,
             const BoundNetLog& net_log);

 private:
  // Callback for when the request to |cert_verifier_| completes, so we
  // dispatch to the user's callback.
  void OnVerifyCompletion(int result);

  // The actual certificate verifier that will handle the request.
  CertVerifier* const cert_verifier_;

  // The current request (if any).
  CertVerifier::RequestHandle cur_request_;
  CompletionCallback cur_request_callback_;

  DISALLOW_COPY_AND_ASSIGN(SingleRequestCertVerifier);
};

}  // namespace net

#endif  // NET_BASE_SINGLE_REQUEST_CERT_VERIFIER_H_
