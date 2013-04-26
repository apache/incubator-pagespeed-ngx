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
#include "strings/stringpiece_utils.h"

#include <vector>
#include "strings/ascii_ctype.h"

namespace strings {

int RemoveLeadingWhitespace(StringPiece* text) {
  int count = 0;
  const char* ptr = text->data();
  while (count < text->size() && ascii_isspace(*ptr)) {
    count++;
    ptr++;
  }
  text->remove_prefix(count);
  return count;
}

int RemoveTrailingWhitespace(StringPiece* text) {
  int count = 0;
  const char* ptr = text->data() + text->size() - 1;
  while (count < text->size() && ascii_isspace(*ptr)) {
    ++count;
    --ptr;
  }
  text->remove_suffix(count);
  return count;
}

int RemoveWhitespaceContext(StringPiece* text) {
  // use RemoveLeadingWhitespace() and RemoveTrailingWhitespace() to do the job
  return (RemoveLeadingWhitespace(text) + RemoveTrailingWhitespace(text));
}

}  // namespace strings
