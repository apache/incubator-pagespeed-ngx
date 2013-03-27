// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CONFIG_SERVICE_H_
#define NET_BASE_SSL_CONFIG_SERVICE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/string_piece.h"
#include "net/base/cert_status_flags.h"
#include "net/base/crl_set.h"
#include "net/base/net_export.h"
#include "net/base/x509_certificate.h"

namespace net {

// Various TLS/SSL ProtocolVersion values encoded as uint16
//      struct {
//          uint8 major;
//          uint8 minor;
//      } ProtocolVersion;
// The most significant byte is |major|, and the least significant byte
// is |minor|.
enum {
  SSL_PROTOCOL_VERSION_SSL3 = 0x0300,
  SSL_PROTOCOL_VERSION_TLS1 = 0x0301,
  SSL_PROTOCOL_VERSION_TLS1_1 = 0x0302,
  SSL_PROTOCOL_VERSION_TLS1_2 = 0x0303,
};

// A collection of SSL-related configuration settings.
struct NET_EXPORT SSLConfig {
  // Default to revocation checking.
  // Default to SSL 3.0 ~ default_version_max() on.
  SSLConfig();
  ~SSLConfig();

  // Returns true if |cert| is one of the certs in |allowed_bad_certs|.
  // The expected cert status is written to |cert_status|. |*cert_status| can
  // be NULL if user doesn't care about the cert status.
  bool IsAllowedBadCert(X509Certificate* cert, CertStatus* cert_status) const;

  // Same as above except works with DER encoded certificates instead
  // of X509Certificate.
  bool IsAllowedBadCert(const base::StringPiece& der_cert,
                        CertStatus* cert_status) const;

  // rev_checking_enabled is true if online certificate revocation checking is
  // enabled (i.e. OCSP and CRL fetching).
  //
  // Regardless of this flag, CRLSet checking is always enabled and locally
  // cached revocation information will be considered.
  bool rev_checking_enabled;

  // The minimum and maximum protocol versions that are enabled.
  // SSL 3.0 is 0x0300, TLS 1.0 is 0x0301, TLS 1.1 is 0x0302, and so on.
  // (Use the SSL_PROTOCOL_VERSION_xxx enumerators defined above.)
  // SSL 2.0 is not supported. If version_max < version_min, it means no
  // protocol versions are enabled.
  uint16 version_min;
  uint16 version_max;

  // Presorted list of cipher suites which should be explicitly prevented from
  // being used in addition to those disabled by the net built-in policy.
  //
  // By default, all cipher suites supported by the underlying SSL
  // implementation will be enabled except for:
  // - Null encryption cipher suites.
  // - Weak cipher suites: < 80 bits of security strength.
  // - FORTEZZA cipher suites (obsolete).
  // - IDEA cipher suites (RFC 5469 explains why).
  // - Anonymous cipher suites.
  // - ECDSA cipher suites on platforms that do not support ECDSA signed
  //   certificates, as servers may use the presence of such ciphersuites as a
  //   hint to send an ECDSA certificate.
  // The ciphers listed in |disabled_cipher_suites| will be removed in addition
  // to the above list.
  //
  // Though cipher suites are sent in TLS as "uint8 CipherSuite[2]", in
  // big-endian form, they should be declared in host byte order, with the
  // first uint8 occupying the most significant byte.
  // Ex: To disable TLS_RSA_WITH_RC4_128_MD5, specify 0x0004, while to
  // disable TLS_ECDH_ECDSA_WITH_RC4_128_SHA, specify 0xC002.
  //
  // Note: Not implemented when using Schannel/SSLClientSocketWin.
  std::vector<uint16> disabled_cipher_suites;

  bool cached_info_enabled;  // True if TLS cached info extension is enabled.
  bool channel_id_enabled;  // True if TLS channel ID extension is enabled.
  bool false_start_enabled;  // True if we'll use TLS False Start.

  // TODO(wtc): move the following members to a new SSLParams structure.  They
  // are not SSL configuration settings.

  struct NET_EXPORT CertAndStatus {
    CertAndStatus();
    ~CertAndStatus();

    std::string der_cert;
    CertStatus cert_status;
  };

  // Add any known-bad SSL certificate (with its cert status) to
  // |allowed_bad_certs| that should not trigger an ERR_CERT_* error when
  // calling SSLClientSocket::Connect.  This would normally be done in
  // response to the user explicitly accepting the bad certificate.
  std::vector<CertAndStatus> allowed_bad_certs;

  // True if we should send client_cert to the server.
  bool send_client_cert;

  bool verify_ev_cert;  // True if we should verify the certificate for EV.

  bool version_fallback;  // True if we are falling back to an older protocol
                          // version (one still needs to decrement
                          // version_max).

  // If cert_io_enabled is false, then certificate verification will not
  // result in additional HTTP requests. (For example: to fetch missing
  // intermediates or to perform OCSP/CRL fetches.) It also implies that online
  // revocation checking is disabled.
  // NOTE: currently only effective on Linux
  bool cert_io_enabled;

  // The list of application level protocols supported. If set, this will
  // enable Next Protocol Negotiation (if supported). The order of the
  // protocols doesn't matter expect for one case: if the server supports Next
  // Protocol Negotiation, but there is no overlap between the server's and
  // client's protocol sets, then the first protocol in this list will be
  // requested by the client.
  std::vector<std::string> next_protos;

  scoped_refptr<X509Certificate> client_cert;
};

// The interface for retrieving the SSL configuration.  This interface
// does not cover setting the SSL configuration, as on some systems, the
// SSLConfigService objects may not have direct access to the configuration, or
// live longer than the configuration preferences.
class NET_EXPORT SSLConfigService
    : public base::RefCountedThreadSafe<SSLConfigService> {
 public:
  // Observer is notified when SSL config settings have changed.
  class NET_EXPORT Observer {
   public:
    // Notify observers if SSL settings have changed.  We don't check all of the
    // data in SSLConfig, just those that qualify as a user config change.
    // The following settings are considered user changes:
    //     rev_checking_enabled
    //     version_min
    //     version_max
    //     disabled_cipher_suites
    //     channel_id_enabled
    //     false_start_enabled
    virtual void OnSSLConfigChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  SSLConfigService();

  // May not be thread-safe, should only be called on the IO thread.
  virtual void GetSSLConfig(SSLConfig* config) = 0;

  // Sets and gets the current, global CRL set.
  static void SetCRLSet(scoped_refptr<CRLSet> crl_set);
  static scoped_refptr<CRLSet> GetCRLSet();

  // Enables the TLS cached info extension, which allows the server to send
  // just a digest of its certificate chain.
  static void EnableCachedInfo();
  static bool cached_info_enabled();

  // Gets the default minimum protocol version.
  static uint16 default_version_min();

  // Sets and gets the default maximum protocol version.
  static void SetDefaultVersionMax(uint16 version_max);
  static uint16 default_version_max();

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
