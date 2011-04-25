/*
 * Copyright 2010 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)
//

#ifndef NET_INSTAWEB_UTIL_PUBLIC_GFLAGS_H_
#define NET_INSTAWEB_UTIL_PUBLIC_GFLAGS_H_

#include "net/instaweb/util/public/basictypes.h"

// Holy cow this is ugly.  We get our integer types from base/basictypes.h,
// but gflags.h, which we include only to gain access to its memory cleanup
// function, defines its own versions of those integer typedefs.  The simplest
// thing to do is to #define them aside.  Is this brittle?  You betcha.  Will
// it be obvious how to fix it when it breaks in the future?  I hope so!
#define int64 gflags_int64
#define int32 gflags_int32
#define uint64 gflags_uint64
#define uint32 gflags_uint32
#include "gflags/gflags.h"
#undef int64
#undef int32
#undef uint64
#undef uint32


#endif  // NET_INSTAWEB_UTIL_PUBLIC_GFLAGS_H_
