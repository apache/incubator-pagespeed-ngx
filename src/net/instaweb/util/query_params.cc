/*
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

#include "net/instaweb/util/public/query_params.h"

#include <stdio.h>
#include <vector>
#include "base/logging.h"
#include "net/instaweb/util/public/message_handler.h"

namespace net_instaweb {

void QueryParams::Parse(const StringPiece& text) {
  CHECK_EQ(0, size());
  std::vector<StringPiece> components;
  SplitStringPieceToVector(text, "&", &components, true);
  for (int i = 0, n = components.size(); i < n; ++i) {
    StringPiece::size_type pos = components[i].find('=');
    if (pos != StringPiece::npos) {
      Add(components[i].substr(0, pos), components[i].substr(pos + 1));
    } else {
      Add(components[i], StringPiece(NULL, 0));
    }
  }
}

GoogleString QueryParams::ToString() const {
  GoogleString str;
  const char* prefix="";
  for (int i = 0; i < size(); ++i) {
    if (value(i) == NULL) {
      str += StrCat(prefix, name(i));
    } else {
      str += StrCat(prefix, name(i), "=", *(value(i)));
    }
    prefix = "&";
  }
  return str;
}

}  // namespace net_instaweb
