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

// Author: matterbury@google.com (Matt Atterbury)

// A set of utility functions for handling hostnames.

#ifndef PAGESPEED_KERNEL_BASE_HOSTNAME_UTIL_H_
#define PAGESPEED_KERNEL_BASE_HOSTNAME_UTIL_H_

#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

// Determine the local hostname.
GoogleString GetHostname();

// Determine if the given host is localhost or a variant thereof:
// localhost, 127.0.0.1 (IPv4), ::1 (IPv6), or hostname (the local hostname).
// TODO(sligocki): Cover other representations of IPv6 localhost IP?
// TODO(matterbury): Handle all 127.0.0.0/8 address since they are 'localhost'.
bool IsLocalhost(StringPiece host_to_test, StringPiece hostname);

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_HOSTNAME_UTIL_H_
