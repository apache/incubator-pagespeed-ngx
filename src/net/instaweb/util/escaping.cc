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

namespace net_instaweb {

// We escape backslash, double-quote, CR and LF while forming a string
// from the code. This is /almost/ completely right: U+2028 and U+2029 are
// line terminators as well (ECMA 262-5 --- 7.3, 7.8.4), so should really be
// escaped, too, but we don't have the encoding here.
void EscapeToJsStringLiteral(const StringPiece& original,
                             bool add_quotes,
                             GoogleString* escaped) {
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
      case '/':
        // Forward slashes are generally OK, but </script> is trouble
        // if it happens inside an inline <script>. We therefore escape the
        // forward slash if we see /script>
        if (original.substr(c).starts_with("/script")) {
          (*escaped) += '\\';
        }
        // fallthrough intentional.
      default:
        (*escaped) += original[c];
    }
  }
  if (add_quotes) {
    (*escaped) += "\"";
  }
}

}  // namespace net_instaweb
