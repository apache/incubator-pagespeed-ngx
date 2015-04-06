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

// Copyright 2012 Google Inc. All Rights Reserved.
// Author: sligocki@google.com (Shawn Ligocki)

#ifndef WEBUTIL_CSS_TOSTRING_H_
#define WEBUTIL_CSS_TOSTRING_H_

#include "strings/stringpiece.h"
#include "webutil/css/string.h"

class UnicodeText;

namespace Css {

// Escape text so that it is safe to put in "" in a CSS string.
string EscapeString(StringPiece src);
string EscapeString(const UnicodeText& src);
// Escape a text URL so that it is safe to put in url() in CSS.
string EscapeUrl(StringPiece src);
string EscapeUrl(const UnicodeText& src);
// Escape an identifier so that it will be re-parsed correctly.
string EscapeIdentifier(StringPiece src);
string EscapeIdentifier(const UnicodeText& src);

}  // namespace Css

#endif  // WEBUTIL_CSS_TOSTRING_H_
