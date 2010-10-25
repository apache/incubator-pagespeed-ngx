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
#ifndef STRINGS_STRUTIL_H_
#define STRINGS_STRUTIL_H_

//#include "third_party/chromium/src/base/string_util.h"
#include "base/string_util.h"

// Like the CONSIDER macro above except that it supports enums from another
// class -- e.g., if:   enum Status { VERIFIED, NOT_VERIFIED, WHITE_LISTED }
// is in class Foo and you are using it in another class, use:
//   switch (val) {
//     CONSIDER_IN_CLASS(Foo, VERIFIED);
//     CONSIDER_IN_CLASS(Foo, NOT_VERIFIED);
//     CONSIDER_IN_CLASS(Foo, WHITE_LISTED);
//     default: return "UNKNOWN value";
//   }
// Only the enum string will be returned (i.e., without the "Foo::" prefix).
#define CONSIDER_IN_CLASS(cls,val)       case cls::val: return #val

#endif  // WEBUTIL_CSS_OPEN_SOURCE_STRINGS_STRUTIL_H_
