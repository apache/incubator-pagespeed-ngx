// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_ALLOCATOR_EXTENSION_H
#define BASE_ALLOCATOR_ALLOCATOR_EXTENSION_H

#include <stddef.h> // for size_t

#include "base/allocator/allocator_extension_thunks.h"
#include "base/base_export.h"
#include "build/build_config.h"

namespace base {
namespace allocator {

// Request the allocator to report value of its internal state variable.
//
// |name| name of the variable
// |value| pointer to the returned value, must be not NULL.
// Returns true if the value has been returned, false if a variable with such
// name does not exist.
BASE_EXPORT bool GetProperty(const char* name, size_t* value);

// Request that the allocator print a human-readable description of the current
// state of the allocator into a null-terminated string in the memory segment
// buffer[0,buffer_length-1].
//
// |buffer| must point to a valid piece of memory
// |buffer_length| must be > 0.
BASE_EXPORT void GetStats(char* buffer, int buffer_length);

// Request that the allocator release any free memory it knows about to the
// system.
BASE_EXPORT void ReleaseFreeMemory();


// These settings allow specifying a callback used to implement the allocator
// extension functions.  These are optional, but if set they must only be set
// once.  These will typically called in an allocator-specific initialization
// routine.
//
// No threading promises are made.  The caller is responsible for making sure
// these pointers are set before any other threads attempt to call the above
// functions.
BASE_EXPORT void SetGetPropertyFunction(
    thunks::GetPropertyFunction get_property_function);

BASE_EXPORT void SetGetStatsFunction(
    thunks::GetStatsFunction get_stats_function);

BASE_EXPORT void SetReleaseFreeMemoryFunction(
    thunks::ReleaseFreeMemoryFunction release_free_memory_function);

}  // namespace allocator
}  // namespace base

#endif
