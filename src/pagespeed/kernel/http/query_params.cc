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

#include "pagespeed/kernel/http/query_params.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

void QueryParams::Parse(const StringPiece& text) {
  CHECK_EQ(0, size());
  AddFromNameValuePairs(text, "&", '=', false);
}

GoogleString QueryParams::ToString() const {
  GoogleString str;
  const char* prefix="";
  for (int i = 0; i < size(); ++i) {
    if (value(i) == NULL) {
      StrAppend(&str, prefix, name(i));
    } else {
      StrAppend(&str, prefix, name(i), "=", *(value(i)));
    }
    prefix = "&";
  }
  return str;
}

}  // namespace net_instaweb
