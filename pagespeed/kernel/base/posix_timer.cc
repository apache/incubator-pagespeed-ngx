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

#include "pagespeed/kernel/base/posix_timer.h"

#include <sys/time.h>
#include <unistd.h>
#include <cerrno>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

PosixTimer::~PosixTimer() {
}

int64 PosixTimer::NowUs() const {
  struct timeval tv;
  struct timezone tz = { 0, 0 };  // UTC
  if (gettimeofday(&tv, &tz) != 0) {
    LOG(FATAL) << "Could not determine time of day: " << strerror(errno);
  }
  return (static_cast<int64>(tv.tv_sec) * 1000000) + tv.tv_usec;
}

void PosixTimer::SleepUs(int64 us) {
  usleep(us);
}

}  // namespace net_instaweb
