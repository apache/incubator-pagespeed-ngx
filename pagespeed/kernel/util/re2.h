/*
 * Copyright 2012 Google Inc.
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

// Author: gagansingh@google.com (Gagan Singh)

#ifndef PAGESPEED_KERNEL_UTIL_RE2_H_
#define PAGESPEED_KERNEL_UTIL_RE2_H_

#include "pagespeed/kernel/base/string_util.h"

#include "third_party/re2/src/re2/re2.h"

using re2::RE2;

namespace re2 {
const RE2::CannedOptions posix_syntax = RE2::POSIX;
}  // namespace re2

typedef re2::StringPiece Re2StringPiece;
// Converts a Google StringPiece into an RE2 StringPiece.  These are of course
// the same basic thing but are declared in distinct namespaces and as far as
// C++ type-checking is concerned they are incompatible.
//
// TODO(jmarantz): In the re2 code itself there are no references to
// re2::StringPiece, always just plain StringPiece, so if we can
// arrange to get the right definition #included we should be all set.
// We could somehow rewrite '#include "re2/stringpiece.h"' to
// #include Chromium's stringpiece then everything would just work.
inline re2::StringPiece StringPieceToRe2(StringPiece sp) {
  return re2::StringPiece(sp.data(), sp.size());
}

inline StringPiece Re2ToStringPiece(re2::StringPiece sp) {
  return StringPiece(sp.data(), sp.size());
}


#endif  // PAGESPEED_KERNEL_UTIL_RE2_H_
