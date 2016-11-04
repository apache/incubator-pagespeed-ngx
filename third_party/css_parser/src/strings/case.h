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
// Implement strings/case.h in terms of memutil.h
#ifndef WEBUTIL_CSS_OPEN_SOURCE_STRINGS_CASE_H_
#define WEBUTIL_CSS_OPEN_SOURCE_STRINGS_CASE_H_

#include "memutil.h"  // NOLINT

inline bool CaseEqual(StringPiece s1, StringPiece s2) {
  if (s1.size() != s2.size()) return false;
  return memcasecmp(s1.data(), s2.data(), s1.size()) == 0;
}

#endif  // WEBUTIL_CSS_OPEN_SOURCE_STRINGS_CASE_H_
