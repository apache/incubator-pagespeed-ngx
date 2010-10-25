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
#ifndef STRINGS_ESCAPING_H_
#define STRINGS_ESCAPING_H_

#include "base/logging.h"

inline int hex_digit_to_int(char c) {
  /* Assume ASCII. */
  DCHECK('0' == 0x30 && 'A' == 0x41 && 'a' == 0x61);
  DCHECK(isxdigit(c));
  int x = static_cast<unsigned char>(c);
  if (x > '9') {
    x += 9;
  }
  return x & 0xf;
}

#endif  // STRINGS_ESCAPING_H_
