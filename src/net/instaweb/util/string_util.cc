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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/util/public/string_util.h"
#include <vector>

namespace net_instaweb {

void SplitStringPieceToVector(const StringPiece& sp, const char* separator,
                              std::vector<StringPiece>* components,
                              bool omit_empty_strings) {
  size_t prev_pos = 0;
  size_t pos = 0;
  StringPiece sep(separator);
  while ((pos = sp.find(sep, pos)) != StringPiece::npos) {
    if (!omit_empty_strings || (pos > prev_pos)) {
      components->push_back(sp.substr(prev_pos, pos - prev_pos));
    }
    ++pos;
    prev_pos = pos;
  }
  if (!omit_empty_strings || (prev_pos < sp.size())) {
    components->push_back(sp.substr(prev_pos, prev_pos - sp.size()));
  }
}

std::string StrCat(const StringPiece& a,
                    const StringPiece& b,
                    const StringPiece& c,
                    const StringPiece& d,
                    const StringPiece& e,
                    const StringPiece& f,
                    const StringPiece& g,
                    const StringPiece& h) {
  std::string res;
  res.reserve(a.size() + b.size() + c.size() + d.size() + e.size() + f.size() +
              g.size() + h.size());
  a.AppendToString(&res);
  b.AppendToString(&res);
  c.AppendToString(&res);
  d.AppendToString(&res);
  e.AppendToString(&res);
  f.AppendToString(&res);
  g.AppendToString(&res);
  h.AppendToString(&res);
  return res;
}

void BackslashEscape(const StringPiece& src,
                     const StringPiece& to_escape,
                     std::string* dest) {
  dest->reserve(dest->size() + src.size());
  for (const char *p = src.data(), *end = src.data() + src.size();
       p != end; ++p) {
    if (to_escape.find(*p) != StringPiece::npos) {
      dest->push_back('\\');
    }
    dest->push_back(*p);
  }
}

}  // namespace net_instaweb
