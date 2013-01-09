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

// Copyright 2006 Google Inc. All Rights Reserved.
// Author: dpeng@google.com (Daniel Peng)
//
// Property represents the name of a CSS property (e.g.,
// background, font-face, font-size).  If we recognize the property,
// we store it as an enum.  Otherwise, we store the text for the name
// of the property.


#ifndef WEBUTIL_CSS_PROPERTY_H__
#define WEBUTIL_CSS_PROPERTY_H__

#include <cmath>
#include <string>

#include "util/utf8/public/unicodetext.h"
#include "webutil/css/string.h"

// resolving conflict on macro OVERFLOW defined in math.h.
#ifdef OVERFLOW
const int OVERFLOW_temporary = OVERFLOW;
#undef OVERFLOW
const int OVERFLOW = OVERFLOW_temporary;
#endif

namespace Css {

class Property {
 public:
  enum Prop {
    _WEBKIT_APPEARANCE, BACKGROUND_ATTACHMENT,
    _WEBKIT_BACKGROUND_CLIP, BACKGROUND_COLOR,
    _WEBKIT_BACKGROUND_COMPOSITE, BACKGROUND_IMAGE,
    _WEBKIT_BACKGROUND_ORIGIN, BACKGROUND_POSITION, BACKGROUND_POSITION_X,
    BACKGROUND_POSITION_Y, BACKGROUND_REPEAT, _WEBKIT_BACKGROUND_SIZE,
    _WEBKIT_BINDING, BORDER_COLLAPSE, _WEBKIT_BORDER_IMAGE,
    BORDER_SPACING, _WEBKIT_BORDER_HORIZONTAL_SPACING,
    _WEBKIT_BORDER_VERTICAL_SPACING, _WEBKIT_BORDER_RADIUS,
    _WEBKIT_BORDER_TOP_LEFT_RADIUS, _WEBKIT_BORDER_TOP_RIGHT_RADIUS,
    _WEBKIT_BORDER_BOTTOM_LEFT_RADIUS, _WEBKIT_BORDER_BOTTOM_RIGHT_RADIUS,
    BORDER_TOP_COLOR, BORDER_RIGHT_COLOR, BORDER_BOTTOM_COLOR,
    BORDER_LEFT_COLOR, BORDER_TOP_STYLE, BORDER_RIGHT_STYLE,
    BORDER_BOTTOM_STYLE, BORDER_LEFT_STYLE, BORDER_TOP_WIDTH,
    BORDER_RIGHT_WIDTH, BORDER_BOTTOM_WIDTH, BORDER_LEFT_WIDTH, BOTTOM,
    _WEBKIT_BOX_ALIGN, _WEBKIT_BOX_DIRECTION, _WEBKIT_BOX_FLEX,
    _WEBKIT_BOX_FLEX_GROUP, _WEBKIT_BOX_LINES, _WEBKIT_BOX_ORDINAL_GROUP,
    _WEBKIT_BOX_ORIENT, _WEBKIT_BOX_PACK, BOX_SIZING, CAPTION_SIDE, CLEAR,
    CLIP, COLOR, CONTENT, COUNTER_INCREMENT, COUNTER_RESET, CURSOR,
    DIRECTION, DISPLAY, EMPTY_CELLS, FLOAT, FONT_FAMILY, FONT_SIZE,
    _WEBKIT_FONT_SIZE_DELTA, FONT_STRETCH, FONT_STYLE, FONT_VARIANT,
    FONT_WEIGHT, HEIGHT, _WEBKIT_HIGHLIGHT, LEFT, LETTER_SPACING,
    _WEBKIT_LINE_CLAMP, LINE_HEIGHT, LIST_STYLE_IMAGE,
    LIST_STYLE_POSITION, LIST_STYLE_TYPE, MARGIN_TOP, MARGIN_RIGHT,
    MARGIN_BOTTOM, MARGIN_LEFT, _WEBKIT_LINE_BREAK,
    _WEBKIT_MARGIN_COLLAPSE, _WEBKIT_MARGIN_TOP_COLLAPSE,
    _WEBKIT_MARGIN_BOTTOM_COLLAPSE, _WEBKIT_MARGIN_START, _WEBKIT_MARQUEE,
    _WEBKIT_MARQUEE_DIRECTION, _WEBKIT_MARQUEE_INCREMENT,
    _WEBKIT_MARQUEE_REPETITION, _WEBKIT_MARQUEE_SPEED,
    _WEBKIT_MARQUEE_STYLE, _WEBKIT_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR,
    MAX_HEIGHT, MAX_WIDTH, MIN_HEIGHT, MIN_WIDTH, _WEBKIT_NBSP_MODE,
    OPACITY, ORPHANS, OUTLINE_COLOR, OUTLINE_OFFSET, OUTLINE_STYLE,
    OUTLINE_WIDTH, OVERFLOW, OVERFLOW_X, OVERFLOW_Y, PADDING_TOP,
    PADDING_RIGHT, PADDING_BOTTOM, PADDING_LEFT, _WEBKIT_PADDING_START,
    PAGE, PAGE_BREAK_AFTER, PAGE_BREAK_BEFORE, PAGE_BREAK_INSIDE,
    POSITION, QUOTES, RIGHT, SIZE, TABLE_LAYOUT, TEXT_ALIGN,
    TEXT_DECORATION, TEXT_INDENT, TEXT_LINE_THROUGH,
    TEXT_LINE_THROUGH_COLOR, TEXT_LINE_THROUGH_MODE,
    TEXT_LINE_THROUGH_STYLE, TEXT_LINE_THROUGH_WIDTH, TEXT_OVERFLOW,
    TEXT_OVERLINE, TEXT_OVERLINE_COLOR, TEXT_OVERLINE_MODE,
    TEXT_OVERLINE_STYLE, TEXT_OVERLINE_WIDTH, _WEBKIT_TEXT_SECURITY,
    TEXT_SHADOW, TEXT_TRANSFORM, TEXT_UNDERLINE, TEXT_UNDERLINE_COLOR,
    TEXT_UNDERLINE_MODE, TEXT_UNDERLINE_STYLE, TEXT_UNDERLINE_WIDTH,
    RESIZE, _WEBKIT_TEXT_SIZE_ADJUST, _WEBKIT_DASHBOARD_REGION, TOP,
    UNICODE_BIDI, _WEBKIT_USER_DRAG, _WEBKIT_USER_MODIFY,
    _WEBKIT_USER_SELECT, VERTICAL_ALIGN, VISIBILITY, WHITE_SPACE, WIDOWS,
    WIDTH, WORD_WRAP, WORD_SPACING, Z_INDEX, BACKGROUND, BORDER,
    BORDER_COLOR, BORDER_STYLE, BORDER_TOP, BORDER_RIGHT, BORDER_BOTTOM,
    BORDER_LEFT, BORDER_WIDTH, FONT, LIST_STYLE, MARGIN, OUTLINE, PADDING,
    SCROLLBAR_FACE_COLOR, SCROLLBAR_SHADOW_COLOR,
    SCROLLBAR_HIGHLIGHT_COLOR, SCROLLBAR_3DLIGHT_COLOR,
    SCROLLBAR_DARKSHADOW_COLOR, SCROLLBAR_TRACK_COLOR,
    SCROLLBAR_ARROW_COLOR, _WEBKIT_TEXT_DECORATIONS_IN_EFFECT,
    _WEBKIT_RTL_ORDERING,
    // Dummy property value used for when we failed to parse a declaration and
    // so all we can do is store the verbatim text.
    UNPARSEABLE,
    // If OTHER, actual property text is stored in other_.
    OTHER,
  };

  // Constructor.
  explicit Property(UnicodeText s);
  Property(Prop prop) : prop_(prop) { }

  // Accessors.
  //
  // prop() returns the property enum -- OTHER if unrecognized.
  Prop prop() const { return prop_; }

  // prop_text() returns the property as a string.
  string prop_text() const {
    if (prop_ == OTHER)
      return string(other_.utf8_data(), other_.utf8_length());
    else
      return TextFromProp(prop_);
  }

  // Static methods mapping between Prop and strings:
  //
  // Given the text of a CSS property, PropFromText returns the
  // corresponding enum.  If no such property is found, UnitFromText
  // returns OTHER.  Since all CSS properties are ASCII, we are happy
  // with ASCII, UTF8, Latin-1, etc.
  static Prop PropFromText(const char* s, int len);
  // Given a Prop, returns its string representation.  If u is
  // NO_UNIT, returns "".  If u is OTHER, we return "OTHER", but this
  // may not be what you want.
  static const char* TextFromProp(Prop p);

 private:
  Prop prop_;
  UnicodeText other_;  // valid if prop_ is OTHER.

  Property();  // sorry, not default-constructible.
};

}  // namespace

#endif  // WEBUTIL_CSS_PROPERTY_H__
