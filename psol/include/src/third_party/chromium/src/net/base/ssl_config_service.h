// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CONFIG_SERVICE_H_
#define NET_BASE_SSL_CONFIG_SERVICE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "net/base/net_api.h"
#include "net/base/x509_certificate.h"

namespace net {

// A collection of SSL-related configuration settings.
struct NET_API SSLConfig {
  // Default to revocation checking.
  // Default to SSL 3.0 on and TLS 1.0 on.
  SSLConfig();
  ~SSLConfig();

  // Returns true if |cert| is one of the certs in |allowed_bad_certs|.
  // The expected cert status is written to |cert_status|. |*cert_status| can
  // be NULL if user doesn't care about the cert status.
  bool IsAllowedBadCert(X509Certificate* cert, int* cert_status) const;

  bool rev_checking_enabled;  // True if server certificate revocation
                              // checking is enabled.
  // SSL 2.0 is not supported.
  bool ssl3_enabled;  // True if SSL 3.0 is enabled.
  bool tls1_enabled;  // True if TLS 1.0 is enabled.
  // True if we'll do async checks for certificate provenance using DNS.
  bool dns_cert_provenance_checking_enabled;

  // Cipher suites which should be explicitly prevented from being used in
  // addition to those disabled by the net built-in policy -- by default, all
  // cipher suites supported by the underlying SSL implementation will be
  // enabled except for:
  // - Null encryption cipher suites.
  // - Weak cipher suites: < 80 bits of security strength.
  // - FORTEZZA cipher suites (obsolete).
  // - IDEA cipher suites (RFC 5469 explains why).
  // - Anonymous cipher suites.
  // The ciphers listed in |disabled_cipher_suites| will be removed in addition
  // to the above statically defined disable list.
  //
  // Though cipher suites are sent in TLS as "uint8 CipherSuite[2]", in
  // big-endian form, they should be declared in host byte order, with the
  // first uint8 occupying the most significant byte.
  // Ex: To disable TLS_RSA_WITH_RC4_128_MD5, specify 0x0004, while to
  // disable TLS_ECDH_ECDSA_WITH_RC4_128_SHA, specify 0xC002.
  //
  // TODO(rsleevi): Not implemented when using Schannel.
  std::vector<uint16> disabled_cipher_suites;

  bool cached_info_enabled;  // True if TLS cached info extension is enabled.
  bool false_start_enabled;  // True if we'll use TLS False Start.

  // TODO(wtc): move the following members to a new SSLParams structure.  They
  // are not SSL configuration settings.

  struct NET_API CertAndStatus {
    CertAndStatus();
    ~CertAndStatus();

    scoped_refptr<X509Certificate> cert;
    int cert_status;
  };

  // Add any known-bad SSL certificate (with its cert status) to
  // |allowed_bad_certs| that should not trigger an ERR_CERT_* error when
  // calling SSLClientSocket::Connect.  This would normally be done in
  // response to the user explicitly accepting the bad certificate.
  std::vector<CertAndStatus> allowed_bad_certs;

  // True if we should send client_cert to the server.
  bool send_client_cert;

  bool verify_ev_cert;  // True if we should verify the certificate for EV.

  bool ssl3_fallback;  // True if we are falling back to SSL 3.0 (one still
                       // needs to clear tls1_enabled).

  // The list of application level protocols supported. If set, this will
  // enable Next Protocol Negotiation (if supported). This is a list of 8-bit
  // length prefixed strings. The order of the protocols doesn't matter expect
  // for one case: if the server supports Next Protocol Negotiation, but there
  // is no overlap between the server's and client's protocol sets, then the
  // first protocol in this list will be requested by the client.
  std::string next_protos;

  scoped_refptr<X509Certificate> client_cert;
};

// The interface for retrieving the SSL configuration.  This interface
// does not cover setting the SSL configuration, as on some systems, the
// SSLConfigService objects may not have direct access to the configuration, or
// live longer than the configuration preferences.
class NET_API SSLConfigService
    : public base::RefCountedThreadSafe<SSLConfigService> {
 public:
  // Observer is notified when SSL config settings have changed.
  class NET_API Observer {
   public:
    // Notify observers if SSL settings have changed.  We don't check all of the
    // data in SSLConfig, just those that qualify as a user config change.
    // The following settings are considered user changes:
    //     rev_checking_enabled
    //     ssl3_enabled
    //     tls1_enabled
    virtual void OnSSLConfigChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  SSLConfigService();

  // Create an instance of SSLConfigService which retrieves the configuration
  // from the system SSL configuration, or an instance of
  // SSLConfigServiceDefaults if the current system does not have a system SSL
  // configuration.  Note: this does not handle SSLConfigService implementations
  // that are not native to their platform, such as preference-backed ones.
  static SSLConfigService* CreateSystemSSLConfigService();

  // May not be thread-safe, should only be called on the IO thread.
  virtual void GetSSLConfig(SSLConfig* config) = 0;

  // Returns true if the given hostname is known to be incompatible with TLS
  // False Start.
  static bool IsKnownFalseStartIncompatibleServer(const std::string& hostname);

  // Disables False Start in SSL connections.
  static void DisableFalseStart();
  // True if we use False Start for SSL and TLS.
  static bool false_start_enabled();

  // Enables DNS side checks for certificates.
  static void EnableDNSCertProvenanceChecking();
  static bool dns_cert_provenance_checking_enabled();

  // Enables the TLS cached info extension, which allows the server to send
  // just a digest of its certificate chain.
  static void EnableCachedInfo();
  static bool cached_info_enabled();

  // Is SNI available in this configuration?
  static bool IsSNIAvailable(SSLConfigService* service);

  // Add an observer of this service.
  void AddObserver(Observer* observer);

  // Remove an observer of this service.
  void RemoveObserver(Observer* observer);

 protected:
  friend class base::RefCountedThreadSafe<SSLConfigService>;

  virtual ~SSLConfigService();

  // SetFlags sets the values of several flags based on global configuration.
  static void SetSSLConfigFlags(SSLConfig* ssl_config);

  // Process before/after config update.
  void ProcessConfigUpdate(const SSLConfig& orig_config,
                           const SSLConfig& new_config);

 private:
  ObserverList<Observer> observer_list_;
};

}  // namespace net

#endif  // NET_BASE_SSL_CONFIG_SERVICE_H_
