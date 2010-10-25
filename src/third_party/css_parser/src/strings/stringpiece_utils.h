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

// Copyright 2003 and onwards Google Inc.
//
// Utility functions for operating on StringPieces
// Collected here for convenience

#ifndef STRINGS_STRINGPIECE_UTILS_H_
#define STRINGS_STRINGPIECE_UTILS_H_

#include <vector>
#include "strings/stringpiece.h"

namespace StringPieceUtils {

// removes leading and trailing ascii_isspace() chars. Returns
// number of chars removed
int RemoveWhitespaceContext(StringPiece* text);

// similar to SplitStringUsing (see strings/split.h).
// this one returns a vector of pieces in the original string thus eliminating
// the allocation/copy for each string in the result vector.
void Split(const StringPiece& full, const char* delim,
           std::vector<StringPiece>* pieces);

}  // namespace StringPieceUtils

#endif  // STRINGS_STRINGPIECE_UTILS_H_
