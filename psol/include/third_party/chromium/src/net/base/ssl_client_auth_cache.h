// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CLIENT_AUTH_CACHE_H_
#define NET_BASE_SSL_CLIENT_AUTH_CACHE_H_

#include <string>
#include <map>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "net/base/cert_database.h"
#include "net/base/net_export.h"

namespace net {

class X509Certificate;

// The SSLClientAuthCache class is a simple cache structure to store SSL
// client certificates. Provides lookup, insertion, and deletion of entries.
// The parameter for doing lookups, insertions, and deletions is the server's
// host and port.
//
// TODO(wtc): This class is based on FtpAuthCache.  We can extract the common
// code to a template class.
class NET_EXPORT_PRIVATE SSLClientAuthCache : public CertDatabase::Observer {
 public:
  SSLClientAuthCache();
  virtual ~SSLClientAuthCache();

  // Checks for a client certificate preference for SSL server at |server|.
  // Returns true if a preference is found, and sets |*certificate| to the
  // desired client certificate. The desired certificate may be NULL, which
  // indicates a preference to not send any certificate to |server|.
  // If a certificate preference is not found, returns false.
  bool Lookup(const std::string& server,
              scoped_refptr<X509Certificate>* certificate);

  // Add a client certificate for |server| to the cache. If there is already
  // a client certificate for |server|, it will be overwritten. A NULL
  // |client_cert| indicates a preference that no client certificate should
  // be sent to |server|.
  void Add(const std::string& server, X509Certificate* client_cert);

  // Remove the client certificate for |server| from the cache, if one exists.
  void Remove(const std::string& server);

  // CertDatabase::Observer methods:
  virtual void OnCertAdded(const X509Certificate* cert) OVERRIDE;

 private:
  typedef std::string AuthCacheKey;
  typedef scoped_refptr<X509Certificate> AuthCacheValue;
  typedef std::map<AuthCacheKey, AuthCacheValue> AuthCacheMap;

  // internal representation of cache, an STL map.
  AuthCacheMap cache_;
};

}  // namespace net

#endif  // NET_BASE_SSL_CLIENT_AUTH_CACHE_H_
