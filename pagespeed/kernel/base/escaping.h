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

#ifndef PAGESPEED_KERNEL_BASE_ESCAPING_H_
#define PAGESPEED_KERNEL_BASE_ESCAPING_H_

#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

// Appends version of original escaped for JS string syntax, safe for inclusion
// into HTML, to *escaped, (optionally with quotes, if asked).
void EscapeToJsStringLiteral(const StringPiece& original,
                             bool add_quotes,
                             GoogleString* escaped);

// Appends version of original escaped for JSON string syntax to *escaped,
// (optionally with quotes, if asked).
//
// Warning: this is dangerous if you have non-ASCII characters, in that it
// doesn't interpret the input encoding, and will just blindly turn them
// into \u escapes. However, it will ensure that the output won't have any
// dangerous characters that can cause format sniff.
void EscapeToJsonStringLiteral(const StringPiece& original,
                               bool add_quotes,
                               GoogleString* escaped);

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_ESCAPING_H_
