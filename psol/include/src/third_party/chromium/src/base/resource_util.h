// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for accessing resources in external
// files (DLLs) or embedded in the executable itself.

#ifndef BASE_RESOURCE_UTIL_H__
#define BASE_RESOURCE_UTIL_H__
#pragma once

#include <windows.h>

#include "base/base_api.h"
#include "base/basictypes.h"

namespace base {

// Function for getting a data resource (BINDATA) from a dll.  Some
// resources are optional, especially in unit tests, so this returns false
// but doesn't raise an error if the resource can't be loaded.
bool BASE_API GetDataResourceFromModule(HMODULE module, int resource_id,
                                        void** data, size_t* length);
}  // namespace base

#endif  // BASE_RESOURCE_UTIL_H__
