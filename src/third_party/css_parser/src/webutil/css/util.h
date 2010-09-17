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

// Copyright 2007 Google Inc. All Rights Reserved.
// Author: yian@google.com (Yian Huang)
// Author: dpeng@google.com (Daniel Peng)

#ifndef WEBUTIL_CSS_UTIL_H__
#define WEBUTIL_CSS_UTIL_H__

#include <string>
#include <vector>

#include "strings/stringpiece.h"
#include "webutil/css/string.h"

class HtmlColor;
class UnicodeText;

namespace Css {

class Value;

namespace Util {

enum COLOR_ATTR {ORIGINAL, TRANSPARENT, UNKNOWN, INHERIT};

// Parses CSS color value (may be a string, identifier or a color) into
// HtmlColor. returns def if the color is invalid. set attr to one of the
// following attributes: ORIGINAL if the color is valid, INHERIT if it is the
// keyword inherit, UNKNOWN if it is the keywoard unknown, TRANSPARENT
// if it is invalid otherwise.
HtmlColor GetCssColor(const Css::Value* val, const HtmlColor& def,
                      COLOR_ATTR* attr);

// Converts length or percentage string to absolute px units. Refer to
// parent_size when seeing 10% (invalid if parent_size is -1). Refer to
// font_size when seeing 1.2EM or 1.2EX. Invalid if val is NULL or it is not
// a number. It can also be invalid if can_negative is set and the value is
// negative, can_unitless works similarly. Returns if parsing succeeds, and
// if so, size stores the result.
bool GetCssLength(const Css::Value* val, double parent_size,
                  double font_size, double unit, bool can_negative,
                  bool can_unitless, double* size);

// Updates color with system color specified in colorstr. The change is only
// done only when the conversion succeeds, indicated by the return value.
// For a list of system colors, please see
//   http://www.w3.org/TR/CSS21/ui.html#system-colors
// Actual system colors depend on OS's graphic environment. For the purpose
// of hidden text detection, we assume a typical setting based on Windows XP
// default theme.
bool GetSystemColor(const string& colorstr, HtmlColor* color);

// Whether a media string (comma separated list of media) is compatible with
// screen-oriented applications. It is valid if no media is specified or some
// medium has the name "screen" or "all".
bool MediaAppliesToScreen(const StringPiece& media);

// Whether a media list is compatible with screen-oriented applications. It
// is valid if no media is specified or some medium has the name "screen" or
// "all".
bool MediaAppliesToScreen(const std::vector<UnicodeText>& media);


}  // namespace Util
}  // namespace Css

#endif  // WEBUTIL_CSS_UTIL_H__
