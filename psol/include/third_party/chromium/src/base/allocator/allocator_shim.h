// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

// Expose some of tcmalloc functions for test.
void* TCMallocDoMallocForTest(size_t size);
void TCMallocDoFreeForTest(void* ptr);
size_t ExcludeSpaceForMarkForTest(size_t size);

}  // namespace allocator.
}  // namespace base.

#endif   // BASE_ALLOCATOR_ALLOCATOR_SHIM_H_
