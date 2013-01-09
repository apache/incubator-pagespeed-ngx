// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TEST_ROOT_CERTS_H_
#define NET_BASE_TEST_ROOT_CERTS_H_
#pragma once

#include "base/lazy_instance.h"
#include "build/build_config.h"
#include "net/base/net_api.h"

#if defined(OS_WIN)
#include <windows.h>
#include <wincrypt.h>
#elif defined(OS_MACOSX)
#include <CoreFoundation/CFArray.h>
#include <Security/SecTrust.h>
#include "base/mac/scoped_cftyperef.h"
#elif defined(USE_NSS)
#include <list>
#endif

class FilePath;

namespace net {

class X509Certificate;

// TestRootCerts is a helper class for unit tests that is used to
// artificially mark a certificate as trusted, independent of the local
// machine configuration.
class NET_TEST TestRootCerts {
 public:
  // Obtains the Singleton instance to the trusted certificates.
  static TestRootCerts* GetInstance();

  // Returns true if an instance exists, without forcing an initialization.
  static bool HasInstance();

  // Marks |certificate| as trusted for X509Certificate::Verify(). Returns
  // false if the certificate could not be marked trusted.
  bool Add(X509Certificate* certificate);

  // Reads a single certificate from |file| and marks it as trusted. Returns
  // false if an error is encountered, such as being unable to read |file|
  // or more than one certificate existing in |file|.
  bool AddFromFile(const FilePath& file);

  // Clears the trusted status of any certificates that were previously
  // marked trusted via Add().
  void Clear();

  // Returns true if there are no certificates that have been marked trusted.
  bool IsEmpty() const;

#if defined(OS_MACOSX)
  CFArrayRef temporary_roots() const { return temporary_roots_; }

  // Modifies the root certificates of |trust_ref| to include the
  // certificates stored in |temporary_roots_|. If IsEmpty() is true, this
  // does not modify |trust_ref|.
  OSStatus FixupSecTrustRef(SecTrustRef trust_ref) const;
#elif defined(OS_WIN)
  HCERTSTORE temporary_roots() const { return temporary_roots_; }

  // Returns an HCERTCHAINENGINE suitable to be used for certificate
  // validation routines, or NULL to indicate that the default system chain
  // engine is appropriate. The caller is responsible for freeing the
  // returned HCERTCHAINENGINE.
  HCERTCHAINENGINE GetChainEngine() const;
#endif

 private:
  friend struct base::DefaultLazyInstanceTraits<TestRootCerts>;

  TestRootCerts();
  ~TestRootCerts();

  // Performs platform-dependent initialization.
  void Init();

#if defined(OS_MACOSX)
  base::mac::ScopedCFTypeRef<CFMutableArrayRef> temporary_roots_;
#elif defined(OS_WIN)
  HCERTSTORE temporary_roots_;
#elif defined(USE_NSS)
  // It is necessary to maintain a cache of the original certificate trust
  // settings, in order to restore them when Clear() is called.
  class TrustEntry;
  std::list<TrustEntry*> trust_cache_;
#endif

#if defined(OS_WIN) || defined(USE_OPENSSL)
  // True if there are no temporarily trusted root certificates.
  bool empty_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TestRootCerts);
};

}  // namespace net

#endif  // NET_BASE_TEST_ROOT_CERTS_H_
