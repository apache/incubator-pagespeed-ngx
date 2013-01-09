// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_ALLOCATOR_SHIM_H_
#define BASE_ALLOCATOR_ALLOCATOR_SHIM_H_

namespace base {
namespace allocator {

// Resets the environment variable CHROME_ALLOCATOR to specify the choice to
// be used by subprocesses.  Priority is given to the current value of
// CHROME_ALLOCATOR_2 (if specified), then CHROME_ALLOCATOR (if specified), and
// then a default value (typically set to TCMALLOC).
void SetupSubprocessAllocator();

}  // namespace base.
}  // namespace allocator.

#endif   // BASE_ALLOCATOR_ALLOCATOR_SHIM_H_
