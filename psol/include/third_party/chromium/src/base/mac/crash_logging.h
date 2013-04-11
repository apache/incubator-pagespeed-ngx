// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_CRASH_LOGGING_H_
#define BASE_MAC_CRASH_LOGGING_H_

#include "base/base_export.h"

#if __OBJC__
#import "base/memory/scoped_nsobject.h"

@class NSString;
#else
class NSString;
#endif

namespace base {
namespace mac {

typedef void (*SetCrashKeyValueFuncPtr)(NSString*, NSString*);
typedef void (*ClearCrashKeyValueFuncPtr)(NSString*);

// Set the low level functions used to supply crash keys to Breakpad.
BASE_EXPORT void SetCrashKeyFunctions(SetCrashKeyValueFuncPtr set_key_func,
                          ClearCrashKeyValueFuncPtr clear_key_func);

// Set and clear meta information for Minidump.
// IMPORTANT: On OS X, the key/value pairs are sent to the crash server
// out of bounds and not recorded on disk in the minidump, this means
// that if you look at the minidump file locally you won't see them!
BASE_EXPORT void SetCrashKeyValue(NSString* key, NSString* val);
BASE_EXPORT void ClearCrashKey(NSString* key);

// Format |count| items from |addresses| using %p, and set the
// resulting string as value for crash key |key|.  A maximum of 23
// items will be encoded, since breakpad limits values to 255 bytes.
BASE_EXPORT void SetCrashKeyFromAddresses(NSString* key,
                                          const void* const* addresses,
                                          size_t count);

#if __OBJC__

class BASE_EXPORT ScopedCrashKey {
 public:
  ScopedCrashKey(NSString* key, NSString* value);
  ~ScopedCrashKey();
 private:
  scoped_nsobject<NSString> crash_key_;
  DISALLOW_COPY_AND_ASSIGN(ScopedCrashKey);
};

#endif  // __OBJC__

}  // namespace mac
}  // namespace base

#endif  // BASE_MAC_CRASH_LOGGING_H_
