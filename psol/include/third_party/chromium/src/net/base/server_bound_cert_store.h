// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SERVER_BOUND_CERT_STORE_H_
#define NET_BASE_SERVER_BOUND_CERT_STORE_H_

#include <list>
#include <string>

#include "base/time.h"
#include "net/base/net_export.h"
#include "net/base/ssl_client_cert_type.h"

namespace net {

// An interface for storing and retrieving server bound certs.
// There isn't a domain bound certs spec yet, but the old origin bound
// certificates are specified in
// http://balfanz.github.com/tls-obc-spec/draft-balfanz-tls-obc-01.html.

// Owned only by a single ServerBoundCertService object, which is responsible
// for deleting it.
class NET_EXPORT ServerBoundCertStore {
 public:
  // The ServerBoundCert class contains a private key in addition to the server
  // cert, and cert type.
  class NET_EXPORT ServerBoundCert {
   public:
    ServerBoundCert();
    ServerBoundCert(const std::string& server_identifier,
                    SSLClientCertType type,
                    base::Time creation_time,
                    base::Time expiration_time,
                    const std::string& private_key,
                    const std::string& cert);
    ~ServerBoundCert();

    // Server identifier.  For domain bound certs, for instance "verisign.com".
    const std::string& server_identifier() const { return server_identifier_; }
    // TLS ClientCertificateType.
    SSLClientCertType type() const { return type_; }
    // The time the certificate was created, also the start of the certificate
    // validity period.
    base::Time creation_time() const { return creation_time_; }
    // The time after which this certificate is no longer valid.
    base::Time expiration_time() const { return expiration_time_; }
    // The encoding of the private key depends on the type.
    // rsa_sign: DER-encoded PrivateKeyInfo struct.
    // ecdsa_sign: DER-encoded EncryptedPrivateKeyInfo struct.
    const std::string& private_key() const { return private_key_; }
    // DER-encoded certificate.
    const std::string& cert() const { return cert_; }

   private:
    std::string server_identifier_;
    SSLClientCertType type_;
    base::Time creation_time_;
    base::Time expiration_time_;
    std::string private_key_;
    std::string cert_;
  };

  typedef std::list<ServerBoundCert> ServerBoundCertList;

  virtual ~ServerBoundCertStore() {}

  // TODO(rkn): File I/O may be required, so this should have an asynchronous
  // interface.
  // Returns true on success. |private_key_result| stores a DER-encoded
  // PrivateKeyInfo struct, |cert_result| stores a DER-encoded certificate,
  // |type| is the ClientCertificateType of the returned certificate,
  // |creation_time| stores the start of the validity period of the certificate
  // and |expiration_time| is the expiration time of the certificate.
  // Returns false if no server bound cert exists for the specified server.
  virtual bool GetServerBoundCert(
      const std::string& server_identifier,
      SSLClientCertType* type,
      base::Time* creation_time,
      base::Time* expiration_time,
      std::string* private_key_result,
      std::string* cert_result) = 0;

  // Adds a server bound cert and the corresponding private key to the store.
  virtual void SetServerBoundCert(
      const std::string& server_identifier,
      SSLClientCertType type,
      base::Time creation_time,
      base::Time expiration_time,
      const std::string& private_key,
      const std::string& cert) = 0;

  // Removes a server bound cert and the corresponding private key from the
  // store.
  virtual void DeleteServerBoundCert(const std::string& server_identifier) = 0;

  // Deletes all of the server bound certs that have a creation_date greater
  // than or equal to |delete_begin| and less than |delete_end|.  If a
  // base::Time value is_null, that side of the comparison is unbounded.
  virtual void DeleteAllCreatedBetween(base::Time delete_begin,
                                       base::Time delete_end) = 0;

  // Removes all server bound certs and the corresponding private keys from
  // the store.
  virtual void DeleteAll() = 0;

  // Returns all server bound certs and the corresponding private keys.
  virtual void GetAllServerBoundCerts(
      ServerBoundCertList* server_bound_certs) = 0;

  // Returns the number of certs in the store.
  // Public only for unit testing.
  virtual int GetCertCount() = 0;

  // When invoked, instructs the store to keep session related data on
  // destruction.
  virtual void SetForceKeepSessionState() = 0;
};

}  // namespace net

#endif  // NET_BASE_SERVER_BOUND_CERT_STORE_H_
