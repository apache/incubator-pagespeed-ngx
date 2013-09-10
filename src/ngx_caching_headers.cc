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

// Author: jefftk@google.com (Jeff Kaufman)

#include "ngx_caching_headers.h"
#include "ngx_list_iterator.h"

#include "ngx_pagespeed.h"

namespace net_instaweb {

bool NgxCachingHeaders::Lookup(const StringPiece& key,
                               StringPieceVector* values) {
  ngx_table_elt_t* header;
  NgxListIterator it(&(request_->headers_out.headers.part));
  while ((header = it.Next()) != NULL) {
    if (header->hash != 0 && key == str_to_string_piece(header->key)) {
      // This will be called multiple times if there are multiple headers with
      // this name.  Each time it will append to values.
      SplitStringPieceToVector(str_to_string_piece(header->value), ",", values,
                               true /* omit empty strings */);
    }
  }

  if (values->size() == 0) {
    // No header found with this name.
    return false;
  }

  for (int i = 0, n = values->size(); i < n; ++i) {
    TrimWhitespace(&((*values)[i]));
  }

  return true;
}

}  // namespace net_instaweb
