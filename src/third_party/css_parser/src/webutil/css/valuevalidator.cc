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
// Author: yian@google.com (Yi-An Huang)

#include <set>

#include "base/logging.h"
#include "base/macros.h"
#include "util/gtl/map-util.h"
#include "util/gtl/stl_util-inl.h"
#include "webutil/css/valuevalidator.h"

namespace Css {

const Property::Prop kEndProp = static_cast<Property::Prop>(-1);
const Value::ValueType kEndType = static_cast<Value::ValueType>(-1);
const Identifier::Ident kEndIdent = static_cast<Identifier::Ident>(-1);

// For each CSS property (or a group of properties), the valid value types and
// valid IDENT values. We do not list the IDENT, DEFAULT and UNKOWN types
// because they are always valid, we also do not list INHERIT values explicitly
// for the same reason. Arrays are terminated with -1.
static struct valid_prop_info_t {
  Property::Prop props[10];
  Value::ValueType types[10];
  Identifier::Ident idents[30];
  // the following properties make sense for numbers.
  bool accept_percent;
  bool accept_no_unit;
  bool accept_length;
  bool accept_negative;
} kValidPropInfo[] = {
  // Chapter 8: Box model

  { { Property::BORDER_COLOR, Property::BORDER_TOP_COLOR,
      Property::BORDER_RIGHT_COLOR, Property::BORDER_BOTTOM_COLOR,
      Property::BORDER_LEFT_COLOR, kEndProp },
    { Value::COLOR, kEndType },
    { Identifier::TRANSPARENT, Identifier::GOOG_INITIAL, kEndIdent },
  },
  { { Property::BORDER_STYLE, Property::BORDER_TOP_STYLE,
      Property::BORDER_RIGHT_STYLE, Property::BORDER_BOTTOM_STYLE,
      Property::BORDER_LEFT_STYLE, kEndProp },
    { kEndType },
    { Identifier::NONE, Identifier::HIDDEN, Identifier::DOTTED,
      Identifier::DASHED, Identifier::SOLID, Identifier::DOUBLE,
      Identifier::GROOVE, Identifier::RIDGE, Identifier::INSET,
      Identifier::OUTSET, kEndIdent },
  },
  { { Property::BORDER_WIDTH, Property::BORDER_TOP_WIDTH,
      Property::BORDER_RIGHT_WIDTH, Property::BORDER_BOTTOM_WIDTH,
      Property::BORDER_LEFT_WIDTH, kEndProp },
    { Value::NUMBER, kEndType },
    { Identifier::THIN, Identifier::MEDIUM, Identifier::THICK, kEndIdent },
    false, false, true, false,
  },
  { { Property::BORDER, Property::BORDER_TOP, Property::BORDER_RIGHT,
      Property::BORDER_BOTTOM, Property::BORDER_LEFT, kEndProp },
    { Value::COLOR, Value::NUMBER, kEndType },
    { Identifier::TRANSPARENT, Identifier::GOOG_INITIAL,
      Identifier::NONE, Identifier::HIDDEN, Identifier::DOTTED,
      Identifier::DASHED, Identifier::SOLID, Identifier::DOUBLE,
      Identifier::GROOVE, Identifier::RIDGE, Identifier::INSET,
      Identifier::OUTSET,
      Identifier::THIN, Identifier::MEDIUM, Identifier::THICK, kEndIdent },
    false, false, true, false,
  },

  { { Property::MARGIN, Property::MARGIN_RIGHT, Property::MARGIN_LEFT,
      Property::MARGIN_TOP, Property::MARGIN_BOTTOM, kEndProp },
    { Value::NUMBER, kEndType },
    { Identifier::AUTO, kEndIdent },
    true, false, true, true,
  },

  { { Property::PADDING, Property::PADDING_RIGHT, Property::PADDING_LEFT,
      Property::PADDING_TOP, Property::PADDING_BOTTOM, kEndProp },
    { Value::NUMBER, kEndType },
    { kEndIdent },
    true, false, true, false,
  },

  // Chapter 9: Visual formatting model

  { { Property::BOTTOM, Property::LEFT, Property::RIGHT, Property::TOP,
      kEndProp },
    { Value::NUMBER, kEndType },
    { Identifier::AUTO, kEndIdent },
    true, false, true, true,
  },

  { { Property::CLEAR, kEndProp },
    { kEndType },
    { Identifier::NONE, Identifier::LEFT, Identifier::RIGHT, Identifier::BOTH,
      kEndIdent },
  },

  { { Property::DIRECTION, kEndProp },
    { kEndType },
    { Identifier::LTR, Identifier::RTL, kEndIdent },
  },

  { { Property::DISPLAY, kEndProp },
    { kEndType },
    { Identifier::INLINE, Identifier::BLOCK, Identifier::LIST_ITEM,
      Identifier::RUN_IN, Identifier::INLINE_BLOCK, Identifier::TABLE,
      Identifier::INLINE_TABLE, Identifier::TABLE_ROW_GROUP,
      Identifier::TABLE_HEADER_GROUP, Identifier::TABLE_FOOTER_GROUP,
      Identifier::TABLE_ROW, Identifier::TABLE_COLUMN_GROUP,
      Identifier::TABLE_COLUMN, Identifier::TABLE_CELL,
      Identifier::TABLE_CAPTION, Identifier::NONE, kEndIdent },
  },

  { { Property::FLOAT, kEndProp },
    { kEndType },
    { Identifier::LEFT, Identifier::RIGHT, Identifier::NONE, kEndIdent },
  },

  { { Property::POSITION, kEndProp },
    { kEndType },
    { Identifier::STATIC, Identifier::RELATIVE, Identifier::ABSOLUTE,
      Identifier::FIXED, kEndIdent },
  },

  { { Property::UNICODE_BIDI, kEndProp },
    { kEndType },
    { Identifier::NORMAL, Identifier::EMBED, Identifier::BIDI_OVERRIDE,
      kEndIdent },
  },

  { { Property::Z_INDEX, kEndProp },
    { Value::NUMBER, kEndType },
    { Identifier::AUTO, kEndIdent },
    false, true, false, true,
  },

  // Chapter 10: Visual formatting model details

  { { Property::HEIGHT, Property::WIDTH, kEndProp },
    { Value::NUMBER, kEndType },
    { Identifier::AUTO, kEndIdent },
    true, false, true, false,
  },

  { { Property::LINE_HEIGHT, kEndProp },
    { Value::NUMBER, kEndType },
    { Identifier::NORMAL, kEndIdent },
    true, true, true, false,
  },

  { { Property::MAX_HEIGHT, Property::MAX_WIDTH, kEndProp },
    { Value::NUMBER, kEndType },
    { Identifier::NONE, kEndIdent },
    true, false, true, false,
  },

  { { Property::MIN_HEIGHT, Property::MIN_WIDTH, kEndProp },
    { Value::NUMBER, kEndType },
    { kEndIdent },
    true, false, true, false,
  },

  { { Property::VERTICAL_ALIGN, kEndProp },
    { Value::NUMBER, kEndType },
    { Identifier::BASELINE, Identifier::SUB, Identifier::SUPER,
      Identifier::TOP, Identifier::TEXT_TOP, Identifier::MIDDLE,
      Identifier::BOTTOM, Identifier::TEXT_BOTTOM, kEndIdent },
    true, false, true, true,
  },

  // Chapter 11: Visual effects

  { { Property::CLIP, kEndProp },
    { Value::RECT, kEndType },
    { Identifier::AUTO, kEndIdent },
    false, false, true, true,
  },

  { { Property::OVERFLOW, kEndProp },
    { kEndType },
    { Identifier::VISIBLE, Identifier::HIDDEN, Identifier::SCROLL,
      Identifier::AUTO, kEndIdent },
  },

  { { Property::VISIBILITY, kEndProp },
    { kEndType },
    { Identifier::VISIBLE, Identifier::HIDDEN, Identifier::COLLAPSE, kEndIdent
    },
  },

  // Chapter 12: Generated content, automatic numbering, and lists

  { { Property::CONTENT, kEndProp },
    { Value::STRING, Value::URI, Value::FUNCTION, kEndType },
    { Identifier::NORMAL, Identifier::NONE,
      Identifier::OPEN_QUOTE, Identifier::CLOSE_QUOTE,
      Identifier::NO_OPEN_QUOTE, Identifier::NO_CLOSE_QUOTE, kEndIdent },
  },

  { { Property::COUNTER_INCREMENT, Property::COUNTER_RESET, kEndProp },
    { Value::NUMBER, kEndType },
    { Identifier::NONE, Identifier::OTHER, kEndIdent },
    false, true, false, true,
  },

  { { Property::LIST_STYLE_IMAGE, kEndProp },
    { Value::URI, kEndType },
    { Identifier::NONE, kEndIdent },
  },
  { { Property::LIST_STYLE_POSITION, kEndProp },
    { kEndType },
    { Identifier::INSIDE, Identifier::OUTSIDE, kEndIdent },
  },
  { { Property::LIST_STYLE_TYPE, kEndProp },
    { kEndType },
    { Identifier::DISC, Identifier::CIRCLE, Identifier::SQUARE,
      Identifier::DECIMAL, Identifier::DECIMAL_LEADING_ZERO,
      Identifier::LOWER_ROMAN, Identifier::UPPER_ROMAN,
      Identifier::LOWER_GREEK, Identifier::LOWER_LATIN,
      Identifier::UPPER_LATIN, Identifier::ARMENIAN, Identifier::GEORGIAN,
      Identifier::LOWER_ALPHA, Identifier::UPPER_ALPHA,
      Identifier::NONE, kEndIdent },
  },
  { { Property::LIST_STYLE, kEndProp },
    { Value::URI, kEndType },
    { Identifier::NONE, Identifier::INSIDE, Identifier::OUTSIDE,
      Identifier::DISC, Identifier::CIRCLE, Identifier::SQUARE,
      Identifier::DECIMAL, Identifier::DECIMAL_LEADING_ZERO,
      Identifier::LOWER_ROMAN, Identifier::UPPER_ROMAN,
      Identifier::LOWER_GREEK, Identifier::LOWER_LATIN,
      Identifier::UPPER_LATIN, Identifier::ARMENIAN, Identifier::GEORGIAN,
      Identifier::LOWER_ALPHA, Identifier::UPPER_ALPHA, kEndIdent },
  },

  { { Property::QUOTES, kEndProp },
    { Value::STRING, kEndType },
    { Identifier::NONE, kEndIdent },
  },

  // Chapter 13: Paged media

  { { Property::ORPHANS, kEndProp },
    { Value::NUMBER, kEndType },
    { kEndIdent },
    false, true, false, false,
  },

  { { Property::PAGE_BREAK_AFTER, Property::PAGE_BREAK_BEFORE, kEndProp },
    { kEndType },
    { Identifier::AUTO, Identifier::ALWAYS, Identifier::AVOID,
      Identifier::LEFT, Identifier::RIGHT, kEndIdent },
  },
  { { Property::PAGE_BREAK_INSIDE, kEndProp },
    { kEndType },
    { Identifier::AVOID, Identifier::AUTO, kEndIdent },
  },

  { { Property::WIDOWS, kEndProp },
    { Value::NUMBER, kEndType },
    { kEndIdent },
    false, true, false, false,
  },

  // Chapter 14: Colors and Backgrounds

  { { Property::BACKGROUND_ATTACHMENT, kEndProp },
    { kEndType },
    { Identifier::SCROLL, Identifier::FIXED, kEndIdent },
  },
  { { Property::BACKGROUND_COLOR, kEndProp },
    { Value::COLOR, kEndType },
    { Identifier::TRANSPARENT, kEndIdent },
  },
  { { Property::BACKGROUND_IMAGE, kEndProp },
    { Value::URI, kEndType },
    { Identifier::NONE, kEndIdent },
  },
  { { Property::BACKGROUND_POSITION, kEndProp },
    { Value::NUMBER, kEndType },
    { Identifier::LEFT, Identifier::CENTER, Identifier::RIGHT, Identifier::TOP,
      Identifier::BOTTOM, kEndIdent },
    true, false, true, true,
  },
  { { Property::BACKGROUND_REPEAT, kEndProp },
    { kEndType },
    { Identifier::REPEAT, Identifier::REPEAT_X, Identifier::REPEAT_Y,
      Identifier::NO_REPEAT, kEndIdent },
  },
  { { Property::BACKGROUND, kEndProp },
    { Value::COLOR, Value::URI, Value::NUMBER, kEndType },
    { Identifier::SCROLL, Identifier::FIXED, Identifier::TRANSPARENT,
      Identifier::NONE, Identifier::LEFT, Identifier::CENTER,
      Identifier::RIGHT, Identifier::TOP, Identifier::BOTTOM,
      Identifier::REPEAT, Identifier::REPEAT_X, Identifier::REPEAT_Y,
      Identifier::NO_REPEAT, kEndIdent },
    true, false, true, true,
  },

  { { Property::COLOR, kEndProp },
    { Value::COLOR, kEndType },
    { Identifier::GOOG_BODY_COLOR, Identifier::GOOG_BODY_LINK_COLOR,
      kEndIdent },
  },

  // Chapter 15: Fonts

  { { Property::FONT_FAMILY, kEndProp },
    { Value::STRING, kEndType },
    { Identifier::SERIF, Identifier::SANS_SERIF, Identifier::CURSIVE,
      Identifier::FANTASY, Identifier::MONOSPACE, Identifier::OTHER, kEndIdent
    },
  },
  { { Property::FONT_SIZE, kEndProp },
    { Value::NUMBER, kEndType },
    { Identifier::XX_SMALL, Identifier::X_SMALL, Identifier::SMALL,
      Identifier::MEDIUM, Identifier::LARGE, Identifier::X_LARGE,
      Identifier::XX_LARGE, Identifier::LARGER, Identifier::SMALLER,
      Identifier::GOOG_BIG, Identifier::GOOG_SMALL, kEndIdent
    },
    true, false, true, false,
  },
  { { Property::FONT_STYLE, kEndProp },
    { kEndType },
    { Identifier::NORMAL, Identifier::ITALIC, Identifier::OBLIQUE, kEndIdent },
  },
  { { Property::FONT_VARIANT, kEndProp },
    { kEndType },
    { Identifier::NORMAL, Identifier::SMALL_CAPS, kEndIdent },
  },
  { { Property::FONT_WEIGHT, kEndProp },
    { Value::NUMBER, kEndType },
    { Identifier::NORMAL, Identifier::BOLD, Identifier::BOLDER,
      Identifier::LIGHTER, kEndIdent },
    false, true, true, false,
  },
  { { Property::FONT, kEndProp },
    { Value::STRING, Value::NUMBER, kEndType },
    { Identifier::SERIF, Identifier::SANS_SERIF, Identifier::CURSIVE,
      Identifier::FANTASY, Identifier::MONOSPACE, Identifier::OTHER,
      Identifier::XX_SMALL, Identifier::X_SMALL, Identifier::SMALL,
      Identifier::MEDIUM, Identifier::LARGE, Identifier::X_LARGE,
      Identifier::XX_LARGE, Identifier::LARGER, Identifier::SMALLER,
      Identifier::NORMAL, Identifier::ITALIC, Identifier::OBLIQUE,
      Identifier::SMALL_CAPS, Identifier::BOLD, Identifier::BOLDER,
      Identifier::LIGHTER,
      Identifier::CAPTION, Identifier::ICON, Identifier::MENU,
      Identifier::MESSAGE_BOX, Identifier::MESSAGE_BOX,
      Identifier::SMALL_CAPTION, Identifier::STATUS_BAR, kEndIdent },
    true, true, true, false,
  },

  // Chapter 16: Text

  { { Property::LETTER_SPACING, kEndProp },
    { Value::NUMBER, kEndType },
    { Identifier::NORMAL, kEndIdent },
    false, false, true, true,
  },

  { { Property::TEXT_ALIGN, kEndProp },
    { kEndType },
    { Identifier::LEFT, Identifier::RIGHT, Identifier::CENTER,
      Identifier::JUSTIFY, Identifier::GOOG_INITIAL, kEndIdent },
  },
  { { Property::TEXT_DECORATION, kEndProp },
    { kEndType },
    { Identifier::NONE, Identifier::UNDERLINE, Identifier::OVERLINE,
      Identifier::LINE_THROUGH, Identifier::BLINK, kEndIdent },
  },
  { { Property::TEXT_INDENT, kEndProp },
    { Value::NUMBER, kEndType },
    { kEndIdent },
    true, false, true, true,
  },
  { { Property::TEXT_TRANSFORM, kEndProp },
    { kEndType },
    { Identifier::CAPITALIZE, Identifier::UPPERCASE, Identifier::LOWERCASE,
      Identifier::NONE, kEndIdent },
  },

  { { Property::WHITE_SPACE, kEndProp },
    { kEndType },
    { Identifier::NORMAL, Identifier::PRE, Identifier::NOWRAP,
      Identifier::PRE_WRAP, Identifier::PRE_LINE, kEndIdent },
  },

  { { Property::WORD_SPACING, kEndProp },
    { Value::NUMBER, kEndType },
    { Identifier::NORMAL, kEndIdent },
    false, false, true, true,
  },

  // Chapter 17: Tables

  { { Property::BORDER_COLLAPSE, kEndProp },
    { kEndType },
    { Identifier::COLLAPSE, Identifier::SEPARATE, kEndIdent },
  },
  { { Property::BORDER_SPACING, kEndProp },
    { Value::NUMBER, kEndType },
    { kEndIdent },
    false, false, true, false,
  },

  { { Property::CAPTION_SIDE, kEndProp },
    { kEndType },
    { Identifier::TOP, Identifier::BOTTOM, kEndIdent },
  },

  { { Property::EMPTY_CELLS, kEndProp },
    { kEndType },
    { Identifier::SHOW, Identifier::HIDE, kEndIdent },
  },

  { { Property::TABLE_LAYOUT, kEndProp },
    { kEndType },
    { Identifier::AUTO, Identifier::FIXED, kEndIdent },
  },

  // Chapter 18: User interface

  { { Property::CURSOR, kEndProp },
    { Value::URI, kEndType },
    { Identifier::AUTO, Identifier::CROSSHAIR, Identifier::DEFAULT,
      Identifier::POINTER, Identifier::MOVE, Identifier::E_RESIZE,
      Identifier::NE_RESIZE, Identifier::NW_RESIZE, Identifier::N_RESIZE,
      Identifier::SE_RESIZE, Identifier::SW_RESIZE, Identifier::S_RESIZE,
      Identifier::W_RESIZE, Identifier::TEXT, Identifier::WAIT,
      Identifier::HELP, Identifier::PROGRESS, kEndIdent },
  },

  { { Property::OUTLINE_COLOR, kEndProp },
    { Value::COLOR, kEndType },
    { Identifier::INVERT, kEndIdent },
  },
  { { Property::OUTLINE_STYLE, kEndProp },
    { kEndType },
    { Identifier::NONE, Identifier::DOTTED, Identifier::DASHED,
      Identifier::SOLID, Identifier::DOUBLE, Identifier::GROOVE,
      Identifier::RIDGE, Identifier::INSET, Identifier::OUTSET, kEndIdent },
  },
  { { Property::OUTLINE_WIDTH, kEndProp },
    { Value::NUMBER, kEndType },
    { Identifier::THIN, Identifier::MEDIUM, Identifier::THICK, kEndIdent },
    false, false, true, false,
  },
  { { Property::OUTLINE, kEndProp },
    { Value::COLOR, Value::NUMBER, kEndType },
    { Identifier::INVERT, Identifier::NONE, Identifier::DOTTED,
      Identifier::DASHED, Identifier::SOLID, Identifier::DOUBLE,
      Identifier::GROOVE, Identifier::RIDGE, Identifier::INSET,
      Identifier::OUTSET, Identifier::THIN, Identifier::MEDIUM,
      Identifier::THICK, kEndIdent },
    false, false, true, false,
  }
};

struct PropertyValidationInfo {
  std::set<Value::ValueType> valid_types;
  std::set<Identifier::Ident> valid_idents;
  bool accept_percent;
  bool accept_no_unit;
  bool accept_length;
  bool accept_negative;
};

ValueValidator::ValueValidator() {
  validation_info_.resize(Property::OTHER + 1);
  for (int i = 0, n = arraysize(kValidPropInfo); i < n; ++i) {
    std::set<Value::ValueType> t;
    t.insert(Value::IDENT);
    t.insert(Value::UNKNOWN);
    t.insert(Value::DEFAULT);
    for (int j = 0; kValidPropInfo[i].types[j] != -1; ++j) {
      CHECK_LT(j, arraysize(kValidPropInfo[i].types));
      t.insert(kValidPropInfo[i].types[j]);
    }

    std::set<Identifier::Ident> s;
    s.insert(Identifier::INHERIT);
    for (int j = 0; kValidPropInfo[i].idents[j] != -1; ++j) {
      CHECK_LT(j, arraysize(kValidPropInfo[i].idents));
      s.insert(kValidPropInfo[i].idents[j]);
    }

    for (int j = 0; kValidPropInfo[i].props[j] != -1; ++j) {
      CHECK_LT(j, arraysize(kValidPropInfo[i].props));
      PropertyValidationInfo* info = new PropertyValidationInfo;
      validation_info_[kValidPropInfo[i].props[j]] = info;
      info->valid_types = t;
      info->valid_idents = s;
      info->accept_percent = kValidPropInfo[i].accept_percent;
      info->accept_no_unit = kValidPropInfo[i].accept_no_unit;
      info->accept_length = kValidPropInfo[i].accept_length;
      info->accept_negative = kValidPropInfo[i].accept_negative;
    }
  }
}

ValueValidator::~ValueValidator() {
  STLDeleteElements(&validation_info_);
}

bool ValueValidator::IsValidValue(Property::Prop prop,
                                  const Value& value,
                                  bool quirks_mode) const {
  if (!IsValidType(prop, value.GetLexicalUnitType()))
    return false;
  if (value.GetLexicalUnitType() == Value::IDENT &&
      !IsValidIdentifier(prop, value.GetIdentifier().ident()))
      return false;
  if (value.GetLexicalUnitType() == Value::NUMBER &&
      !IsValidNumber(prop, value, quirks_mode))
    return false;
  if (value.GetLexicalUnitType() == Value::RECT) {
    const Values* params = value.GetParameters();
    CHECK(params != NULL && params->size() == 4);
    for (Values::const_iterator iter = params->begin();
         iter < params->end(); ++iter) {
      const Value* param = *iter;
      if (param->GetLexicalUnitType() == Value::IDENT) {
        if (!IsValidIdentifier(prop, param->GetIdentifier().ident()))
          return false;
      } else if (param->GetLexicalUnitType() == Value::NUMBER) {
        if (!IsValidNumber(prop, *param, quirks_mode))
          return false;
      } else {
        return false;
      }
    }
  }
  return true;
}

bool ValueValidator::IsValidType(Property::Prop prop,
                                 Value::ValueType type) const {
  const PropertyValidationInfo* info = validation_info_[prop];
  // If we don't have information about prop, then all values are accepted.
  return info == NULL || ContainsKey(info->valid_types, type);
}

bool ValueValidator::IsValidIdentifier(Property::Prop prop,
                                       Identifier::Ident ident) const {
  const PropertyValidationInfo* info = validation_info_[prop];
  // If we don't have information about prop, then all values are accepted.
  return info == NULL || ContainsKey(info->valid_idents, ident);
}

bool ValueValidator::IsValidNumber(Property::Prop prop, const Value& value,
                                   bool quirks_mode) const {
  const PropertyValidationInfo* info = validation_info_[prop];
  // If we don't have information about prop, then all values are accepted.
  if (info == NULL) return true;

  switch (value.GetDimension()) {
    case Value::OTHER:
      return false;
    case Value::DEG:
    case Value::RAD:
    case Value::GRAD:
    case Value::HZ:
    case Value::KHZ:
    case Value::MS:
    case Value::S:
      return false;   // unless we handle aural properties
    case Value::PERCENT:
      if (!info->accept_percent)
        return false;
      break;
    case Value::NO_UNIT:
      // We accept no-unit numbers if any of
      //   1) accepted by the property,
      //   2) 0 is always accepted,
      //   3) in quirks mode.
      if (!info->accept_no_unit && !quirks_mode &&
          value.GetFloatValue() != 0.0)
        return false;
      break;
    default:
      if (!info->accept_length)
        return false;
      break;
  }
  if (value.GetFloatValue() < 0 && !info->accept_negative)
    return false;
  return true;
}
}  // namespace
