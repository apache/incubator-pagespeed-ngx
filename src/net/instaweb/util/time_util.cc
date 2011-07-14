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

#include "net/instaweb/util/public/time_util.h"
#include <ctime>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/core/resource_util.h"

namespace net_instaweb {

namespace {
// Converts time (in ms or us) to a string.
// us is true if time is in microseconds;
// otherwise time is in milliseconds.
bool TimeToString(int64 time, GoogleString* time_string,
                  bool include_microseconds) {
  time_t time_sec;
  if (include_microseconds) {
    time_sec = time / 1000000;
  } else {
    time_sec = time / 1000;
  }
  struct tm time_buf;
  struct tm* time_info = gmtime_r(&time_sec, &time_buf);
  if ((time_info == NULL) ||
      (time_buf.tm_wday < 0) ||
      (time_buf.tm_wday > 6) ||
      (time_buf.tm_mon < 0) ||
      (time_buf.tm_mon > 11)) {
    return false;
  }

  static const char* kWeekDay[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char* kMonth[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov",
    "Dec"};

  // RFC 822 says to format like this:
  //                Thu Nov 18 02:15:22 2010 GMT
  // See http://www.faqs.org/rfcs/rfc822.html
  //
  // But redbot.org likes this:
  //    Wed, 24 Nov 2010 21:14:12 GMT

  // If us is true, time is down to microseconds, the format is like this:
  //    Wed, 24 Nov 2010 21:14:12.12345 GMT
  *time_string = StringPrintf("%s, %02d %s %4d %02d:%02d:%02d",
                              kWeekDay[time_buf.tm_wday],
                              time_buf.tm_mday,
                              kMonth[time_buf.tm_mon],
                              1900 + time_buf.tm_year,
                              time_buf.tm_hour,
                              time_buf.tm_min,
                              time_buf.tm_sec);
  if (include_microseconds) {
    // Append microseconds.
    int remainder = time % 1000000;
    StrAppend(time_string, ".", IntegerToString(remainder), " GMT");
  } else {
    // Note if we do StrAppend(time_string, " GMT") results in error "too few
    // arguments to function StrAppend".
    StrAppend(time_string, " ", "GMT");
  }

  return true;
}

}  // namespace.

bool ConvertTimeToString(int64 time_ms, GoogleString* time_string) {
  return (TimeToString(time_ms, time_string, false));
}

// This function is similar to ConvertTimeToString, except it takes time_us
// and returns a string with microsecond accuracy.
bool ConvertTimeToStringWithUs(int64 time_us, GoogleString* time_string) {
  return(TimeToString(time_us, time_string, true));
}

bool ConvertStringToTime(const StringPiece& time_string, int64 *time_ms) {
  GoogleString buf(time_string.data(), time_string.size());
  return pagespeed::resource_util::ParseTimeValuedHeader(buf.c_str(), time_ms);
}

}  // namespace net_instaweb
