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

// Author: jefftk@google.com (Jeff Kaufman)

#ifndef NGX_PAGESPEED_H_
#define NGX_PAGESPEED_H_

extern "C" {
  #include <ngx_core.h>
}

#include "net/instaweb/util/public/string_util.h"

StringPiece
ngx_http_pagespeed_str_to_string_piece(ngx_str_t s);

// s1: ngx_str_t, s2: string literal
// true if they're equal, false otherwise
#define STR_EQ_LITERAL(s1, s2)          \
    ((s1).len == (sizeof(s2)-1) &&      \
     ngx_strncmp((s1).data, (s2), (sizeof(s2)-1)) == 0)

// Allocate memory out of the pool for the string piece, and copy the contents
// over.  Returns NULL if we can't get memory.
char*
ngx_http_string_piece_to_pool_string(ngx_pool_t* pool, StringPiece sp);

#endif  // NGX_PAGESPEED_H_
