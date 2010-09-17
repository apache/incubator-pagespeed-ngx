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

#ifndef STRINGS_JOIN_H_
#define STRINGS_JOIN_H_

#include <string>
#include <vector>

// TODO(sligocki): Add this to Chromium string_util.h
inline void JoinStrings(const std::vector<std::string>& parts,
                        const char* delim, std::string* result) {
  if (parts.size() != 0) {
    result->append(parts[0]);
    std::vector<std::string>::const_iterator iter = parts.begin();
    ++iter;

    for (; iter != parts.end(); ++iter) {
      result->append(delim);
      result->append(*iter);
    }
  }
}

#endif  // STRINGS_JOIN_H_
