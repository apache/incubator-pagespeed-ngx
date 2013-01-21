// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BASE_PATHS_H_
#define BASE_BASE_PATHS_H_
#pragma once

// This file declares path keys for the base module.  These can be used with
// the PathService to access various special directories and files.

#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#elif defined(OS_MACOSX)
#include "base/base_paths_mac.h"
#endif

namespace base {

enum {
  PATH_START = 0,

  DIR_CURRENT,  // current directory
  DIR_EXE,      // directory containing FILE_EXE
  DIR_MODULE,   // directory containing FILE_MODULE
  DIR_TEMP,     // temporary directory
  FILE_EXE,     // Path and filename of the current executable.
  FILE_MODULE,  // Path and filename of the module containing the code for the
                // PathService (which could differ from FILE_EXE if the
                // PathService were compiled into a shared object, for example).
  DIR_SOURCE_ROOT,  // Returns the root of the source tree.  This key is useful
                    // for tests that need to locate various resources.  It
                    // should not be used outside of test code.
#if defined(OS_POSIX)
  DIR_CACHE,    // Directory where to put cache data.  Note this is
                // *not* where the browser cache lives, but the
                // browser cache can be a subdirectory.
                // This is $XDG_CACHE_HOME on Linux and
                // ~/Library/Caches on Mac.
#endif

  PATH_END
};

}  // namespace base

#endif  // BASE_BASE_PATHS_H_
