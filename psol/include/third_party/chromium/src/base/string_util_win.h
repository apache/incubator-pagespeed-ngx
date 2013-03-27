// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRING_UTIL_WIN_H_
#define BASE_STRING_UTIL_WIN_H_

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "base/logging.h"

namespace base {

// Chromium code style is to not use malloc'd strings; this is only for use
// for interaction with APIs that require it.
inline char* strdup(const char* str) {
  return _strdup(str);
}

inline int strcasecmp(const char* s1, const char* s2) {
  return _stricmp(s1, s2);
}

inline int strncasecmp(const char* s1, const char* s2, size_t count) {
  return _strnicmp(s1, s2, count);
}

inline int strncmp16(const char16* s1, const char16* s2, size_t count) {
  return ::wcsncmp(s1, s2, count);
}

inline int vsnprintf(char* buffer, size_t size,
                     const char* format, va_list arguments) {
  int length = _vsprintf_p(buffer, size, format, arguments);
  if (length < 0) {
    if (size > 0)
      buffer[0] = 0;
    return _vscprintf_p(format, arguments);
  }
  return length;
}

inline int vswprintf(wchar_t* buffer, size_t size,
                     const wchar_t* format, va_list arguments) {
  DCHECK(IsWprintfFormatPortable(format));

  int length = _vswprintf_p(buffer, size, format, arguments);
  if (length < 0) {
    if (size > 0)
      buffer[0] = 0;
    return _vscwprintf_p(format, arguments);
  }
  return length;
}

}  // namespace base

#endif  // BASE_STRING_UTIL_WIN_H_
