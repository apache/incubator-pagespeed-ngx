// Copyright 2009 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PAGESPEED_FORMATTERS_FORMATTER_UTIL_H_
#define PAGESPEED_FORMATTERS_FORMATTER_UTIL_H_

#include <string>

#include "base/basictypes.h"

namespace pagespeed {

namespace formatters {

// Format a bytes quantity as a human-readable string by converting
// the value to Kilo-Bytes or Mega-Bytes when appropriate and
// appending the appropriate units identifier.
std::string FormatBytes(int64 bytes);

// Format a time duration quantity as a human-readable string,
// e.g. "10 minutes" or "1 year" or "5 days 12 hours".
std::string FormatTimeDuration(int64 milliseconds);

}  // namespace formatters

}  // namespace pagespeed

#endif  // PAGESPEED_FORMATTERS_FORMATTER_UTIL_H_
