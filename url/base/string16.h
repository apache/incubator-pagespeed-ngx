/*
 * Copyright 2015 Google Inc.
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
// Author: morlovich@google.com (Maks Orlovich)
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
