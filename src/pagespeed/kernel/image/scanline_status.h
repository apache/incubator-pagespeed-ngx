/*
 * Copyright 2013 Google Inc.
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

// Author: Victor Chudnovsky

#ifndef PAGESPEED_KERNEL_IMAGE_SCANLINE_STATUS_H_
#define PAGESPEED_KERNEL_IMAGE_SCANLINE_STATUS_H_

#include <cstdarg>
#include "pagespeed/kernel/base/printf_format.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace pagespeed {

namespace image_compression {

#if defined(PAGESPEED_SCANLINE_STATUS) ||                \
  defined(PAGESPEED_SCANLINE_STATUS_SOURCE) ||           \
  defined(PAGESPEED_SCANLINE_STATUS_ENUM_NAME) ||        \
  defined(PAGESPEED_SCANLINE_STATUS_ENUM_STRING)
#error "Preprocessor macro collision."
#endif

#define PAGESPEED_SCANLINE_STATUS(_X)                  \
    _X(SCANLINE_STATUS_UNINITIALIZED),                 \
    _X(SCANLINE_STATUS_SUCCESS),                       \
    _X(SCANLINE_STATUS_UNSUPPORTED_FORMAT),            \
    _X(SCANLINE_STATUS_UNSUPPORTED_FEATURE),           \
    _X(SCANLINE_STATUS_PARSE_ERROR),                   \
    _X(SCANLINE_STATUS_MEMORY_ERROR),                  \
    _X(SCANLINE_STATUS_INTERNAL_ERROR),                \
    _X(SCANLINE_STATUS_TIMEOUT_ERROR),                 \
    _X(SCANLINE_STATUS_INVOCATION_ERROR)

// Note the source of the error message by means of an enum rather
// than a string.
#define PAGESPEED_SCANLINE_STATUS_SOURCE(_X) \
  _X(SCANLINE_UNKNOWN),                      \
    _X(SCANLINE_PNGREADER),                  \
    _X(SCANLINE_PNGREADERRAW),               \
    _X(SCANLINE_GIFREADER),                  \
    _X(SCANLINE_GIFREADERRAW),               \
    _X(SCANLINE_JPEGREADER),                 \
    _X(SCANLINE_WEBPREADER),                 \
    _X(SCANLINE_RESIZER),                    \
    _X(SCANLINE_PNGWRITER),                  \
    _X(SCANLINE_JPEGWRITER),                 \
    _X(SCANLINE_WEBPWRITER),                 \
    _X(SCANLINE_UTIL)

#define PAGESPEED_SCANLINE_STATUS_ENUM_NAME(_Y) _Y
#define PAGESPEED_SCANLINE_STATUS_ENUM_STRING(_Y) #_Y

enum ScanlineStatusType {
  PAGESPEED_SCANLINE_STATUS(PAGESPEED_SCANLINE_STATUS_ENUM_NAME)
};

enum ScanlineStatusSource {
  PAGESPEED_SCANLINE_STATUS_SOURCE(PAGESPEED_SCANLINE_STATUS_ENUM_NAME)
};

// A struct to report the success or error of ScanlineInterface
// operations. Scanline*Interface should return the
// ScanlineStatus corresponding to the earliest error
// encountered. ScanlineStatus.details should be of the form
// "FunctionThatFailed()" or "failure message".
struct ScanlineStatus {
  ScanlineStatus() : status_type(SCANLINE_STATUS_UNINITIALIZED),
                     source(SCANLINE_UNKNOWN),
                     details() {}

  explicit ScanlineStatus(ScanlineStatusType the_status_type)
      : status_type(the_status_type),
        source(SCANLINE_UNKNOWN),
        details() {}

  ScanlineStatus(ScanlineStatusType the_status_type,
                 ScanlineStatusSource the_source,
                 const GoogleString& the_details)
      : status_type(the_status_type),
        source(the_source),
        details(the_details) {}

  // This function takes variadic arguments so that we can use the
  // same sets of arguments here and for logging via the
  // PS_LOGGED_STATUS macro below.
  static inline ScanlineStatus New(ScanlineStatusType status_type,
                                   ScanlineStatusSource source,
                                   const char* details,
                                   ...) INSTAWEB_PRINTF_FORMAT(3, 4) {
    va_list args;
    GoogleString detail_list;
    // Ignore the name of this routine: it formats with vsnprintf.
    // See base/stringprintf.cc.
    va_start(args, details);
    StringAppendV(&detail_list, details, args);
    va_end(args);
    return ScanlineStatus(status_type, source, detail_list);
  };

  inline const char* StatusTypeStr() const {
    static const char* kScanlineStatusTypeNames[] = {
      PAGESPEED_SCANLINE_STATUS(PAGESPEED_SCANLINE_STATUS_ENUM_STRING)
    };
    return kScanlineStatusTypeNames[status_type];
  }

  inline const char* SourceTypeStr() const {
    static const char* kScanlineStatusSourceNames[] = {
      PAGESPEED_SCANLINE_STATUS_SOURCE(PAGESPEED_SCANLINE_STATUS_ENUM_STRING)
    };
    return kScanlineStatusSourceNames[source];
  }

  inline bool Success() const {
    return (status_type == SCANLINE_STATUS_SUCCESS);
  }

  ScanlineStatusType status_type;
  ScanlineStatusSource source;
  GoogleString details;
};

#undef PAGESPEED_SCANLINE_STATUS_ENUM_STRING
#undef PAGESPEED_SCANLINE_STATUS_ENUM_NAME
#undef PAGESPEED_SCANLINE_STATUS_SOURCE
#undef PAGESPEED_SCANLINE_STATUS

// Convenience macro for simultaneously logging error descriptions and
// creating a ScanlineStatus with that error description. LOGGER_ is
// meant to be one of the PS_LOG* macros defined in message_handler.h.
#define PS_LOGGED_STATUS(_LOGGER, _HANDLER, _TYPE, _SOURCE, ...)     \
  (_LOGGER(_HANDLER, #_TYPE ":" #_SOURCE ": " __VA_ARGS__),          \
   ScanlineStatus::New(_TYPE, _SOURCE, __VA_ARGS__))

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_SCANLINE_STATUS_H_
