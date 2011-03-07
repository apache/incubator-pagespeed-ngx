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
#ifndef MEMUTIL_H_
#define MEMUTIL_H_

#include "strings/ascii_ctype.h"

// The ""'s catch people who don't pass in a literal for "str"
#define strliterallen(str) (sizeof("" str "")-1)

inline int memcasecmp(const char *s1, const char *s2, size_t len) {
  const unsigned char *us1 = reinterpret_cast<const unsigned char *>(s1);
  const unsigned char *us2 = reinterpret_cast<const unsigned char *>(s2);

  for ( int i = 0; i < len; i++ ) {
    const int diff =
      static_cast<int>(static_cast<unsigned char>(ascii_tolower(us1[i]))) -
      static_cast<int>(static_cast<unsigned char>(ascii_tolower(us2[i])));
    if (diff != 0) return diff;
  }
  return 0;
}

#define memcaseis(str, len, literal)                            \
   ( (((len) == strliterallen(literal))                         \
      && memcasecmp(str, literal, strliterallen(literal)) == 0) )

#define memis(str, len, literal)                                \
   ( (((len) == strliterallen(literal))                         \
      && memcmp(str, literal, strliterallen(literal)) == 0) )


#endif  // MEMUTIL_H_
