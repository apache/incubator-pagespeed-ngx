// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CERT_DATABASE_H_
#define NET_BASE_CERT_DATABASE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "net/base/cert_type.h"
#include "net/base/net_api.h"

namespace net {

class CryptoModule;
typedef std::vector<scoped_refptr<CryptoModule> > CryptoModuleList;
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;


// This class provides functions to manipulate the local
// certificate store.

// TODO(gauravsh): This class could be augmented with methods
// for all operations that manipulate the underlying system
// certificate store.

class NET_API CertDatabase {
 public:

  // A CertDatabase::Observer will be notified on certificate database changes.
  // The change could be either a new user certificate is added or trust on
  // a certificate is changed.  Observers can register themselves
  // via CertDatabase::AddObserver, and can un-register with
  // CertDatabase::RemoveObserver.
  class NET_API Observer {
   public:
    virtual ~Observer() {}

    // Will be called when a new user certificate is added.
    // Note that |cert| could be NULL when called.
    virtual void OnUserCertAdded(const X509Certificate* cert) {}

    // Will be called when a certificate's trust is changed.
    // Note that |cert| could be NULL when called.
    virtual void OnCertTrustChanged(const X509Certificate* cert) {}

   protected:
    Observer() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // Stores per-certificate error codes for import failures.
  struct NET_API ImportCertFailure {
   public:
    ImportCertFailure(X509Certificate* cert, int err);
    ~ImportCertFailure();

    scoped_refptr<X509Certificate> certificate;
    int net_error;
  };
  typedef std::vector<ImportCertFailure> ImportCertFailureList;

  // Constants that define which usages a certificate is trusted for.
  // They are used in combination with CertType to specify trust for each type
  // of certificate.
  // For a CA_CERT, they specify that the CA is trusted for issuing server and
  // client certs of each type.
  // For SERVER_CERT, only TRUSTED_SSL makes sense, and specifies the cert is
  // trusted as a server.
  // For EMAIL_CERT, only TRUSTED_EMAIL makes sense, and specifies the cert is
  // trusted for email.
  enum {
    UNTRUSTED        =      0,
    TRUSTED_SSL      = 1 << 0,
    TRUSTED_EMAIL    = 1 << 1,
    TRUSTED_OBJ_SIGN = 1 << 2,
  };

  CertDatabase();

  // Check whether this is a valid user cert that we have the private key for.
  // Returns OK or a network error code such as ERR_CERT_CONTAINS_ERRORS.
  int CheckUserCert(X509Certificate* cert);

  // Store user (client) certificate. Assumes CheckUserCert has already passed.
  // Returns OK, or ERR_ADD_USER_CERT_FAILED if there was a problem saving to
  // the platform cert database, or possibly other network error codes.
  int AddUserCert(X509Certificate* cert);

#if defined(USE_NSS) || defined(USE_OPENSSL)
  // Get a list of unique certificates in the certificate database.  (One
  // instance of all certificates.)
  void ListCerts(CertificateList* certs);

  // Get the default module for public key data.
  // The returned pointer must be stored in a scoped_refptr<CryptoModule>.
  CryptoModule* GetPublicModule() const;

  // Get the default module for private key or mixed private/public key data.
  // The returned pointer must be stored in a scoped_refptr<CryptoModule>.
  CryptoModule* GetPrivateModule() const;

  // Get all modules.
  // If |need_rw| is true, only writable modules will be returned.
  void ListModules(CryptoModuleList* modules, bool need_rw) const;

  // Import certificates and private keys from PKCS #12 blob into the module.
  // Returns OK or a network error code such as ERR_PKCS12_IMPORT_BAD_PASSWORD
  // or ERR_PKCS12_IMPORT_ERROR.
  int ImportFromPKCS12(CryptoModule* module,
                       const std::string& data,
                       const string16& password);

  // Export the given certificates and private keys into a PKCS #12 blob,
  // storing into |output|.
  // Returns the number of certificates successfully exported.
  int ExportToPKCS12(const CertificateList& certs, const string16& password,
                     std::string* output) const;

  // Uses similar logic to nsNSSCertificateDB::handleCACertDownload to find the
  // root.  Assumes the list is an ordered hierarchy with the root being either
  // the first or last element.
  // TODO(mattm): improve this to handle any order.
  X509Certificate* FindRootInList(const CertificateList& certificates) const;

  // Import CA certificates.
  // Tries to import all the certificates given.  The root will be trusted
  // according to |trust_bits|.  Any certificates that could not be imported
  // will be listed in |not_imported|.
  // Returns false if there is an internal error, otherwise true is returned and
  // |not_imported| should be checked for any certificates that were not
  // imported.
  bool ImportCACerts(const CertificateList& certificates,
                     unsigned int trust_bits,
                     ImportCertFailureList* not_imported);

  // Import server certificate.  The first cert should be the server cert.  Any
  // additional certs should be intermediate/CA certs and will be imported but
  // not given any trust.
  // Any certificates that could not be imported will be listed in
  // |not_imported|.
  // Returns false if there is an internal error, otherwise true is returned and
  // |not_imported| should be checked for any certificates that were not
  // imported.
  bool ImportServerCert(const CertificateList& certificates,
                        ImportCertFailureList* not_imported);

  // Get trust bits for certificate.
  unsigned int GetCertTrust(const X509Certificate* cert, CertType type) const;

  // Set trust values for certificate.
  // Returns true on success or false on failure.
  bool SetCertTrust(const X509Certificate* cert,
                    CertType type,
                    unsigned int trust_bits);

  // Delete certificate and associated private key (if one exists).
  // Returns true on success or false on failure.
  // |cert| is still valid when this function returns.
  bool DeleteCertAndKey(const X509Certificate* cert);

  // Check whether cert is stored in a readonly slot.
  bool IsReadOnly(const X509Certificate* cert) const;
#endif

  // Registers |observer| to receive notifications of certificate changes.  The
  // thread on which this is called is the thread on which |observer| will be
  // called back with notifications.
  static void AddObserver(Observer* observer);

  // Unregisters |observer| from receiving notifications.  This must be called
  // on the same thread on which AddObserver() was called.
  static void RemoveObserver(Observer* observer);

 private:
  // Broadcasts notifications to all registered observers.
  static void NotifyObserversOfUserCertAdded(const X509Certificate* cert);
  static void NotifyObserversOfCertTrustChanged(const X509Certificate* cert);

  DISALLOW_COPY_AND_ASSIGN(CertDatabase);
};

}  // namespace net

#endif  // NET_BASE_CERT_DATABASE_H_
