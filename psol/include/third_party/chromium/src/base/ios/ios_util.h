// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_IOS_IOS_UTIL_H_
#define BASE_IOS_IOS_UTIL_H_

#include "base/base_export.h"
#include "base/basictypes.h"

namespace base {
namespace ios {

// Returns whether the operation system is iOS 5 or later.
BASE_EXPORT bool IsRunningOnIOS5OrLater();

// Returns whether the operation system is iOS 6 or later.
BASE_EXPORT bool IsRunningOnIOS6OrLater();

// Returns whether the operation system is at the given version or later.
BASE_EXPORT bool IsRunningOnOrLater(int32 major, int32 minor, int32 bug_fix);

}  // namespace ios
}  // namespace base

#endif  // BASE_IOS_IOS_UTIL_H_
