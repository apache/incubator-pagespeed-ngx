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
// Author: yian@google.com (Yi-an Huang)
//
// Identifier represents the value of a CSS identifier (e.g.,
// normal, repeat, small, inherit).  If we recognize the ident,
// we store it as an enum.  Otherwise, we store the text for the
// string value of the identifier.
//
// This code is based on CSS 2.1.

#ifndef WEBUTIL_CSS_IDENTIFIER_H__
#define WEBUTIL_CSS_IDENTIFIER_H__

#include "util/utf8/public/unicodetext.h"

namespace Css {

class Identifier {
 public:
  enum Ident {
    // local add
    // UNKNOWN identifiers. Reserved for internal use.
    GOOG_UNKNOWN,
    // common values
    INHERIT, NONE, AUTO, NORMAL, VISIBLE, HIDDEN, ALWAYS, AVOID, MEDIUM,
    REPEAT, COLLAPSE, LEFT, CENTER, RIGHT, TOP, BOTTOM, BOTH,
    SCROLL, FIXED,
    // background-color
    TRANSPARENT,
    // background-repeat
    REPEAT_X, REPEAT_Y, NO_REPEAT,
    // border-collapse
    SEPARATE,
    // border-style
    DOTTED, DASHED, SOLID, DOUBLE, GROOVE, RIDGE, INSET, OUTSET,
    // border-width
    THIN, THICK,
    // content
    OPEN_QUOTE, CLOSE_QUOTE, NO_OPEN_QUOTE, NO_CLOSE_QUOTE,
    // cursor
    CROSSHAIR, DEFAULT, POINTER, MOVE, E_RESIZE, NE_RESIZE, NW_RESIZE,
    N_RESIZE, SE_RESIZE, SW_RESIZE, S_RESIZE, W_RESIZE, TEXT, WAIT, HELP,
    PROGRESS,
    // direction,
    LTR, RTL,
    // display
    INLINE, BLOCK, LIST_ITEM, RUN_IN, INLINE_BLOCK, TABLE, INLINE_TABLE,
    TABLE_ROW_GROUP, TABLE_HEADER_GROUP, TABLE_FOOTER_GROUP, TABLE_ROW,
    TABLE_COLUMN_GROUP, TABLE_COLUMN, TABLE_CELL, TABLE_CAPTION,
    // empty-cells
    SHOW, HIDE,
    // font-family
    SERIF, SANS_SERIF, CURSIVE, FANTASY, MONOSPACE,
    // font-size
    XX_SMALL, X_SMALL, SMALL, LARGE, X_LARGE, XX_LARGE, SMALLER, LARGER,
    // font-style
    ITALIC, OBLIQUE,
    // font-variant
    SMALL_CAPS,
    // font-weight
    BOLD, BOLDER, LIGHTER,
    // font
    CAPTION, ICON, MENU, MESSAGE_BOX, SMALL_CAPTION, STATUS_BAR,
    // list-style-position
    INSIDE, OUTSIDE,
    // list-style-type
    DISC, CIRCLE, SQUARE, DECIMAL, DECIMAL_LEADING_ZERO, LOWER_ROMAN,
    UPPER_ROMAN, LOWER_GREEK, LOWER_LATIN, UPPER_LATIN, ARMENIAN, GEORGIAN,
    LOWER_ALPHA, UPPER_ALPHA,
    // outline-color
    INVERT,
    // position
    STATIC, RELATIVE, ABSOLUTE,
    // text-align
    JUSTIFY,
    // text-decoration
    UNDERLINE, OVERLINE, LINE_THROUGH, BLINK,
    // text-transform
    CAPITALIZE, UPPERCASE, LOWERCASE,
    // unicode-bidi
    EMBED, BIDI_OVERRIDE,
    // vertical-align
    BASELINE, SUB, SUPER, TEXT_TOP, MIDDLE, TEXT_BOTTOM,
    // white-space
    PRE, NOWRAP, PRE_WRAP, PRE_LINE,
    // google specific. Internal use only.
    // For property with context-dependent initial values. such as border-color
    // and text-align.
    GOOG_INITIAL,
    // color specified by <body text=color>
    GOOG_BODY_COLOR,
    // color specified by <body link=color>
    GOOG_BODY_LINK_COLOR,
    // identifier reserved for font-size in <big> and <small>. IE has special
    // semantics for them.
    GOOG_BIG, GOOG_SMALL,
    OTHER
  };

  // Constructor.
  Identifier() : ident_(GOOG_UNKNOWN) { }
  explicit Identifier(const UnicodeText& s);
  explicit Identifier(Ident ident) : ident_(ident) { }

  // Accessors.
  //
  // ident() returns the ident enum -- OTHER if unrecognized.
  Ident ident() const { return ident_; }

  // ident_text() returns the identifier as a string.
  UnicodeText ident_text() const {
    if (ident_ == OTHER)
      return other_;
    else
      return TextFromIdent(ident_);
  }

  // Static methods mapping between Ident and strings:
  //
  // Given the text of a CSS identifier, IdentFromText returns the
  // corresponding enum.  If no such identifier is found, IdentromText returns
  // OTHER.
  static Ident IdentFromText(const UnicodeText& s);
  // Given a Ident, returns its string representation.  If u is OTHER, we
  // return "OTHER", but this may not be what you want.
  static UnicodeText TextFromIdent(Ident p);

 private:
  Ident ident_;
  UnicodeText other_;  // valid if ident_ is OTHER.
};

}  // namespace

#endif  // WEBUTIL_CSS_IDENTIFIER_H__
