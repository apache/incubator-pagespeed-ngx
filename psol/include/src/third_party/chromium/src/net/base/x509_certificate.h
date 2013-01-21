// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_X509_CERTIFICATE_H_
#define NET_BASE_X509_CERTIFICATE_H_
#pragma once

#include <string.h>

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/string_piece.h"
#include "base/time.h"
#include "net/base/net_api.h"
#include "net/base/x509_cert_types.h"

#if defined(OS_WIN)
#include <windows.h>
#include <wincrypt.h>
#elif defined(OS_MACOSX)
#include <CoreFoundation/CFArray.h>
#include <Security/SecBase.h>

#include "base/synchronization/lock.h"
#elif defined(USE_OPENSSL)
// Forward declaration; real one in <x509.h>
struct x509_st;
typedef struct x509_store_st X509_STORE;
#elif defined(USE_NSS)
// Forward declaration; real one in <cert.h>
struct CERTCertificateStr;
#endif

class Pickle;

namespace crypto {
class StringPiece;
class RSAPrivateKey;
}  // namespace crypto

namespace net {

class CertVerifyResult;

typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;

// X509Certificate represents an X.509 certificate used by SSL.
class NET_API X509Certificate
    : public base::RefCountedThreadSafe<X509Certificate> {
 public:
  // A handle to the certificate object in the underlying crypto library.
  // We assume that OSCertHandle is a pointer type on all platforms and
  // NULL is an invalid OSCertHandle.
#if defined(OS_WIN)
  typedef PCCERT_CONTEXT OSCertHandle;
#elif defined(OS_MACOSX)
  typedef SecCertificateRef OSCertHandle;
#elif defined(USE_OPENSSL)
  typedef struct x509_st* OSCertHandle;
#elif defined(USE_NSS)
  typedef struct CERTCertificateStr* OSCertHandle;
#else
  // TODO(ericroman): not implemented
  typedef void* OSCertHandle;
#endif

  typedef std::vector<OSCertHandle> OSCertHandles;

  // Predicate functor used in maps when X509Certificate is used as the key.
  class NET_API LessThan {
   public:
    bool operator() (X509Certificate* lhs,  X509Certificate* rhs) const;
  };

  // Where the certificate comes from.  The enumeration constants are
  // listed in increasing order of preference.
  enum Source {
    SOURCE_UNUSED = 0,            // The source_ member is not used.
    SOURCE_LONE_CERT_IMPORT = 1,  // From importing a certificate without
                                  // any intermediate CA certificates.
    SOURCE_FROM_CACHE = 2,        // From the disk cache - which contains
                                  // intermediate CA certificates, but may be
                                  // stale.
    SOURCE_FROM_NETWORK = 3,      // From the network.
  };

  enum VerifyFlags {
    VERIFY_REV_CHECKING_ENABLED = 1 << 0,
    VERIFY_EV_CERT = 1 << 1,
  };

  enum Format {
    // The data contains a single DER-encoded certificate, or a PEM-encoded
    // DER certificate with the PEM encoding block name of "CERTIFICATE".
    // Any subsequent blocks will be ignored.
    FORMAT_SINGLE_CERTIFICATE = 1 << 0,

    // The data contains a sequence of one or more PEM-encoded, DER
    // certificates, with the PEM encoding block name of "CERTIFICATE".
    // All PEM blocks will be parsed, until the first error is encountered.
    FORMAT_PEM_CERT_SEQUENCE = 1 << 1,

    // The data contains a PKCS#7 SignedData structure, whose certificates
    // member is to be used to initialize the certificate and intermediates.
    // The data may further be encoded using PEM, specifying block names of
    // either "PKCS7" or "CERTIFICATE".
    FORMAT_PKCS7 = 1 << 2,

    // Automatically detect the format.
    FORMAT_AUTO = FORMAT_SINGLE_CERTIFICATE | FORMAT_PEM_CERT_SEQUENCE |
                  FORMAT_PKCS7,
  };

  // PickleType is intended for deserializing certificates that were pickled
  // by previous releases as part of a net::HttpResponseInfo, which in version
  // 1 only contained a single certificate. When serializing certificates to a
  // new Pickle, PICKLETYPE_CERTIFICATE_CHAIN is always used.
  enum PickleType {
    // When reading a certificate from a Pickle, the Pickle only contains a
    // single certificate.
    PICKLETYPE_SINGLE_CERTIFICATE,

    // When reading a certificate from a Pickle, the Pickle contains the
    // the certificate plus any certificates that were stored in
    // |intermediate_ca_certificates_| at the time it was serialized.
    PICKLETYPE_CERTIFICATE_CHAIN,
  };

  // Creates a X509Certificate from the ground up.  Used by tests that simulate
  // SSL connections.
  X509Certificate(const std::string& subject, const std::string& issuer,
                  base::Time start_date, base::Time expiration_date);

  // Create an X509Certificate from a handle to the certificate object in the
  // underlying crypto library. |source| specifies where |cert_handle| comes
  // from.  Given two certificate handles for the same certificate, our
  // certificate cache prefers the handle from the network because our HTTP
  // cache isn't caching the corresponding intermediate CA certificates yet
  // (http://crbug.com/7065).
  // The returned pointer must be stored in a scoped_refptr<X509Certificate>.
  static X509Certificate* CreateFromHandle(OSCertHandle cert_handle,
                                           Source source,
                                           const OSCertHandles& intermediates);

  // Create an X509Certificate from a chain of DER encoded certificates. The
  // first certificate in the chain is the end-entity certificate to which a
  // handle is returned. The other certificates in the chain are intermediate
  // certificates. See the comment for |CreateFromHandle| about the |source|
  // argument.
  // The returned pointer must be stored in a scoped_refptr<X509Certificate>.
  static X509Certificate* CreateFromDERCertChain(
      const std::vector<base::StringPiece>& der_certs);

  // Create an X509Certificate from the DER-encoded representation.
  // Returns NULL on failure.
  //
  // The returned pointer must be stored in a scoped_refptr<X509Certificate>.
  static X509Certificate* CreateFromBytes(const char* data, int length);

  // Create an X509Certificate from the representation stored in the given
  // pickle.  The data for this object is found relative to the given
  // pickle_iter, which should be passed to the pickle's various Read* methods.
  // Returns NULL on failure.
  //
  // The returned pointer must be stored in a scoped_refptr<X509Certificate>.
  static X509Certificate* CreateFromPickle(const Pickle& pickle,
                                           void** pickle_iter,
                                           PickleType type);

  // Parses all of the certificates possible from |data|. |format| is a
  // bit-wise OR of Format, indicating the possible formats the
  // certificates may have been serialized as. If an error occurs, an empty
  // collection will be returned.
  static CertificateList CreateCertificateListFromBytes(const char* data,
                                                        int length,
                                                        int format);

  // Create a self-signed certificate containing the public key in |key|.
  // Subject, serial number and validity period are given as parameters.
  // The certificate is signed by the private key in |key|. The hashing
  // algorithm for the signature is SHA-1.
  //
  // |subject| is a distinguished name defined in RFC4514.
  //
  // An example:
  // CN=Michael Wong,O=FooBar Corporation,DC=foobar,DC=com
  //
  // SECURITY WARNING
  //
  // Using self-signed certificates has the following security risks:
  // 1. Encryption without authentication and thus vulnerable to
  //    man-in-the-middle attacks.
  // 2. Self-signed certificates cannot be revoked.
  //
  // Use this certificate only after the above risks are acknowledged.
  static X509Certificate* CreateSelfSigned(crypto::RSAPrivateKey* key,
                                           const std::string& subject,
                                           uint32 serial_number,
                                           base::TimeDelta valid_duration);

  // Appends a representation of this object to the given pickle.
  void Persist(Pickle* pickle);

  // The subject of the certificate.  For HTTPS server certificates, this
  // represents the web server.  The common name of the subject should match
  // the host name of the web server.
  const CertPrincipal& subject() const { return subject_; }

  // The issuer of the certificate.
  const CertPrincipal& issuer() const { return issuer_; }

  // Time period during which the certificate is valid.  More precisely, this
  // certificate is invalid before the |valid_start| date and invalid after
  // the |valid_expiry| date.
  // If we were unable to parse either date from the certificate (or if the cert
  // lacks either date), the date will be null (i.e., is_null() will be true).
  const base::Time& valid_start() const { return valid_start_; }
  const base::Time& valid_expiry() const { return valid_expiry_; }

  // The fingerprint of this certificate.
  const SHA1Fingerprint& fingerprint() const { return fingerprint_; }

  // Gets the DNS names in the certificate.  Pursuant to RFC 2818, Section 3.1
  // Server Identity, if the certificate has a subjectAltName extension of
  // type dNSName, this method gets the DNS names in that extension.
  // Otherwise, it gets the common name in the subject field.
  void GetDNSNames(std::vector<std::string>* dns_names) const;

  // Convenience method that returns whether this certificate has expired as of
  // now.
  bool HasExpired() const;

  // Returns true if this object and |other| represent the same certificate.
  bool Equals(const X509Certificate* other) const;

  // Returns intermediate certificates added via AddIntermediateCertificate().
  // Ownership follows the "get" rule: it is the caller's responsibility to
  // retain the elements of the result.
  const OSCertHandles& GetIntermediateCertificates() const {
    return intermediate_ca_certs_;
  }

  // Returns true if I already contain the given intermediate cert.
  bool HasIntermediateCertificate(OSCertHandle cert);

  // Returns true if I already contain all the given intermediate certs.
  bool HasIntermediateCertificates(const OSCertHandles& certs);

#if defined(OS_MACOSX)
  // Does this certificate's usage allow SSL client authentication?
  bool SupportsSSLClientAuth() const;

  // Do any of the given issuer names appear in this cert's chain of trust?
  bool IsIssuedBy(const std::vector<CertPrincipal>& valid_issuers);

  // Creates a security policy for certificates used as client certificates
  // in SSL.
  // If a policy is successfully created, it will be stored in
  // |*policy| and ownership transferred to the caller.
  static OSStatus CreateSSLClientPolicy(SecPolicyRef* policy);

  // Creates a security policy for certificates used by SSL servers.
  // |hostname| is an optionally-supplied string indicating the name to verify
  // the server certificate as; if it is empty, no hostname verification will
  // happen.
  // If a policy is successfully created, it will be stored in |*policy| and
  // ownership transferred to the caller.
  static OSStatus CreateSSLServerPolicy(const std::string& hostname,
                                        SecPolicyRef* policy);

  // Creates a security policy for basic X.509 validation. If the policy is
  // successfully created, it will be stored in |*policy| and ownership
  // transferred to the caller.
  static OSStatus CreateBasicX509Policy(SecPolicyRef* policy);

  // Creates security policies to control revocation checking (OCSP and CRL).
  // If |enable_revocation_checking| is false, the policies returned will be
  // explicitly disabled from accessing the network or the cache. This may be
  // used to override system settings regarding revocation checking.
  // If the policies are successfully created, they will be appended to
  // |policies|.
  static OSStatus CreateRevocationPolicies(bool enable_revocation_checking,
                                           CFMutableArrayRef policies);

  // Adds all available SSL client identity certs to the given vector.
  // |server_domain| is a hint for which domain the cert is to be sent to
  // (a cert previously specified as the default for that domain will be given
  // precedence and returned first in the output vector.)
  // If valid_issuers is non-empty, only certs that were transitively issued
  // by one of the given names will be included in the list.
  static bool GetSSLClientCertificates(
      const std::string& server_domain,
      const std::vector<CertPrincipal>& valid_issuers,
      CertificateList* certs);

  // Creates the chain of certs to use for this client identity cert.
  CFArrayRef CreateClientCertificateChain() const;
#endif

#if defined(OS_WIN)
  // Returns a handle to a global, in-memory certificate store. We use it for
  // two purposes:
  // 1. Import server certificates into this store so that we can verify and
  //    display the certificates using CryptoAPI.
  // 2. Copy client certificates from the "MY" system certificate store into
  //    this store so that we can close the system store when we finish
  //    searching for client certificates.
  static HCERTSTORE cert_store();
#endif

#if defined(USE_OPENSSL)
  // Returns a handle to a global, in-memory certificate store. We
  // use it for test code, e.g. importing the test server's certificate.
  static X509_STORE* cert_store();
#endif

  // Verifies the certificate against the given hostname.  Returns OK if
  // successful or an error code upon failure.
  //
  // The |*verify_result| structure, including the |verify_result->cert_status|
  // bitmask, is always filled out regardless of the return value.  If the
  // certificate has multiple errors, the corresponding status flags are set in
  // |verify_result->cert_status|, and the error code for the most serious
  // error is returned.
  //
  // |flags| is bitwise OR'd of VerifyFlags.
  // If VERIFY_REV_CHECKING_ENABLED is set in |flags|, certificate revocation
  // checking is performed.  If VERIFY_EV_CERT is set in |flags| too,
  // EV certificate verification is performed.
  int Verify(const std::string& hostname,
             int flags,
             CertVerifyResult* verify_result) const;

  // Verifies that |hostname| matches this certificate.
  // Does not verify that the certificate is valid, only that the certificate
  // matches this host.
  // Returns true if it matches.
  //
  // WARNING:  This function may return false negatives (for example, if
  //           |hostname| is an IP address literal) on some platforms.  Only
  //           use in cases where some false-positives are acceptible.
  bool VerifyNameMatch(const std::string& hostname) const;

  // This method returns the DER encoded certificate.
  // If the return value is true then the DER encoded certificate is available.
  // The content of the DER encoded certificate is written to |encoded|.
  bool GetDEREncoded(std::string* encoded);

  OSCertHandle os_cert_handle() const { return cert_handle_; }

  // Returns true if two OSCertHandles refer to identical certificates.
  static bool IsSameOSCert(OSCertHandle a, OSCertHandle b);

  // Creates an OS certificate handle from the BER-encoded representation.
  // Returns NULL on failure.
  static OSCertHandle CreateOSCertHandleFromBytes(const char* data,
                                                  int length);

  // Creates all possible OS certificate handles from |data| encoded in a
  // specific |format|. Returns an empty collection on failure.
  static OSCertHandles CreateOSCertHandlesFromBytes(
      const char* data, int length, Format format);

  // Duplicates (or adds a reference to) an OS certificate handle.
  static OSCertHandle DupOSCertHandle(OSCertHandle cert_handle);

  // Frees (or releases a reference to) an OS certificate handle.
  static void FreeOSCertHandle(OSCertHandle cert_handle);

 private:
  friend class base::RefCountedThreadSafe<X509Certificate>;
  friend class TestRootCerts;  // For unit tests
  FRIEND_TEST_ALL_PREFIXES(X509CertificateTest, Cache);
  FRIEND_TEST_ALL_PREFIXES(X509CertificateTest, IntermediateCertificates);
  FRIEND_TEST_ALL_PREFIXES(X509CertificateTest, SerialNumbers);
  FRIEND_TEST_ALL_PREFIXES(X509CertificateNameVerifyTest, VerifyHostname);

  // Construct an X509Certificate from a handle to the certificate object
  // in the underlying crypto library.
  X509Certificate(OSCertHandle cert_handle, Source source,
                  const OSCertHandles& intermediates);

  ~X509Certificate();

  // Common object initialization code.  Called by the constructors only.
  void Initialize();

#if defined(OS_WIN)
  bool CheckEV(PCCERT_CHAIN_CONTEXT chain_context,
               const char* policy_oid) const;
  static bool IsIssuedByKnownRoot(PCCERT_CHAIN_CONTEXT chain_context);
#endif
#if defined(OS_MACOSX)
  static bool IsIssuedByKnownRoot(CFArrayRef chain);
#endif
#if defined(USE_NSS)
  bool VerifyEV() const;
#endif
#if defined(USE_OPENSSL)
  // Resets the store returned by cert_store() to default state. Used by
  // TestRootCerts to undo modifications.
  static void ResetCertStore();
#endif

  // Calculates the SHA-1 fingerprint of the certificate.  Returns an empty
  // (all zero) fingerprint on failure.
  static SHA1Fingerprint CalculateFingerprint(OSCertHandle cert_handle);

  // Verifies that |hostname| matches one of the names in |cert_names|, based on
  // TLS name matching rules, specifically following http://tools.ietf.org/html/draft-saintandre-tls-server-id-check-09#section-4.4.3
  // The members of |cert_names| must have been extracted from the Subject CN or
  // SAN fields of a certificate.
  // WARNING:  This function may return false negatives (for example, if
  //           |hostname| is an IP address literal) on some platforms.  Only
  //           use in cases where some false-negatives are acceptible.
  static bool VerifyHostname(const std::string& hostname,
                             const std::vector<std::string>& cert_names);

  // Performs the platform-dependent part of the Verify() method, verifiying
  // this certificate against the platform's root CA certificates.
  //
  // Parameters and return value are as per Verify().
  int VerifyInternal(const std::string& hostname,
                     int flags,
                     CertVerifyResult* verify_result) const;

  // The serial number, DER encoded.
  // NOTE: keep this method private, used by IsBlacklisted only.  To simplify
  // IsBlacklisted, we strip the leading 0 byte of a serial number, used to
  // encode a positive DER INTEGER (a signed type) with a most significant bit
  // of 1.  Other code must not use this method for general purpose until this
  // is fixed.
  const std::string& serial_number() const { return serial_number_; }

  // IsBlacklisted returns true if this certificate is explicitly blacklisted.
  bool IsBlacklisted() const;

  // IsSHA1HashInSortedArray returns true iff |hash| is in |array|, a sorted
  // array of SHA1 hashes.
  static bool IsSHA1HashInSortedArray(const SHA1Fingerprint& hash,
                                      const uint8* array,
                                      size_t array_byte_len);

  // Reads a single certificate from |pickle| and returns a platform-specific
  // certificate handle. The format of the certificate stored in |pickle| is
  // not guaranteed to be the same across different underlying cryptographic
  // libraries, nor acceptable to CreateFromBytes(). Returns an invalid
  // handle, NULL, on failure.
  static OSCertHandle ReadOSCertHandleFromPickle(const Pickle& pickle,
                                                 void** pickle_iter);

  // Writes a single certificate to |pickle|. Returns false on failure.
  static bool WriteOSCertHandleToPickle(OSCertHandle handle, Pickle* pickle);

  // The subject of the certificate.
  CertPrincipal subject_;

  // The issuer of the certificate.
  CertPrincipal issuer_;

  // This certificate is not valid before |valid_start_|
  base::Time valid_start_;

  // This certificate is not valid after |valid_expiry_|
  base::Time valid_expiry_;

  // The fingerprint of this certificate.
  SHA1Fingerprint fingerprint_;

  // The serial number of this certificate, DER encoded.
  std::string serial_number_;

  // A handle to the certificate object in the underlying crypto library.
  OSCertHandle cert_handle_;

  // Untrusted intermediate certificates associated with this certificate
  // that may be needed for chain building.
  OSCertHandles intermediate_ca_certs_;

#if defined(OS_MACOSX)
  // Blocks multiple threads from verifying the cert simultaneously.
  // (Marked mutable because it's used in a const method.)
  mutable base::Lock verification_lock_;
#endif

  // Where the certificate comes from.
  Source source_;

  DISALLOW_COPY_AND_ASSIGN(X509Certificate);
};

}  // namespace net

#endif  // NET_BASE_X509_CERTIFICATE_H_
