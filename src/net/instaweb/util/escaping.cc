/*
 * Copyright 2011 Google Inc.
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

// Author: vitaliyl@google.com (Vitaliy Lvin)

#include "net/instaweb/util/public/escaping.h"

#include <cstddef>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// We escape backslash, double-quote, CR and LF while forming a string
// from the code. Single quotes are escaped as well, if we don't know we're
// explicitly double-quoting.  Appends to *escaped.
//
// This is /almost/ completely right: U+2028 and U+2029 are
// line terminators as well (ECMA 262-5 --- 7.3, 7.8.4), so should really be
// escaped, too, but we don't have the encoding here.
void EscapeToJsStringLiteral(const StringPiece& original,
                             bool add_quotes,
                             GoogleString* escaped) {
  // Optimistically assume no escaping will be required and reserve enough space
  // for that result.  This assumes that either escaped is empty (or nearly so),
  // or reserve(...) behaves sanely and only vector doubles rather than
  // increasing size linearly.  The latter is true in gcc at least (but not true
  // of some implementations of std::vector, thus the caveat).
  escaped->reserve(escaped->size() + original.size() + (add_quotes ? 2 : 0));
  if (add_quotes) {
    (*escaped) += "\"";
  }
  for (size_t c = 0; c < original.length(); ++c) {
    switch (original[c]) {
      case '\\':
        (*escaped) += "\\\\";
        break;
      case '"':
        (*escaped) += "\\\"";
        break;
      case '\r':
        (*escaped) += "\\r";
        break;
      case '\n':
        (*escaped) += "\\n";
        break;
      case '\'':
        if (!add_quotes) {
          (*escaped) += "\\'";
        } else {
          (*escaped) += '\'';
        }
        break;
      case '/':
        // Forward slashes are generally OK, but </script> is trouble
        // if it happens inside an inline <script>. We therefore escape the
        // forward slash if we see /script>
        if (StringCaseStartsWith(original.substr(c), "/script")) {
          (*escaped) += '\\';
        }
        FALLTHROUGH_INTENDED;
      default:
        (*escaped) += original[c];
    }
  }
  if (add_quotes) {
    (*escaped) += "\"";
  }
}

}  // namespace net_instaweb
