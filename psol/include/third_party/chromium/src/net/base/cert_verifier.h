// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CERT_VERIFIER_H_
#define NET_BASE_CERT_VERIFIER_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "net/base/cert_database.h"
#include "net/base/cert_verify_result.h"
#include "net/base/completion_callback.h"
#include "net/base/net_api.h"
#include "net/base/x509_cert_types.h"

namespace net {

class CertVerifierJob;
class CertVerifierWorker;
class X509Certificate;

// CachedCertVerifyResult contains the result of a certificate verification.
struct CachedCertVerifyResult {
  CachedCertVerifyResult();
  ~CachedCertVerifyResult();

  // Returns true if |current_time| is greater than or equal to |expiry|.
  bool HasExpired(base::Time current_time) const;

  int error;  // The return value of CertVerifier::Verify.
  CertVerifyResult result;  // The output of CertVerifier::Verify.

  // The time at which the certificate verification result expires.
  base::Time expiry;
};

// CertVerifier represents a service for verifying certificates.
//
// CertVerifier can handle multiple requests at a time, so when canceling a
// request the RequestHandle that was returned by Verify() needs to be
// given.  A simpler alternative for consumers that only have 1 outstanding
// request at a time is to create a SingleRequestCertVerifier wrapper around
// CertVerifier (which will automatically cancel the single request when it
// goes out of scope).
class NET_API CertVerifier : NON_EXPORTED_BASE(public base::NonThreadSafe),
                             public CertDatabase::Observer {
 public:
  // Opaque type used to cancel a request.
  typedef void* RequestHandle;

  // CertVerifier must not call base::Time::Now() directly.  It must call
  // time_service_->Now().  This allows unit tests to mock the current time.
  class TimeService {
   public:
    virtual ~TimeService() {}

    virtual base::Time Now() = 0;
  };

  CertVerifier();

  // Used by unit tests to mock the current time.  Takes ownership of
  // |time_service|.
  explicit CertVerifier(TimeService* time_service);

  // When the verifier is destroyed, all certificate verifications requests are
  // canceled, and their completion callbacks will not be called.
  virtual ~CertVerifier();

  // Verifies the given certificate against the given hostname.  Returns OK if
  // successful or an error code upon failure.
  //
  // The |*verify_result| structure, including the |verify_result->cert_status|
  // bitmask, is always filled out regardless of the return value.  If the
  // certificate has multiple errors, the corresponding status flags are set in
  // |verify_result->cert_status|, and the error code for the most serious
  // error is returned.
  //
  // |flags| is bitwise OR'd of X509Certificate::VerifyFlags.
  // If VERIFY_REV_CHECKING_ENABLED is set in |flags|, certificate revocation
  // checking is performed.
  //
  // If VERIFY_EV_CERT is set in |flags| too, EV certificate verification is
  // performed.  If |flags| is VERIFY_EV_CERT (that is,
  // VERIFY_REV_CHECKING_ENABLED is not set), EV certificate verification will
  // not be performed.
  //
  // |callback| must not be null.  ERR_IO_PENDING is returned if the operation
  // could not be completed synchronously, in which case the result code will
  // be passed to the callback when available.
  //
  // If |out_req| is non-NULL, then |*out_req| will be filled with a handle to
  // the async request. This handle is not valid after the request has
  // completed.
  int Verify(X509Certificate* cert,
             const std::string& hostname,
             int flags,
             CertVerifyResult* verify_result,
             CompletionCallback* callback,
             RequestHandle* out_req);

  // Cancels the specified request. |req| is the handle returned by Verify().
  // After a request is canceled, its completion callback will not be called.
  void CancelRequest(RequestHandle req);

  // Clears the verification result cache.
  void ClearCache();

  size_t GetCacheSize() const;

  uint64 requests() const { return requests_; }
  uint64 cache_hits() const { return cache_hits_; }
  uint64 inflight_joins() const { return inflight_joins_; }

 private:
  friend class CertVerifierWorker;  // Calls HandleResult.

  // Input parameters of a certificate verification request.
  struct RequestParams {
    bool operator==(const RequestParams& other) const {
      // |flags| is compared before |cert_fingerprint| and |hostname| under
      // assumption that integer comparisons are faster than memory and string
      // comparisons.
      return (flags == other.flags &&
              memcmp(cert_fingerprint.data, other.cert_fingerprint.data,
                     sizeof(cert_fingerprint.data)) == 0 &&
              hostname == other.hostname);
    }

    bool operator<(const RequestParams& other) const {
      // |flags| is compared before |cert_fingerprint| and |hostname| under
      // assumption that integer comparisons are faster than memory and string
      // comparisons.
      if (flags != other.flags)
        return flags < other.flags;
      int rv = memcmp(cert_fingerprint.data, other.cert_fingerprint.data,
                      sizeof(cert_fingerprint.data));
      if (rv != 0)
        return rv < 0;
      return hostname < other.hostname;
    }

    SHA1Fingerprint cert_fingerprint;
    std::string hostname;
    int flags;
  };

  void HandleResult(X509Certificate* cert,
                    const std::string& hostname,
                    int flags,
                    int error,
                    const CertVerifyResult& verify_result);

  // CertDatabase::Observer methods:
  virtual void OnCertTrustChanged(const X509Certificate* cert);

  // cache_ maps from a request to a cached result. The cached result may
  // have expired and the size of |cache_| must be <= kMaxCacheEntries.
  std::map<RequestParams, CachedCertVerifyResult> cache_;

  // inflight_ maps from a request to an active verification which is taking
  // place.
  std::map<RequestParams, CertVerifierJob*> inflight_;

  scoped_ptr<TimeService> time_service_;

  uint64 requests_;
  uint64 cache_hits_;
  uint64 inflight_joins_;

  DISALLOW_COPY_AND_ASSIGN(CertVerifier);
};

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
             CertVerifyResult* verify_result,
             CompletionCallback* callback);

 private:
  // Callback for when the request to |cert_verifier_| completes, so we
  // dispatch to the user's callback.
  void OnVerifyCompletion(int result);

  // The actual certificate verifier that will handle the request.
  CertVerifier* const cert_verifier_;

  // The current request (if any).
  CertVerifier::RequestHandle cur_request_;
  CompletionCallback* cur_request_callback_;

  // Completion callback for when request to |cert_verifier_| completes.
  CompletionCallbackImpl<SingleRequestCertVerifier> callback_;

  DISALLOW_COPY_AND_ASSIGN(SingleRequestCertVerifier);
};

}  // namespace net

#endif  // NET_BASE_CERT_VERIFIER_H_
