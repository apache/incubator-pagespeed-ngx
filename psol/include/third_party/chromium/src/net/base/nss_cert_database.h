// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NSS_CERT_DATABASE_H_
#define NET_BASE_NSS_CERT_DATABASE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "net/base/cert_type.h"
#include "net/base/net_export.h"
#include "net/base/x509_certificate.h"

template <typename T> struct DefaultSingletonTraits;
template <class ObserverType> class ObserverListThreadSafe;

namespace net {

class CryptoModule;
typedef std::vector<scoped_refptr<CryptoModule> > CryptoModuleList;

// Provides functions to manipulate the NSS certificate stores.
class NET_EXPORT NSSCertDatabase {
 public:

  class NET_EXPORT Observer {
   public:
    virtual ~Observer() {}

    // Will be called when a new certificate is added.
    // Called with |cert| == NULL after importing a list of certificates
    // in ImportFromPKCS12().
    virtual void OnCertAdded(const X509Certificate* cert) {}

    // Will be called when a certificate is removed.
    virtual void OnCertRemoved(const X509Certificate* cert) {}

    // Will be called when a certificate's trust is changed.
    // Called with |cert| == NULL after importing a list of certificates
    // in ImportCACerts().
    virtual void OnCertTrustChanged(const X509Certificate* cert) {}

   protected:
    Observer() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // Stores per-certificate error codes for import failures.
  struct NET_EXPORT ImportCertFailure {
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
  // DISTRUSTED_* specifies that the cert should not be trusted for the given
  // usage, regardless of whether it would otherwise inherit trust from the
  // issuer chain.
  // Use TRUST_DEFAULT to inherit trust as normal.
  // NOTE: The actual constants are defined using an enum instead of static
  // consts due to compilation/linkage constraints with template functions.
  typedef uint32 TrustBits;
  enum {
    TRUST_DEFAULT         =      0,
    TRUSTED_SSL           = 1 << 0,
    TRUSTED_EMAIL         = 1 << 1,
    TRUSTED_OBJ_SIGN      = 1 << 2,
    DISTRUSTED_SSL        = 1 << 3,
    DISTRUSTED_EMAIL      = 1 << 4,
    DISTRUSTED_OBJ_SIGN   = 1 << 5,
  };

  static NSSCertDatabase* GetInstance();

  // Get a list of unique certificates in the certificate database (one
  // instance of all certificates).
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
  // If |is_extractable| is false, mark the private key as being unextractable
  // from the module.
  // Returns OK or a network error code such as ERR_PKCS12_IMPORT_BAD_PASSWORD
  // or ERR_PKCS12_IMPORT_ERROR. |imported_certs|, if non-NULL, returns a list
  // of certs that were imported.
  int ImportFromPKCS12(CryptoModule* module,
                       const std::string& data,
                       const string16& password,
                       bool is_extractable,
                       CertificateList* imported_certs);

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
                     TrustBits trust_bits,
                     ImportCertFailureList* not_imported);

  // Import server certificate.  The first cert should be the server cert.  Any
  // additional certs should be intermediate/CA certs and will be imported but
  // not given any trust.
  // Any certificates that could not be imported will be listed in
  // |not_imported|.
  // |trust_bits| can be set to explicitly trust or distrust the certificate, or
  // use TRUST_DEFAULT to inherit trust as normal.
  // Returns false if there is an internal error, otherwise true is returned and
  // |not_imported| should be checked for any certificates that were not
  // imported.
  bool ImportServerCert(const CertificateList& certificates,
                        TrustBits trust_bits,
                        ImportCertFailureList* not_imported);

  // Get trust bits for certificate.
  TrustBits GetCertTrust(const X509Certificate* cert, CertType type) const;

  // IsUntrusted returns true if |cert| is specifically untrusted. These
  // certificates are stored in the database for the specific purpose of
  // rejecting them.
  bool IsUntrusted(const X509Certificate* cert) const;

  // Set trust values for certificate.
  // Returns true on success or false on failure.
  bool SetCertTrust(const X509Certificate* cert,
                    CertType type,
                    TrustBits trust_bits);

  // Delete certificate and associated private key (if one exists).
  // |cert| is still valid when this function returns. Returns true on
  // success.
  bool DeleteCertAndKey(const X509Certificate* cert);

  // Check whether cert is stored in a readonly slot.
  bool IsReadOnly(const X509Certificate* cert) const;

  // Registers |observer| to receive notifications of certificate changes.  The
  // thread on which this is called is the thread on which |observer| will be
  // called back with notifications.
  void AddObserver(Observer* observer);

  // Unregisters |observer| from receiving notifications.  This must be called
  // on the same thread on which AddObserver() was called.
  void RemoveObserver(Observer* observer);

 private:
  friend struct DefaultSingletonTraits<NSSCertDatabase>;

  NSSCertDatabase();
  ~NSSCertDatabase();

  // Broadcasts notifications to all registered observers.
  void NotifyObserversOfCertAdded(const X509Certificate* cert);
  void NotifyObserversOfCertRemoved(const X509Certificate* cert);
  void NotifyObserversOfCertTrustChanged(const X509Certificate* cert);

  const scoped_refptr<ObserverListThreadSafe<Observer> > observer_list_;

  DISALLOW_COPY_AND_ASSIGN(NSSCertDatabase);
};

}  // namespace net

#endif  // NET_BASE_NSS_CERT_DATABASE_H_
