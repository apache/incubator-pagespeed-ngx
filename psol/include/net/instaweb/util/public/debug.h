/*
 * Copyright 2011 Google Inc.
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

// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_DEBUG_H_
#define NET_INSTAWEB_UTIL_PUBLIC_DEBUG_H_

#include "net/instaweb/util/public/string.h"
// Use Chromium debug support.
#include "base/debug/stack_trace.h"

namespace net_instaweb {

GoogleString StackTraceString();

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_DEBUG_H_
