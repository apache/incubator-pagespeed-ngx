// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// NaCL newlib is not compatible with the default
// stack_trace_posix.cc. So we provide this stubbed out version for
// use when building for NaCL.

#ifndef __native_client__
#error This file should only be used when compiling for Native Client.
#endif

#include "base/debug/stack_trace.h"

namespace base {
namespace debug {

StackTrace::StackTrace() {}
void StackTrace::PrintBacktrace() const {}
void StackTrace::OutputToStream(std::ostream* os) const {}

}  // namespace debug
}  // namespace base
