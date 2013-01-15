// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SCOPED_CERT_CHAIN_CONTEXT_H_
#define NET_BASE_SCOPED_CERT_CHAIN_CONTEXT_H_
#pragma once

#include <windows.h>
#include <wincrypt.h>

#include "base/memory/scoped_ptr.h"

namespace net {

// This class wraps the CertFreeCertificateChain function in a class that can
// be passed as a template argument to scoped_ptr_malloc.
class ScopedPtrMallocFreeCertChain {
 public:
  void operator()(const CERT_CHAIN_CONTEXT* x) const {
    CertFreeCertificateChain(x);
  }
};

typedef scoped_ptr_malloc<const CERT_CHAIN_CONTEXT,
                          ScopedPtrMallocFreeCertChain> ScopedCertChainContext;

}  // namespace net

#endif  // NET_BASE_SCOPED_CERT_CHAIN_CONTEXT_H_
