/**
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

#include "net/instaweb/util/public/time_util.h"
#include <time.h>

#include "pagespeed/core/resource_util.h"

namespace net_instaweb {

void ConvertTimeToString(int64 time_ms, std::string* time_string) {
  time_t time_sec = time_ms / 1000;
  struct tm time_buf;
  struct tm* time_info = gmtime_r(&time_sec, &time_buf);

  char buf[100];  // man ctime says buffer should be at least 26.
  TrimWhitespace(asctime_r(time_info, buf), time_string);
  *time_string += " GMT";
}

bool ConvertStringToTime(const StringPiece& time_string, int64 *time_ms) {
  std::string buf(time_string.data(), time_string.size());
  return pagespeed::resource_util::ParseTimeValuedHeader(buf.c_str(), time_ms);
}

}  // namespace net_instaweb
