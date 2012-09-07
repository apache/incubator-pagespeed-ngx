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
// Author: dpeng@google.com (Daniel Peng)

#include "webutil/css/util.h"

#include "base/commandlineflags.h"
#include "strings/ascii_ctype.h"
#include "strings/memutil.h"
#include "strings/stringpiece_utils.h"
#include "strings/strutil.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/media.h"
#include "webutil/css/parser.h"
#include "webutil/css/string_util.h"

DEFINE_double(font_size_adjustment,
              0.58,
              "Ratio of x-height to font-size in CSS terms");

namespace Css {
namespace Util {

HtmlColor GetCssColor(const Css::Value* val,
                      const HtmlColor& def,
                      COLOR_ATTR* attr) {
  if (val) {
    switch (val->GetLexicalUnitType()) {
      case Value::COLOR:
        if (val->GetColorValue().IsDefined()) {
          if (attr) *attr = ORIGINAL;
          return val->GetColorValue();
        }
        break;
      case Value::UNKNOWN:
        if (attr) *attr = UNKNOWN;
        return def;
      case Value::IDENT:
        switch (val->GetIdentifier().ident()) {
          case Identifier::INHERIT:
            if (attr) *attr = INHERIT;
            return def;
          case Identifier::TRANSPARENT:
            if (attr) *attr = TRANSPARENT;
            return def;
          default:
            break;
        }
        break;
      default:
        break;
    }
  }
  if (attr) *attr = TRANSPARENT;
  return def;
}

bool GetCssLength(const Css::Value* val, double parent_size,
                  double font_size, double unit,
                  bool can_negative, bool can_unitless,
                  double* size) {
  if (val == NULL || val->GetLexicalUnitType() != Css::Value::NUMBER ||
      (!can_negative && val->GetFloatValue() < 0))
    return false;

  switch (val->GetDimension()) {
    case Css::Value::PERCENT:
      if (parent_size != -1) {
        *size = val->GetFloatValue() * parent_size / 100.0;
        return true;
      }
      break;
    case Css::Value::PX:
      *size = val->GetFloatValue();
      return true;
    case Css::Value::EM:
      *size = val->GetFloatValue() * font_size;
      return true;
    case Css::Value::EX:
      *size = val->GetFloatValue() * font_size * FLAGS_font_size_adjustment;
      return true;
    case Css::Value::MM:
      *size = val->GetFloatValue() / 0.265;
      return true;
    case Css::Value::CM:
      *size = val->GetFloatValue() / 0.265 * 10;
      return true;
    case Css::Value::IN:
      *size = val->GetFloatValue() * 96;
      return true;
    case Css::Value::PT:
      *size = val->GetFloatValue() * 4 / 3;
      return true;
    case Css::Value::PC:
      *size = val->GetFloatValue() * 16;
      return true;
    case Css::Value::NO_UNIT:
      // 0 without unit is always allowed
      if (can_unitless || val->GetFloatValue() == 0.0) {
        *size = val->GetFloatValue() * unit;
        return true;
      }
      break;
    default:
      break;
  }
  return false;
}

// Css system colors.
// The structure is nearly literally copied from webutil/html/htmlcolor.cc.

typedef struct RgbValue {
  unsigned char r_;
  unsigned char g_;
  unsigned char b_;
} RgbValue;

// Color table for system colors.
// This is only a rough estimation based on a typical setup.
// when making change to known_system_color_values, please
// also change the GetKnownSystemColorValue function because
// entire table is hardcoded into the function for efficiency
static const RgbValue known_system_color_values[] = {
/* 0 activeborder */{212, 208, 200},
/* 1 activecaption */{  0,  84, 227},
/* 2 appworkspace */{128, 128, 128},
/* 3 background */{  0,  78, 152},
/* 4 buttonface */{236, 233, 216},
/* 5 buttonhighlight */{255, 255, 255},
/* 6 buttonshadow */{172, 168, 153},
/* 7 buttontext */{  0,   0,   0},
/* 8 captiontext */{255, 255, 255},
/* 9 graytext */{172, 168, 153},
/*10 highlight */{ 49, 106, 197},
/*11 highlighttext */{255, 255, 255},
/*12 inactiveborder */{212, 208, 200},
/*13 inactivecaption */{122, 150, 223},
/*14 inactivecaptiontext */{216, 228, 248},
/*15 infobackground */{255, 255, 225},
/*16 infotext */{  0,   0,   0},
/*17 menu */{255, 255, 255},
/*18 menutext */{  0,   0,   0},
/*19 scrollbar */{212, 208, 200},
/*20 threeddarkshadow */{113, 111, 100},
/*21 threedface */{236, 233, 216},
/*22 threedhighlight */{255, 255, 255},
/*23 threedlightshadow */{241, 239, 226},
/*24 threedshadow */{172, 168, 153},
/*25 window */{255, 255, 255},
/*26 windowframe */{  0,   0,   0},
/*27 windowtext */{  0,   0,   0},
};

const RgbValue* GetKnownSystemColorValue(const char *colorstr) {
  switch (ascii_tolower(colorstr[0])) {
  case 'a':
    switch (ascii_tolower(colorstr[1])) {
    case 'c':
      // TODO(sligocki): Use a locale-independent function instead of
      // strcasecmp so that this code is more portable.
      if (!strcasecmp("activeborder", colorstr)) {
        return &known_system_color_values[0];
      } else if (!strcasecmp("activecaption", colorstr)) {
        return &known_system_color_values[1];
      }
      return NULL;
    case 'p':
      if (!strcasecmp("appworkspace", colorstr)) {
        return &known_system_color_values[2];
      }
      return NULL;
    }
    return NULL;
  case 'b':
    switch (ascii_tolower(colorstr[1])) {
    case 'a':
      if (!strcasecmp("background", colorstr)) {
        return &known_system_color_values[3];
      }
      return NULL;
    case 'u':
      if (!strcasecmp("buttonface", colorstr)) {
        return &known_system_color_values[4];
      } else if (!strcasecmp("buttonhighlight", colorstr)) {
        return &known_system_color_values[5];
      } else if (!strcasecmp("buttonshadow", colorstr)) {
        return &known_system_color_values[6];
      } else if (!strcasecmp("buttontext", colorstr)) {
        return &known_system_color_values[7];
      }
      return NULL;
    }
    return NULL;
  case 'c':
    if (!strcasecmp("captiontext", colorstr)) {
      return &known_system_color_values[8];
    }
    return NULL;
  case 'g':
    if (!strcasecmp("graytext", colorstr)) {
      return &known_system_color_values[9];
    }
    return NULL;
  case 'h':
    if (!strcasecmp("highlight", colorstr)) {
      return &known_system_color_values[10];
    } else if (!strcasecmp("highlighttext", colorstr)) {
      return &known_system_color_values[11];
    }
    return NULL;
  case 'i':
    if (!strcasecmp("inactiveborder", colorstr)) {
      return &known_system_color_values[12];
    } else if (!strcasecmp("inactivecaption", colorstr)) {
      return &known_system_color_values[13];
    } else if (!strcasecmp("inactivecaptiontext", colorstr)) {
      return &known_system_color_values[14];
    } else if (!strcasecmp("infobackground", colorstr)) {
      return &known_system_color_values[15];
    } else if (!strcasecmp("infotext", colorstr)) {
      return &known_system_color_values[16];
    }
    return NULL;
  case 'm':
    if (!strcasecmp("menu", colorstr)) {
      return &known_system_color_values[17];
    } else if (!strcasecmp("menutext", colorstr)) {
      return &known_system_color_values[18];
    }
    return NULL;
  case 's':
    if (!strcasecmp("scrollbar", colorstr)) {
      return &known_system_color_values[19];
    }
    return NULL;
  case 't':
    if (!strcasecmp("threeddarkshadow", colorstr)) {
      return &known_system_color_values[20];
    } else if (!strcasecmp("threedface", colorstr)) {
      return &known_system_color_values[21];
    } else if (!strcasecmp("threedhighlight", colorstr)) {
      return &known_system_color_values[22];
    } else if (!strcasecmp("threedlightshadow", colorstr)) {
      return &known_system_color_values[23];
    } else if (!strcasecmp("threedshadow", colorstr)) {
      return &known_system_color_values[24];
    }
    return NULL;
  case 'w':
    if (!strcasecmp("window", colorstr)) {
      return &known_system_color_values[25];
    } else if (!strcasecmp("windowframe", colorstr)) {
      return &known_system_color_values[26];
    } else if (!strcasecmp("windowtext", colorstr)) {
      return &known_system_color_values[27];
    }
    return NULL;
  }
  return NULL;
}

bool GetSystemColor(const string& colorstr, HtmlColor* color) {
  const RgbValue *p_rgb = GetKnownSystemColorValue(colorstr.c_str());
  if (p_rgb) {
    color->SetValueFromRGB(p_rgb->r_, p_rgb->g_, p_rgb->b_);
    return true;
  } else {
    return false;
  }
}

namespace {

bool MediumAppliesToScreen(const StringPiece& medium) {
  return (StringCaseEquals(medium, "all") ||
          StringCaseEquals(medium, "screen"));
}
bool MediumAppliesToScreen(const UnicodeText& medium) {
  return (StringCaseEquals(medium, "all") ||
          StringCaseEquals(medium, "screen"));
}

}  // namespace

bool MediaAppliesToScreen(const StringPiece& media) {
  std::vector<StringPiece> values;
  StringPieceUtils::Split(media, ",", &values);
  for (std::vector<StringPiece>::iterator iter = values.begin();
       iter < values.end(); ++iter) {
    StringPieceUtils::RemoveWhitespaceContext(&(*iter));
    if (MediumAppliesToScreen(*iter))
      return true;
  }
  return false;
}

bool MediaAppliesToScreen(const std::vector<UnicodeText>& media) {
  if (media.empty()) return true;

  for (std::vector<UnicodeText>::const_iterator iter = media.begin();
       iter < media.end(); ++iter) {
    if (MediumAppliesToScreen(*iter))
      return true;
  }
  return false;
}

bool MediaAppliesToScreen(const Css::MediaQueries& media_queries) {
  if (media_queries.empty()) return true;

  for (MediaQueries::const_iterator iter = media_queries.begin();
       iter < media_queries.end(); ++iter) {
    const Css::MediaQuery* query = *iter;
    if (query->qualifier() == Css::MediaQuery::NO_QUALIFIER &&
        MediumAppliesToScreen(query->media_type())) {
      return true;
    }
  }
  return false;
}

}  // namespace Util
}  // namespace Css
