// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CERT_DATABASE_H_
#define NET_BASE_CERT_DATABASE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/base/x509_certificate.h"

template <typename T> struct DefaultSingletonTraits;
template <class ObserverType> class ObserverListThreadSafe;

namespace net {

// This class provides cross-platform functions to verify and add user
// certificates, and to observe changes to the underlying certificate stores.

// TODO(gauravsh): This class could be augmented with methods
// for all operations that manipulate the underlying system
// certificate store.

class NET_EXPORT CertDatabase {
 public:
  // A CertDatabase::Observer will be notified on certificate database changes.
  // The change could be either a new user certificate is added or trust on
  // a certificate is changed.  Observers can register themselves
  // via CertDatabase::AddObserver, and can un-register with
  // CertDatabase::RemoveObserver.
  class NET_EXPORT Observer {
   public:
    virtual ~Observer() {}

    // Will be called when a new certificate is added.
    virtual void OnCertAdded(const X509Certificate* cert) {}

    // Will be called when a certificate is removed.
    virtual void OnCertRemoved(const X509Certificate* cert) {}

    // Will be called when a certificate's trust is changed.
    virtual void OnCertTrustChanged(const X509Certificate* cert) {}

   protected:
    Observer() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // Returns the CertDatabase singleton.
  static CertDatabase* GetInstance();

  // Check whether this is a valid user cert that we have the private key for.
  // Returns OK or a network error code such as ERR_CERT_CONTAINS_ERRORS.
  int CheckUserCert(X509Certificate* cert);

  // Store user (client) certificate. Assumes CheckUserCert has already passed.
  // Returns OK, or ERR_ADD_USER_CERT_FAILED if there was a problem saving to
  // the platform cert database, or possibly other network error codes.
  int AddUserCert(X509Certificate* cert);

  // Registers |observer| to receive notifications of certificate changes.  The
  // thread on which this is called is the thread on which |observer| will be
  // called back with notifications.
  void AddObserver(Observer* observer);

  // Unregisters |observer| from receiving notifications.  This must be called
  // on the same thread on which AddObserver() was called.
  void RemoveObserver(Observer* observer);

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Configures the current message loop to observe and forward events from
  // Keychain services. The MessageLoop must have an associated CFRunLoop,
  // which means that this must be called from a MessageLoop of TYPE_UI.
  void SetMessageLoopForKeychainEvents();
#endif

 private:
  friend struct DefaultSingletonTraits<CertDatabase>;

  CertDatabase();
  ~CertDatabase();

  // Broadcasts notifications to all registered observers.
  void NotifyObserversOfCertAdded(const X509Certificate* cert);
  void NotifyObserversOfCertRemoved(const X509Certificate* cert);
  void NotifyObserversOfCertTrustChanged(const X509Certificate* cert);

  const scoped_refptr<ObserverListThreadSafe<Observer> > observer_list_;

#if defined(USE_NSS) || (defined(OS_MACOSX) && !defined(OS_IOS))
  class Notifier;
  friend class Notifier;
  scoped_ptr<Notifier> notifier_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CertDatabase);
};

}  // namespace net

#endif  // NET_BASE_CERT_DATABASE_H_
