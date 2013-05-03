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

#ifndef PAGESPEED_KERNEL_BASE_TIME_UTIL_H_
#define PAGESPEED_KERNEL_BASE_TIME_UTIL_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

// Converts time, in milliseconds, to a string.  Returns false on failure.
bool ConvertTimeToString(int64 time_ms, GoogleString* time_string);
// Converts time, in microseconds, to a string with accuracy at us.
// Returns false on failure.
bool ConvertTimeToStringWithUs(int64 time_us, GoogleString* time_string);
// Converts time in string format, to the number of milliseconds since 1970.
// Returns false on failure.
bool ConvertStringToTime(const StringPiece& time_string, int64* time_ms);

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_TIME_UTIL_H_
