// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_REG_UTIL_H_
#define BASE_TEST_TEST_REG_UTIL_H_

// Registry utility functions used only by tests.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/win/registry.h"

namespace registry_util {

// Allows a test to easily override registry hives so that it can start from a
// known good state, or make sure to not leave any side effects once the test
// completes.
class RegistryOverrideManager {
 public:
  // All overridden hives will be descendents of this registry path under the
  // main HKCU hive.
  static const wchar_t kTempTestKeyPath[];

  RegistryOverrideManager();
  ~RegistryOverrideManager();

  // Override the given registry hive using a temporary key named by temp_name
  // under the temporary test key path.
  void OverrideRegistry(HKEY override, const std::wstring& temp_name);

  // Deletes all temporary test keys used by the overrides.
  static void DeleteAllTempKeys();

  // Removes all overrides and deletes all temporary test keys used by the
  // overrides.
  void RemoveAllOverrides();

 private:
  // Keeps track of one override.
  class ScopedRegistryKeyOverride {
   public:
    ScopedRegistryKeyOverride(HKEY override, const std::wstring& temp_name);
    ~ScopedRegistryKeyOverride();

   private:
    HKEY override_;
    base::win::RegKey temp_key_;
    std::wstring temp_name_;

    DISALLOW_COPY_AND_ASSIGN(ScopedRegistryKeyOverride);
  };

  std::vector<ScopedRegistryKeyOverride*> overrides_;

  DISALLOW_COPY_AND_ASSIGN(RegistryOverrideManager);
};

}  // namespace registry_util

#endif  // BASE_TEST_TEST_REG_UTIL_H_
