// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CHROMEOS_CHROMEOS_VERSION_H_
#define BASE_CHROMEOS_CHROMEOS_VERSION_H_

#include "base/base_export.h"

namespace base {
namespace chromeos {

// Returns true if the browser is running on Chrome OS.
// Useful for implementing stubs for Linux desktop.
BASE_EXPORT bool IsRunningOnChromeOS();

}  // namespace chromeos
}  // namespace base

#endif  // BASE_CHROMEOS_CHROMEOS_VERSION_H_
