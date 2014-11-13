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

#if defined(PAGESPEED_SCANLINE_STATUS) ||               \
  defined(PAGESPEED_SCANLINE_STATUS_SOURCE) ||          \
  defined(PAGESPEED_SCANLINE_STATUS_ENUM_NAME) ||       \
  defined(PAGESPEED_SCANLINE_STATUS_ENUM_STRING)
#error "Preprocessor macro collision."
#endif

#define PAGESPEED_SCANLINE_STATUS(_X)           \
    _X(SCANLINE_STATUS_UNINITIALIZED),          \
    _X(SCANLINE_STATUS_SUCCESS),                \
    _X(SCANLINE_STATUS_UNSUPPORTED_FORMAT),     \
    _X(SCANLINE_STATUS_UNSUPPORTED_FEATURE),    \
    _X(SCANLINE_STATUS_PARSE_ERROR),            \
    _X(SCANLINE_STATUS_MEMORY_ERROR),           \
    _X(SCANLINE_STATUS_INTERNAL_ERROR),         \
    _X(SCANLINE_STATUS_TIMEOUT_ERROR),          \
    _X(SCANLINE_STATUS_INVOCATION_ERROR),       \
                                                \
    _X(NUM_SCANLINE_STATUS)

// Note the source of the error message by means of an enum rather
// than a string.
#define PAGESPEED_SCANLINE_STATUS_SOURCE(_X)    \
    _X(SCANLINE_UNKNOWN),                       \
    _X(SCANLINE_PNGREADER),                     \
    _X(SCANLINE_PNGREADERRAW),                  \
    _X(SCANLINE_GIFREADER),                     \
    _X(SCANLINE_GIFREADERRAW),                  \
    _X(SCANLINE_JPEGREADER),                    \
    _X(SCANLINE_WEBPREADER),                    \
    _X(SCANLINE_RESIZER),                       \
    _X(SCANLINE_PNGWRITER),                     \
    _X(SCANLINE_JPEGWRITER),                    \
    _X(SCANLINE_WEBPWRITER),                    \
    _X(SCANLINE_UTIL),                          \
    _X(SCANLINE_PIXEL_FORMAT_OPTIMIZER),        \
    _X(FRAME_TO_SCANLINE_READER_ADAPTER),       \
    _X(FRAME_TO_SCANLINE_WRITER_ADAPTER),       \
    _X(SCANLINE_TO_FRAME_READER_ADAPTER),       \
    _X(SCANLINE_TO_FRAME_WRITER_ADAPTER),       \
    _X(FRAME_GIFREADER),                        \
    _X(FRAME_WEBPWRITER),                       \
    _X(FRAME_PADDING_READER),                   \
                                                \
    _X(NUM_SCANLINE_SOURCE)

#define PAGESPEED_SCANLINE_STATUS_ENUM_NAME(_Y) _Y
#define PAGESPEED_SCANLINE_STATUS_ENUM_STRING(_Y) #_Y

enum ScanlineStatusType {
  PAGESPEED_SCANLINE_STATUS(PAGESPEED_SCANLINE_STATUS_ENUM_NAME)
};

enum ScanlineStatusSource {
  PAGESPEED_SCANLINE_STATUS_SOURCE(PAGESPEED_SCANLINE_STATUS_ENUM_NAME)
};

// A class to report the success or error of ScanlineInterface
// operations. Scanline*Interface should return the
// ScanlineStatus corresponding to the earliest error
// encountered. ScanlineStatus.details_ should be of the form
// "FunctionThatFailed()" or "failure message".
class ScanlineStatus {
 public:
  ScanlineStatus() : type_(SCANLINE_STATUS_SUCCESS),
                     source_(SCANLINE_UNKNOWN),
                     details_() {}

  ScanlineStatus(ScanlineStatusType type,
                 ScanlineStatusSource source,
                 const GoogleString& details)
      : type_(type),
        source_(source),
        details_(details) {}

  explicit ScanlineStatus(ScanlineStatusType type)
      : type_(type),
        source_(SCANLINE_UNKNOWN),
        details_() {}

  // This function takes variadic arguments so that we can use the
  // same sets of arguments here and for logging via the
  // PS_LOGGED_STATUS macro below.
  static ScanlineStatus New(ScanlineStatusType type,
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
    return ScanlineStatus(type, source, detail_list);
  };

  bool Success() const { return (type_ == SCANLINE_STATUS_SUCCESS); }
  ScanlineStatusType type() const { return type_; }
  ScanlineStatusSource source() const { return source_; }
  const GoogleString& details() const { return details_; }

  const char* TypeStr() const {
    static const char* const kScanlineStatusTypeNames[] = {
      PAGESPEED_SCANLINE_STATUS(PAGESPEED_SCANLINE_STATUS_ENUM_STRING)
    };
    return kScanlineStatusTypeNames[type_];
  }

  const char* SourceStr() const {
    static const char* const kScanlineStatusSourceNames[] = {
      PAGESPEED_SCANLINE_STATUS_SOURCE(PAGESPEED_SCANLINE_STATUS_ENUM_STRING)
    };
    return kScanlineStatusSourceNames[source_];
  }

  const GoogleString ToString() const {
    return GoogleString(SourceStr()) + "/" + TypeStr() + " " + details();
  }

  // Determines whether the source of this status is a reader of some
  // sort.
  bool ComesFromReader() const {
    switch (source_) {
      case SCANLINE_PNGREADER:
      case SCANLINE_PNGREADERRAW:
      case SCANLINE_GIFREADER:
      case SCANLINE_GIFREADERRAW:
      case SCANLINE_JPEGREADER:
      case SCANLINE_WEBPREADER:
      case FRAME_TO_SCANLINE_READER_ADAPTER:
      case SCANLINE_TO_FRAME_READER_ADAPTER:
      case FRAME_GIFREADER:
      case FRAME_PADDING_READER:
        return true;
      default:
        return false;
    }
    return false;
  }

 private:
  ScanlineStatusType type_;
  ScanlineStatusSource source_;
  GoogleString details_;

  // Note: we are allowing the implicit copy constructor and
  // assignment operators.
};

#undef PAGESPEED_SCANLINE_STATUS_ENUM_STRING
#undef PAGESPEED_SCANLINE_STATUS_ENUM_NAME
#undef PAGESPEED_SCANLINE_STATUS_SOURCE
#undef PAGESPEED_SCANLINE_STATUS

// Convenience macro for simultaneously logging error descriptions and
// creating a ScanlineStatus with that error description. _LOGGER is
// meant to be one of the PS_LOG* macros defined in message_handler.h.
#define PS_LOGGED_STATUS(_LOGGER, _HANDLER, _TYPE, _SOURCE, ...)        \
  (_LOGGER(_HANDLER, #_SOURCE "/" #_TYPE " " __VA_ARGS__),              \
   ScanlineStatus::New(_TYPE, _SOURCE, __VA_ARGS__))

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_SCANLINE_STATUS_H_
