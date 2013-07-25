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

#include "pagespeed/kernel/base/hostname_util.h"

#include <limits.h>
// The following break portability.

// Windows doesn't use <unistd.h> nor does it define HOST_NAME_MAX.
#if defined(WIN32)
#include <windows.h>
#include <winsock2.h>
#define HOST_NAME_MAX (MAX_COMPUTERNAME_LENGTH+1)
#else
// Some Linux environments require this, although not all, but it's benign.
#if defined(unix)
#include <unistd.h>
#endif  // unix
// MacOS does not defined HOST_NAME_MAX so fall back to the POSIX value.
// We are supposed to use sysconf(_SC_HOST_NAME_MAX) but we use this value
// to size an automatic array and we can't portably use variables for that.
#if !defined(HOST_NAME_MAX)
#define HOST_NAME_MAX _POSIX_HOST_NAME_MAX
#endif  // HOST_NAME_MAX
#endif  // WIN32

#include "base/logging.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

GoogleString GetHostname() {
  char hostname[HOST_NAME_MAX + 1];
  hostname[sizeof(hostname) - 1] = '\0';

  // This call really shouldn't fail, so crash if it does under Debug,
  // but ensure an empty (safe) value under Release.
  int err = gethostname(hostname, sizeof(hostname) - 1);
  if (err != 0) {
    DLOG(FATAL) << "gethostname failed: " << err;
    hostname[0] = '\0';
  }

  return GoogleString(hostname);
}

bool IsLocalhost(StringPiece host_to_test, StringPiece hostname) {
  return (host_to_test == "localhost" ||
          host_to_test == "127.0.0.1" ||
          host_to_test == "::1" ||
          host_to_test == hostname);
}

}  // namespace net_instaweb
