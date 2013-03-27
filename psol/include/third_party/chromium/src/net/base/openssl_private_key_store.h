// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_OPENSSL_PRIVATE_KEY_STORE_H_
#define NET_BASE_OPENSSL_PRIVATE_KEY_STORE_H_

#include "base/basictypes.h"

typedef struct evp_pkey_st EVP_PKEY;

class GURL;

namespace net {

// Defines an abstract store for private keys; the OpenSSL library does not
// provide this service so it is left to individual platforms to provide it.
//
// The contract is that the private key will be stored in an appropriate secure
// system location, and be available to the SSLClientSocketOpenSSL when using a
// client certificate created against the associated public key for client
// authentication.
class OpenSSLPrivateKeyStore {
 public:
  // Platforms must define this factory function as appropriate.
  static OpenSSLPrivateKeyStore* GetInstance();

  virtual ~OpenSSLPrivateKeyStore() {}

  // Called to store a private key generated via <keygen> while visiting |url|.
  // Does not takes ownership of |pkey|, the caller reamins responsible to
  // EVP_PKEY_free it. (Internally, a copy maybe made or the reference count
  // incremented).
  // Returns false if an error occurred whilst attempting to store the key.
  virtual bool StorePrivateKey(const GURL& url, EVP_PKEY* pkey) = 0;

  // Given a |public_key| part returns the corresponding private key, or NULL
  // if no key found. Does NOT return ownership.
  virtual EVP_PKEY* FetchPrivateKey(EVP_PKEY* public_key) = 0;

 protected:
  OpenSSLPrivateKeyStore() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(OpenSSLPrivateKeyStore);
};

} // namespace net

#endif  // NET_BASE_OPENSSL_PRIVATE_KEY_STORE_H_
