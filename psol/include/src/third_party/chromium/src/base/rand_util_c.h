// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RAND_UTIL_C_H_
#define BASE_RAND_UTIL_C_H_
#pragma once

#include "base/base_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Note this *should* be in "namespace base" but the function is needed
// from C so namespaces cannot be used.

// Returns an FD for /dev/urandom, possibly pre-opened before sandboxing
// was switched on.  This is a C function so that Native Client can use it.
BASE_API int GetUrandomFD(void);

#ifdef __cplusplus
}
#endif

#endif /* BASE_RAND_UTIL_C_H_ */
