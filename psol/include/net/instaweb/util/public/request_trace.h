/*
 * Copyright 2012 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: piatek@google.com (Michael Piatek)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_REQUEST_TRACE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_REQUEST_TRACE_H_

#include <cstdarg>

#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

// The context for recording a distributed trace associated with a given
// request.
class RequestTrace {
 public:
  RequestTrace();
  virtual ~RequestTrace();

  // Logs formatted output to the distributed tracing context.
  virtual void TraceVPrintf(const char* fm, va_list argp) = 0;
  void TracePrintf(const char* fmt, ...);

  // Returns true iff tracing is enabled. This can be used to avoid virtual
  // function call overhead in the common case that tracing is not active for
  // a given request.
  bool tracing_enabled() { return tracing_enabled_; }
  void set_tracing_enabled(bool x) { tracing_enabled_ = x; }

 private:
  bool tracing_enabled_;

  DISALLOW_COPY_AND_ASSIGN(RequestTrace);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_REQUEST_TRACE_H_
