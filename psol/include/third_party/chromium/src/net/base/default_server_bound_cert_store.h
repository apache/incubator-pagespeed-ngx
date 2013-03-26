// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DEFAULT_SERVER_BOUND_CERT_STORE_H_
#define NET_BASE_DEFAULT_SERVER_BOUND_CERT_STORE_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "net/base/net_export.h"
#include "net/base/server_bound_cert_store.h"

class Task;

namespace net {

// This class is the system for storing and retrieving server bound certs.
// Modeled after the CookieMonster class, it has an in-memory cert store,
// and synchronizes server bound certs to an optional permanent storage that
// implements the PersistentStore interface. The use case is described in
// http://balfanz.github.com/tls-obc-spec/draft-balfanz-tls-obc-00.html
//
// This class can be accessed by multiple threads. For example, it can be used
// by IO and server bound cert management UI.
class NET_EXPORT DefaultServerBoundCertStore : public ServerBoundCertStore {
 public:
  class PersistentStore;

  // The key for each ServerBoundCert* in ServerBoundCertMap is the
  // corresponding server.
  typedef std::map<std::string, ServerBoundCert*> ServerBoundCertMap;

  // The store passed in should not have had Init() called on it yet. This
  // class will take care of initializing it. The backing store is NOT owned by
  // this class, but it must remain valid for the duration of the
  // DefaultServerBoundCertStore's existence. If |store| is NULL, then no
  // backing store will be updated.
  explicit DefaultServerBoundCertStore(PersistentStore* store);

  virtual ~DefaultServerBoundCertStore();

  // Flush the backing store (if any) to disk and post the given task when done.
  // WARNING: THE CALLBACK WILL RUN ON A RANDOM THREAD. IT MUST BE THREAD SAFE.
  // It may be posted to the current thread, or it may run on the thread that
  // actually does the flushing. Your Task should generally post a notification
  // to the thread you actually want to be notified on.
  void FlushStore(const base::Closure& completion_task);

  // ServerBoundCertStore implementation.
  virtual bool GetServerBoundCert(
      const std::string& server_identifier,
      SSLClientCertType* type,
      base::Time* creation_time,
      base::Time* expiration_time,
      std::string* private_key_result,
      std::string* cert_result) OVERRIDE;
  virtual void SetServerBoundCert(
      const std::string& server_identifier,
      SSLClientCertType type,
      base::Time creation_time,
      base::Time expiration_time,
      const std::string& private_key,
      const std::string& cert) OVERRIDE;
  virtual void DeleteServerBoundCert(const std::string& server_identifier)
      OVERRIDE;
  virtual void DeleteAllCreatedBetween(base::Time delete_begin,
                                       base::Time delete_end) OVERRIDE;
  virtual void DeleteAll() OVERRIDE;
  virtual void GetAllServerBoundCerts(
      ServerBoundCertList* server_bound_certs) OVERRIDE;
  virtual int GetCertCount() OVERRIDE;
  virtual void SetForceKeepSessionState() OVERRIDE;

 private:
  static const size_t kMaxCerts;

  // Deletes all of the certs. Does not delete them from |store_|.
  void DeleteAllInMemory();

  // Called by all non-static functions to ensure that the cert store has
  // been initialized. This is not done during creating so it doesn't block
  // the window showing.
  // Note: this method should always be called with lock_ held.
  void InitIfNecessary() {
    if (!initialized_) {
      if (store_)
        InitStore();
      initialized_ = true;
    }
  }

  // Initializes the backing store and reads existing certs from it.
  // Should only be called by InitIfNecessary().
  void InitStore();

  // Deletes the cert for the specified server, if such a cert exists, from the
  // in-memory store. Deletes it from |store_| if |store_| is not NULL.
  void InternalDeleteServerBoundCert(const std::string& server);

  // Takes ownership of *cert.
  // Adds the cert for the specified server to the in-memory store. Deletes it
  // from |store_| if |store_| is not NULL.
  void InternalInsertServerBoundCert(const std::string& server_identifier,
                                     ServerBoundCert* cert);

  // Indicates whether the cert store has been initialized. This happens
  // Lazily in InitStoreIfNecessary().
  bool initialized_;

  scoped_refptr<PersistentStore> store_;

  ServerBoundCertMap server_bound_certs_;

  // Lock for thread-safety
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(DefaultServerBoundCertStore);
};

typedef base::RefCountedThreadSafe<DefaultServerBoundCertStore::PersistentStore>
    RefcountedPersistentStore;

class NET_EXPORT DefaultServerBoundCertStore::PersistentStore
    : public RefcountedPersistentStore {
 public:
  // Initializes the store and retrieves the existing certs. This will be
  // called only once at startup. Note that the certs are individually allocated
  // and that ownership is transferred to the caller upon return.
  virtual bool Load(
      std::vector<ServerBoundCert*>* certs) = 0;

  virtual void AddServerBoundCert(const ServerBoundCert& cert) = 0;

  virtual void DeleteServerBoundCert(const ServerBoundCert& cert) = 0;

  // When invoked, instructs the store to keep session related data on
  // destruction.
  virtual void SetForceKeepSessionState() = 0;

  // Flush the store and post the given Task when complete.
  virtual void Flush(const base::Closure& completion_task) = 0;

 protected:
  friend class base::RefCountedThreadSafe<PersistentStore>;

  PersistentStore();
  virtual ~PersistentStore();

 private:
  DISALLOW_COPY_AND_ASSIGN(PersistentStore);
};

}  // namespace net

#endif  // NET_DEFAULT_ORIGIN_BOUND_CERT_STORE_H_
