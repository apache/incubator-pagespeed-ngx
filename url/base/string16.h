#ifndef URL_COMPAT_STRING16_H
#define URL_COMPAT_STRING16_H
// We have both of:
//  third_party/chromium/src/base/strings/string16.h
//  third_party/chromium/src/googleurl/base/string16.h
// which is troubling wrt to ODR. For now, try to at least get the char type
// from former in place expected by the latter, and have the .gyp #define
// the guard for the later one so it doesn't actually get included.
#include "third_party/chromium/src/base/strings/string16.h"
typedef base::char16 char16;
typedef base::string16 string16;

#endif
