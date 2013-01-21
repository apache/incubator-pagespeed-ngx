// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CRYPTO_MODULE_H_
#define NET_BASE_CRYPTO_MODULE_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"

#if defined(USE_NSS)
typedef struct PK11SlotInfoStr PK11SlotInfo;
#endif

namespace net {

class CryptoModule;

typedef std::vector<scoped_refptr<CryptoModule> > CryptoModuleList;

class CryptoModule : public base::RefCountedThreadSafe<CryptoModule> {
 public:
#if defined(USE_NSS)
  typedef PK11SlotInfo* OSModuleHandle;
#else
  typedef void* OSModuleHandle;
#endif

  OSModuleHandle os_module_handle() const { return module_handle_; }

  std::string GetTokenName() const;

  static CryptoModule* CreateFromHandle(OSModuleHandle handle);

 private:
  friend class base::RefCountedThreadSafe<CryptoModule>;

  explicit CryptoModule(OSModuleHandle handle);
  ~CryptoModule();

  OSModuleHandle module_handle_;

  DISALLOW_COPY_AND_ASSIGN(CryptoModule);
};

}  // namespace net

#endif  // NET_BASE_CRYPTO_MODULE_H_
