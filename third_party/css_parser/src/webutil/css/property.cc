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

/* C++ code produced by gperf version 3.0.3 */
/* Computed positions: -k'1-2,13,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 1 "webutil/css/property.gperf"

#include "webutil/css/property.h"

#include "base/googleinit.h"
#include "base/logging.h"
#include "webutil/css/string_util.h"

namespace Css {
#line 11 "webutil/css/property.gperf"
struct props {
    const char *name;
    Property::Prop id;
};
enum
  {
    TOTAL_KEYWORDS = 179,
    MIN_WORD_LENGTH = 3,
    MAX_WORD_LENGTH = 43,
    MIN_HASH_VALUE = 17,
    MAX_HASH_VALUE = 563
  };

/* maximum key range = 547, duplicates = 0 */

#ifndef GPERF_DOWNCASE
#define GPERF_DOWNCASE 1
static unsigned char gperf_downcase[256] =
  {
      0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
     15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
     30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,
     45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
     60,  61,  62,  63,  64,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106,
    107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
    122,  91,  92,  93,  94,  95,  96,  97,  98,  99, 100, 101, 102, 103, 104,
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
    135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
    150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164,
    165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
    180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194,
    195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
    210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224,
    225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
    255
  };
#endif

#ifndef GPERF_CASE_STRNCMP
#define GPERF_CASE_STRNCMP 1
static int
gperf_case_strncmp (register const char *s1, register const char *s2, register unsigned int n)
{
  for (; n > 0;)
    {
      unsigned char c1 = gperf_downcase[(unsigned char)*s1++];
      unsigned char c2 = gperf_downcase[(unsigned char)*s2++];
      if (c1 != 0 && c1 == c2)
        {
          n--;
          continue;
        }
      return (int)c1 - (int)c2;
    }
  return 0;
}
#endif

class PropertyMapper
{
private:
  static inline unsigned int hash (const char *str, unsigned int len);
public:
  static const struct props *in_word_set (const char *str, unsigned int len);
};

inline unsigned int
PropertyMapper::hash (register const char *str, register unsigned int len)
{
  static const unsigned short asso_values[] =
    {
      564, 564, 564, 564, 564, 564, 564, 564, 564, 564,
      564, 564, 564, 564, 564, 564, 564, 564, 564, 564,
      564, 564, 564, 564, 564, 564, 564, 564, 564, 564,
      564, 564, 564, 564, 564, 564, 564, 564, 564, 564,
      564, 564,   0, 564, 564,   0, 564,   5, 564, 564,
      564, 564, 564, 564, 564, 564, 564, 564, 564, 564,
      564, 564, 564, 564, 564,  50,  25, 130, 140,   0,
      144,  60, 155, 150, 564,   0, 130,  70, 105,  10,
       35,   5,  95, 185,  15,  80,  20,   5,  10, 210,
        0, 564, 564, 564, 564, 564, 564,  50,  25, 130,
      140,   0, 144,  60, 155, 150, 564,   0, 130,  70,
      105,  10,  35,   5,  95, 185,  15,  80,  20,   5,
       10, 210,   0, 564, 564, 564, 564, 564, 564, 564,
      564, 564, 564, 564, 564, 564, 564, 564, 564, 564,
      564, 564, 564, 564, 564, 564, 564, 564, 564, 564,
      564, 564, 564, 564, 564, 564, 564, 564, 564, 564,
      564, 564, 564, 564, 564, 564, 564, 564, 564, 564,
      564, 564, 564, 564, 564, 564, 564, 564, 564, 564,
      564, 564, 564, 564, 564, 564, 564, 564, 564, 564,
      564, 564, 564, 564, 564, 564, 564, 564, 564, 564,
      564, 564, 564, 564, 564, 564, 564, 564, 564, 564,
      564, 564, 564, 564, 564, 564, 564, 564, 564, 564,
      564, 564, 564, 564, 564, 564, 564, 564, 564, 564,
      564, 564, 564, 564, 564, 564, 564, 564, 564, 564,
      564, 564, 564, 564, 564, 564, 564, 564, 564, 564,
      564, 564, 564, 564, 564, 564
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[12]];
      /*FALLTHROUGH*/
      case 12:
      case 11:
      case 10:
      case 9:
      case 8:
      case 7:
      case 6:
      case 5:
      case 4:
      case 3:
      case 2:
        hval += asso_values[(unsigned char)str[1]];
      /*FALLTHROUGH*/
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

static const struct props wordlist[] =
  {
#line 170 "webutil/css/property.gperf"
    {"z-index", Property::Z_INDEX},
#line 109 "webutil/css/property.gperf"
    {"-webkit-nbsp-mode", Property::_WEBKIT_NBSP_MODE},
#line 93 "webutil/css/property.gperf"
    {"-webkit-line-break", Property::_WEBKIT_LINE_BREAK},
#line 30 "webutil/css/property.gperf"
    {"-webkit-border-image", Property::_WEBKIT_BORDER_IMAGE},
#line 194 "webutil/css/property.gperf"
    {"/*verbatim text*/", Property::UNPARSEABLE},
#line 142 "webutil/css/property.gperf"
    {"text-overline", Property::TEXT_OVERLINE},
#line 148 "webutil/css/property.gperf"
    {"text-shadow", Property::TEXT_SHADOW},
#line 144 "webutil/css/property.gperf"
    {"text-overline-mode", Property::TEXT_OVERLINE_MODE},
#line 145 "webutil/css/property.gperf"
    {"text-overline-style", Property::TEXT_OVERLINE_STYLE},
#line 141 "webutil/css/property.gperf"
    {"text-overflow", Property::TEXT_OVERFLOW},
#line 162 "webutil/css/property.gperf"
    {"-webkit-user-select", Property::_WEBKIT_USER_SELECT},
#line 135 "webutil/css/property.gperf"
    {"text-indent", Property::TEXT_INDENT},
#line 116 "webutil/css/property.gperf"
    {"overflow", Property::OVERFLOW},
#line 156 "webutil/css/property.gperf"
    {"-webkit-text-size-adjust", Property::_WEBKIT_TEXT_SIZE_ADJUST},
#line 174 "webutil/css/property.gperf"
    {"border-style", Property::BORDER_STYLE},
#line 58 "webutil/css/property.gperf"
    {"-webkit-box-orient", Property::_WEBKIT_BOX_ORIENT},
#line 117 "webutil/css/property.gperf"
    {"overflow-x", Property::OVERFLOW_X},
#line 44 "webutil/css/property.gperf"
    {"border-right-style", Property::BORDER_RIGHT_STYLE},
#line 192 "webutil/css/property.gperf"
    {"-webkit-text-decorations-in-effect", Property::_WEBKIT_TEXT_DECORATIONS_IN_EFFECT},
#line 59 "webutil/css/property.gperf"
    {"-webkit-box-pack", Property::_WEBKIT_BOX_PACK},
#line 84 "webutil/css/property.gperf"
    {"-webkit-line-clamp", Property::_WEBKIT_LINE_CLAMP},
#line 168 "webutil/css/property.gperf"
    {"word-wrap", Property::WORD_WRAP},
#line 178 "webutil/css/property.gperf"
    {"border-left", Property::BORDER_LEFT},
#line 176 "webutil/css/property.gperf"
    {"border-right", Property::BORDER_RIGHT},
#line 158 "webutil/css/property.gperf"
    {"top", Property::TOP},
#line 43 "webutil/css/property.gperf"
    {"border-top-style", Property::BORDER_TOP_STYLE},
#line 16 "webutil/css/property.gperf"
    {"-webkit-appearance", Property::_WEBKIT_APPEARANCE},
#line 57 "webutil/css/property.gperf"
    {"-webkit-box-ordinal-group", Property::_WEBKIT_BOX_ORDINAL_GROUP},
#line 75 "webutil/css/property.gperf"
    {"-webkit-font-size-delta", Property::_WEBKIT_FONT_SIZE_DELTA},
#line 175 "webutil/css/property.gperf"
    {"border-top", Property::BORDER_TOP},
#line 160 "webutil/css/property.gperf"
    {"-webkit-user-drag", Property::_WEBKIT_USER_DRAG},
#line 29 "webutil/css/property.gperf"
    {"border-collapse", Property::BORDER_COLLAPSE},
#line 169 "webutil/css/property.gperf"
    {"word-spacing", Property::WORD_SPACING},
#line 27 "webutil/css/property.gperf"
    {"-webkit-background-size", Property::_WEBKIT_BACKGROUND_SIZE},
#line 124 "webutil/css/property.gperf"
    {"page", Property::PAGE},
#line 132 "webutil/css/property.gperf"
    {"table-layout", Property::TABLE_LAYOUT},
#line 20 "webutil/css/property.gperf"
    {"-webkit-background-composite", Property::_WEBKIT_BACKGROUND_COMPOSITE},
#line 193 "webutil/css/property.gperf"
    {"-webkit-rtl-ordering", Property::_WEBKIT_RTL_ORDERING},
#line 33 "webutil/css/property.gperf"
    {"-webkit-border-vertical-spacing", Property::_WEBKIT_BORDER_VERTICAL_SPACING},
#line 183 "webutil/css/property.gperf"
    {"outline", Property::OUTLINE},
#line 32 "webutil/css/property.gperf"
    {"-webkit-border-horizontal-spacing", Property::_WEBKIT_BORDER_HORIZONTAL_SPACING},
#line 98 "webutil/css/property.gperf"
    {"-webkit-marquee", Property::_WEBKIT_MARQUEE},
#line 155 "webutil/css/property.gperf"
    {"resize", Property::RESIZE},
#line 126 "webutil/css/property.gperf"
    {"page-break-before", Property::PAGE_BREAK_BEFORE},
#line 114 "webutil/css/property.gperf"
    {"outline-style", Property::OUTLINE_STYLE},
#line 60 "webutil/css/property.gperf"
    {"box-sizing", Property::BOX_SIZING},
#line 103 "webutil/css/property.gperf"
    {"-webkit-marquee-style", Property::_WEBKIT_MARQUEE_STYLE},
#line 26 "webutil/css/property.gperf"
    {"background-repeat", Property::BACKGROUND_REPEAT},
#line 51 "webutil/css/property.gperf"
    {"bottom", Property::BOTTOM},
#line 122 "webutil/css/property.gperf"
    {"padding-left", Property::PADDING_LEFT},
#line 24 "webutil/css/property.gperf"
    {"background-position-x", Property::BACKGROUND_POSITION_X},
#line 113 "webutil/css/property.gperf"
    {"outline-offset", Property::OUTLINE_OFFSET},
#line 18 "webutil/css/property.gperf"
    {"-webkit-background-clip", Property::_WEBKIT_BACKGROUND_CLIP},
#line 45 "webutil/css/property.gperf"
    {"border-bottom-style", Property::BORDER_BOTTOM_STYLE},
#line 100 "webutil/css/property.gperf"
    {"-webkit-marquee-increment", Property::_WEBKIT_MARQUEE_INCREMENT},
#line 17 "webutil/css/property.gperf"
    {"background-attachment", Property::BACKGROUND_ATTACHMENT},
#line 120 "webutil/css/property.gperf"
    {"padding-right", Property::PADDING_RIGHT},
#line 143 "webutil/css/property.gperf"
    {"text-overline-color", Property::TEXT_OVERLINE_COLOR},
#line 133 "webutil/css/property.gperf"
    {"text-align", Property::TEXT_ALIGN},
#line 119 "webutil/css/property.gperf"
    {"padding-top", Property::PADDING_TOP},
#line 138 "webutil/css/property.gperf"
    {"text-line-through-mode", Property::TEXT_LINE_THROUGH_MODE},
#line 139 "webutil/css/property.gperf"
    {"text-line-through-style", Property::TEXT_LINE_THROUGH_STYLE},
#line 150 "webutil/css/property.gperf"
    {"text-underline", Property::TEXT_UNDERLINE},
#line 172 "webutil/css/property.gperf"
    {"border", Property::BORDER},
#line 152 "webutil/css/property.gperf"
    {"text-underline-mode", Property::TEXT_UNDERLINE_MODE},
#line 153 "webutil/css/property.gperf"
    {"text-underline-style", Property::TEXT_UNDERLINE_STYLE},
#line 173 "webutil/css/property.gperf"
    {"border-color", Property::BORDER_COLOR},
#line 105 "webutil/css/property.gperf"
    {"max-height", Property::MAX_HEIGHT},
#line 92 "webutil/css/property.gperf"
    {"margin-left", Property::MARGIN_LEFT},
#line 90 "webutil/css/property.gperf"
    {"margin-right", Property::MARGIN_RIGHT},
#line 40 "webutil/css/property.gperf"
    {"border-right-color", Property::BORDER_RIGHT_COLOR},
#line 82 "webutil/css/property.gperf"
    {"left", Property::LEFT},
#line 184 "webutil/css/property.gperf"
    {"padding", Property::PADDING},
#line 39 "webutil/css/property.gperf"
    {"border-top-color", Property::BORDER_TOP_COLOR},
#line 128 "webutil/css/property.gperf"
    {"position", Property::POSITION},
#line 157 "webutil/css/property.gperf"
    {"-webkit-dashboard-region", Property::_WEBKIT_DASHBOARD_REGION},
#line 21 "webutil/css/property.gperf"
    {"background-image", Property::BACKGROUND_IMAGE},
#line 65 "webutil/css/property.gperf"
    {"content", Property::CONTENT},
#line 74 "webutil/css/property.gperf"
    {"font-size", Property::FONT_SIZE},
#line 77 "webutil/css/property.gperf"
    {"font-style", Property::FONT_STYLE},
#line 89 "webutil/css/property.gperf"
    {"margin-top", Property::MARGIN_TOP},
#line 81 "webutil/css/property.gperf"
    {"-webkit-highlight", Property::_WEBKIT_HIGHLIGHT},
#line 165 "webutil/css/property.gperf"
    {"white-space", Property::WHITE_SPACE},
#line 66 "webutil/css/property.gperf"
    {"counter-increment", Property::COUNTER_INCREMENT},
#line 180 "webutil/css/property.gperf"
    {"font", Property::FONT},
#line 54 "webutil/css/property.gperf"
    {"-webkit-box-flex", Property::_WEBKIT_BOX_FLEX},
#line 80 "webutil/css/property.gperf"
    {"height", Property::HEIGHT},
#line 52 "webutil/css/property.gperf"
    {"-webkit-box-align", Property::_WEBKIT_BOX_ALIGN},
#line 94 "webutil/css/property.gperf"
    {"-webkit-margin-collapse", Property::_WEBKIT_MARGIN_COLLAPSE},
#line 121 "webutil/css/property.gperf"
    {"padding-bottom", Property::PADDING_BOTTOM},
#line 79 "webutil/css/property.gperf"
    {"font-weight", Property::FONT_WEIGHT},
#line 78 "webutil/css/property.gperf"
    {"font-variant", Property::FONT_VARIANT},
#line 95 "webutil/css/property.gperf"
    {"-webkit-margin-top-collapse", Property::_WEBKIT_MARGIN_TOP_COLLAPSE},
#line 67 "webutil/css/property.gperf"
    {"counter-reset", Property::COUNTER_RESET},
#line 96 "webutil/css/property.gperf"
    {"-webkit-margin-bottom-collapse", Property::_WEBKIT_MARGIN_BOTTOM_COLLAPSE},
#line 177 "webutil/css/property.gperf"
    {"border-bottom", Property::BORDER_BOTTOM},
#line 146 "webutil/css/property.gperf"
    {"text-overline-width", Property::TEXT_OVERLINE_WIDTH},
#line 97 "webutil/css/property.gperf"
    {"-webkit-margin-start", Property::_WEBKIT_MARGIN_START},
#line 123 "webutil/css/property.gperf"
    {"-webkit-padding-start", Property::_WEBKIT_PADDING_START},
#line 61 "webutil/css/property.gperf"
    {"caption-side", Property::CAPTION_SIDE},
#line 149 "webutil/css/property.gperf"
    {"text-transform", Property::TEXT_TRANSFORM},
#line 22 "webutil/css/property.gperf"
    {"-webkit-background-origin", Property::_WEBKIT_BACKGROUND_ORIGIN},
#line 19 "webutil/css/property.gperf"
    {"background-color", Property::BACKGROUND_COLOR},
#line 163 "webutil/css/property.gperf"
    {"vertical-align", Property::VERTICAL_ALIGN},
#line 179 "webutil/css/property.gperf"
    {"border-width", Property::BORDER_WIDTH},
#line 55 "webutil/css/property.gperf"
    {"-webkit-box-flex-group", Property::_WEBKIT_BOX_FLEX_GROUP},
#line 127 "webutil/css/property.gperf"
    {"page-break-inside", Property::PAGE_BREAK_INSIDE},
#line 48 "webutil/css/property.gperf"
    {"border-right-width", Property::BORDER_RIGHT_WIDTH},
#line 23 "webutil/css/property.gperf"
    {"background-position", Property::BACKGROUND_POSITION},
#line 34 "webutil/css/property.gperf"
    {"-webkit-border-radius", Property::_WEBKIT_BORDER_RADIUS},
#line 50 "webutil/css/property.gperf"
    {"border-left-width", Property::BORDER_LEFT_WIDTH},
#line 31 "webutil/css/property.gperf"
    {"border-spacing", Property::BORDER_SPACING},
#line 99 "webutil/css/property.gperf"
    {"-webkit-marquee-direction", Property::_WEBKIT_MARQUEE_DIRECTION},
#line 101 "webutil/css/property.gperf"
    {"-webkit-marquee-repetition", Property::_WEBKIT_MARQUEE_REPETITION},
#line 41 "webutil/css/property.gperf"
    {"border-bottom-color", Property::BORDER_BOTTOM_COLOR},
#line 35 "webutil/css/property.gperf"
    {"-webkit-border-top-left-radius", Property::_WEBKIT_BORDER_TOP_LEFT_RADIUS},
#line 36 "webutil/css/property.gperf"
    {"-webkit-border-top-right-radius", Property::_WEBKIT_BORDER_TOP_RIGHT_RADIUS},
#line 37 "webutil/css/property.gperf"
    {"-webkit-border-bottom-left-radius", Property::_WEBKIT_BORDER_BOTTOM_LEFT_RADIUS},
#line 38 "webutil/css/property.gperf"
    {"-webkit-border-bottom-right-radius", Property::_WEBKIT_BORDER_BOTTOM_RIGHT_RADIUS},
#line 171 "webutil/css/property.gperf"
    {"background", Property::BACKGROUND},
#line 137 "webutil/css/property.gperf"
    {"text-line-through-color", Property::TEXT_LINE_THROUGH_COLOR},
#line 28 "webutil/css/property.gperf"
    {"-webkit-binding", Property::_WEBKIT_BINDING},
#line 182 "webutil/css/property.gperf"
    {"margin", Property::MARGIN},
#line 161 "webutil/css/property.gperf"
    {"-webkit-user-modify", Property::_WEBKIT_USER_MODIFY},
#line 151 "webutil/css/property.gperf"
    {"text-underline-color", Property::TEXT_UNDERLINE_COLOR},
#line 147 "webutil/css/property.gperf"
    {"-webkit-text-security", Property::_WEBKIT_TEXT_SECURITY},
#line 46 "webutil/css/property.gperf"
    {"border-left-style", Property::BORDER_LEFT_STYLE},
#line 64 "webutil/css/property.gperf"
    {"color", Property::COLOR},
#line 107 "webutil/css/property.gperf"
    {"min-height", Property::MIN_HEIGHT},
#line 102 "webutil/css/property.gperf"
    {"-webkit-marquee-speed", Property::_WEBKIT_MARQUEE_SPEED},
#line 118 "webutil/css/property.gperf"
    {"overflow-y", Property::OVERFLOW_Y},
#line 110 "webutil/css/property.gperf"
    {"opacity", Property::OPACITY},
#line 130 "webutil/css/property.gperf"
    {"right", Property::RIGHT},
#line 71 "webutil/css/property.gperf"
    {"empty-cells", Property::EMPTY_CELLS},
#line 53 "webutil/css/property.gperf"
    {"-webkit-box-direction", Property::_WEBKIT_BOX_DIRECTION},
#line 91 "webutil/css/property.gperf"
    {"margin-bottom", Property::MARGIN_BOTTOM},
#line 129 "webutil/css/property.gperf"
    {"quotes", Property::QUOTES},
#line 42 "webutil/css/property.gperf"
    {"border-left-color", Property::BORDER_LEFT_COLOR},
#line 49 "webutil/css/property.gperf"
    {"border-bottom-width", Property::BORDER_BOTTOM_WIDTH},
#line 136 "webutil/css/property.gperf"
    {"text-line-through", Property::TEXT_LINE_THROUGH},
#line 106 "webutil/css/property.gperf"
    {"max-width", Property::MAX_WIDTH},
#line 134 "webutil/css/property.gperf"
    {"text-decoration", Property::TEXT_DECORATION},
#line 140 "webutil/css/property.gperf"
    {"text-line-through-width", Property::TEXT_LINE_THROUGH_WIDTH},
#line 181 "webutil/css/property.gperf"
    {"list-style", Property::LIST_STYLE},
#line 112 "webutil/css/property.gperf"
    {"outline-color", Property::OUTLINE_COLOR},
#line 72 "webutil/css/property.gperf"
    {"float", Property::FLOAT},
#line 154 "webutil/css/property.gperf"
    {"text-underline-width", Property::TEXT_UNDERLINE_WIDTH},
#line 111 "webutil/css/property.gperf"
    {"orphans", Property::ORPHANS},
#line 104 "webutil/css/property.gperf"
    {"-webkit-match-nearest-mail-blockquote-color", Property::_WEBKIT_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR},
#line 63 "webutil/css/property.gperf"
    {"clip", Property::CLIP},
#line 85 "webutil/css/property.gperf"
    {"line-height", Property::LINE_HEIGHT},
#line 83 "webutil/css/property.gperf"
    {"letter-spacing", Property::LETTER_SPACING},
#line 68 "webutil/css/property.gperf"
    {"cursor", Property::CURSOR},
#line 167 "webutil/css/property.gperf"
    {"width", Property::WIDTH},
#line 25 "webutil/css/property.gperf"
    {"background-position-y", Property::BACKGROUND_POSITION_Y},
#line 76 "webutil/css/property.gperf"
    {"font-stretch", Property::FONT_STRETCH},
#line 56 "webutil/css/property.gperf"
    {"-webkit-box-lines", Property::_WEBKIT_BOX_LINES},
#line 131 "webutil/css/property.gperf"
    {"size", Property::SIZE},
#line 125 "webutil/css/property.gperf"
    {"page-break-after", Property::PAGE_BREAK_AFTER},
#line 166 "webutil/css/property.gperf"
    {"widows", Property::WIDOWS},
#line 159 "webutil/css/property.gperf"
    {"unicode-bidi", Property::UNICODE_BIDI},
#line 47 "webutil/css/property.gperf"
    {"border-top-width", Property::BORDER_TOP_WIDTH},
#line 62 "webutil/css/property.gperf"
    {"clear", Property::CLEAR},
#line 86 "webutil/css/property.gperf"
    {"list-style-image", Property::LIST_STYLE_IMAGE},
#line 73 "webutil/css/property.gperf"
    {"font-family", Property::FONT_FAMILY},
#line 108 "webutil/css/property.gperf"
    {"min-width", Property::MIN_WIDTH},
#line 164 "webutil/css/property.gperf"
    {"visibility", Property::VISIBILITY},
#line 69 "webutil/css/property.gperf"
    {"direction", Property::DIRECTION},
#line 115 "webutil/css/property.gperf"
    {"outline-width", Property::OUTLINE_WIDTH},
#line 87 "webutil/css/property.gperf"
    {"list-style-position", Property::LIST_STYLE_POSITION},
#line 190 "webutil/css/property.gperf"
    {"scrollbar-track-color", Property::SCROLLBAR_TRACK_COLOR},
#line 186 "webutil/css/property.gperf"
    {"scrollbar-shadow-color", Property::SCROLLBAR_SHADOW_COLOR},
#line 187 "webutil/css/property.gperf"
    {"scrollbar-highlight-color", Property::SCROLLBAR_HIGHLIGHT_COLOR},
#line 88 "webutil/css/property.gperf"
    {"list-style-type", Property::LIST_STYLE_TYPE},
#line 70 "webutil/css/property.gperf"
    {"display", Property::DISPLAY},
#line 191 "webutil/css/property.gperf"
    {"scrollbar-arrow-color", Property::SCROLLBAR_ARROW_COLOR},
#line 189 "webutil/css/property.gperf"
    {"scrollbar-darkshadow-color", Property::SCROLLBAR_DARKSHADOW_COLOR},
#line 185 "webutil/css/property.gperf"
    {"scrollbar-face-color", Property::SCROLLBAR_FACE_COLOR},
#line 188 "webutil/css/property.gperf"
    {"scrollbar-3dlight-color", Property::SCROLLBAR_3DLIGHT_COLOR}
  };

const struct props *
PropertyMapper::in_word_set (register const char *str, register unsigned int len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= MIN_HASH_VALUE)
        {
          register const struct props *resword;

          switch (key - 17)
            {
              case 0:
                resword = &wordlist[0];
                goto compare;
              case 5:
                resword = &wordlist[1];
                goto compare;
              case 6:
                resword = &wordlist[2];
                goto compare;
              case 8:
                resword = &wordlist[3];
                goto compare;
              case 10:
                resword = &wordlist[4];
                goto compare;
              case 11:
                resword = &wordlist[5];
                goto compare;
              case 14:
                resword = &wordlist[6];
                goto compare;
              case 16:
                resword = &wordlist[7];
                goto compare;
              case 17:
                resword = &wordlist[8];
                goto compare;
              case 21:
                resword = &wordlist[9];
                goto compare;
              case 22:
                resword = &wordlist[10];
                goto compare;
              case 24:
                resword = &wordlist[11];
                goto compare;
              case 26:
                resword = &wordlist[12];
                goto compare;
              case 27:
                resword = &wordlist[13];
                goto compare;
              case 30:
                resword = &wordlist[14];
                goto compare;
              case 31:
                resword = &wordlist[15];
                goto compare;
              case 33:
                resword = &wordlist[16];
                goto compare;
              case 36:
                resword = &wordlist[17];
                goto compare;
              case 37:
                resword = &wordlist[18];
                goto compare;
              case 39:
                resword = &wordlist[19];
                goto compare;
              case 41:
                resword = &wordlist[20];
                goto compare;
              case 42:
                resword = &wordlist[21];
                goto compare;
              case 44:
                resword = &wordlist[22];
                goto compare;
              case 45:
                resword = &wordlist[23];
                goto compare;
              case 46:
                resword = &wordlist[24];
                goto compare;
              case 49:
                resword = &wordlist[25];
                goto compare;
              case 56:
                resword = &wordlist[26];
                goto compare;
              case 58:
                resword = &wordlist[27];
                goto compare;
              case 61:
                resword = &wordlist[28];
                goto compare;
              case 63:
                resword = &wordlist[29];
                goto compare;
              case 65:
                resword = &wordlist[30];
                goto compare;
              case 68:
                resword = &wordlist[31];
                goto compare;
              case 70:
                resword = &wordlist[32];
                goto compare;
              case 71:
                resword = &wordlist[33];
                goto compare;
              case 72:
                resword = &wordlist[34];
                goto compare;
              case 75:
                resword = &wordlist[35];
                goto compare;
              case 76:
                resword = &wordlist[36];
                goto compare;
              case 78:
                resword = &wordlist[37];
                goto compare;
              case 79:
                resword = &wordlist[38];
                goto compare;
              case 80:
                resword = &wordlist[39];
                goto compare;
              case 81:
                resword = &wordlist[40];
                goto compare;
              case 83:
                resword = &wordlist[41];
                goto compare;
              case 84:
                resword = &wordlist[42];
                goto compare;
              case 85:
                resword = &wordlist[43];
                goto compare;
              case 86:
                resword = &wordlist[44];
                goto compare;
              case 88:
                resword = &wordlist[45];
                goto compare;
              case 89:
                resword = &wordlist[46];
                goto compare;
              case 90:
                resword = &wordlist[47];
                goto compare;
              case 94:
                resword = &wordlist[48];
                goto compare;
              case 95:
                resword = &wordlist[49];
                goto compare;
              case 99:
                resword = &wordlist[50];
                goto compare;
              case 102:
                resword = &wordlist[51];
                goto compare;
              case 106:
                resword = &wordlist[52];
                goto compare;
              case 107:
                resword = &wordlist[53];
                goto compare;
              case 108:
                resword = &wordlist[54];
                goto compare;
              case 109:
                resword = &wordlist[55];
                goto compare;
              case 111:
                resword = &wordlist[56];
                goto compare;
              case 112:
                resword = &wordlist[57];
                goto compare;
              case 113:
                resword = &wordlist[58];
                goto compare;
              case 114:
                resword = &wordlist[59];
                goto compare;
              case 115:
                resword = &wordlist[60];
                goto compare;
              case 116:
                resword = &wordlist[61];
                goto compare;
              case 117:
                resword = &wordlist[62];
                goto compare;
              case 119:
                resword = &wordlist[63];
                goto compare;
              case 122:
                resword = &wordlist[64];
                goto compare;
              case 123:
                resword = &wordlist[65];
                goto compare;
              case 125:
                resword = &wordlist[66];
                goto compare;
              case 128:
                resword = &wordlist[67];
                goto compare;
              case 129:
                resword = &wordlist[68];
                goto compare;
              case 130:
                resword = &wordlist[69];
                goto compare;
              case 131:
                resword = &wordlist[70];
                goto compare;
              case 132:
                resword = &wordlist[71];
                goto compare;
              case 135:
                resword = &wordlist[72];
                goto compare;
              case 139:
                resword = &wordlist[73];
                goto compare;
              case 141:
                resword = &wordlist[74];
                goto compare;
              case 142:
                resword = &wordlist[75];
                goto compare;
              case 144:
                resword = &wordlist[76];
                goto compare;
              case 145:
                resword = &wordlist[77];
                goto compare;
              case 146:
                resword = &wordlist[78];
                goto compare;
              case 147:
                resword = &wordlist[79];
                goto compare;
              case 148:
                resword = &wordlist[80];
                goto compare;
              case 150:
                resword = &wordlist[81];
                goto compare;
              case 154:
                resword = &wordlist[82];
                goto compare;
              case 155:
                resword = &wordlist[83];
                goto compare;
              case 156:
                resword = &wordlist[84];
                goto compare;
              case 158:
                resword = &wordlist[85];
                goto compare;
              case 159:
                resword = &wordlist[86];
                goto compare;
              case 160:
                resword = &wordlist[87];
                goto compare;
              case 161:
                resword = &wordlist[88];
                goto compare;
              case 162:
                resword = &wordlist[89];
                goto compare;
              case 163:
                resword = &wordlist[90];
                goto compare;
              case 164:
                resword = &wordlist[91];
                goto compare;
              case 165:
                resword = &wordlist[92];
                goto compare;
              case 166:
                resword = &wordlist[93];
                goto compare;
              case 168:
                resword = &wordlist[94];
                goto compare;
              case 171:
                resword = &wordlist[95];
                goto compare;
              case 172:
                resword = &wordlist[96];
                goto compare;
              case 173:
                resword = &wordlist[97];
                goto compare;
              case 174:
                resword = &wordlist[98];
                goto compare;
              case 175:
                resword = &wordlist[99];
                goto compare;
              case 177:
                resword = &wordlist[100];
                goto compare;
              case 178:
                resword = &wordlist[101];
                goto compare;
              case 179:
                resword = &wordlist[102];
                goto compare;
              case 182:
                resword = &wordlist[103];
                goto compare;
              case 185:
                resword = &wordlist[104];
                goto compare;
              case 189:
                resword = &wordlist[105];
                goto compare;
              case 190:
                resword = &wordlist[106];
                goto compare;
              case 191:
                resword = &wordlist[107];
                goto compare;
              case 192:
                resword = &wordlist[108];
                goto compare;
              case 194:
                resword = &wordlist[109];
                goto compare;
              case 195:
                resword = &wordlist[110];
                goto compare;
              case 197:
                resword = &wordlist[111];
                goto compare;
              case 198:
                resword = &wordlist[112];
                goto compare;
              case 199:
                resword = &wordlist[113];
                goto compare;
              case 202:
                resword = &wordlist[114];
                goto compare;
              case 203:
                resword = &wordlist[115];
                goto compare;
              case 204:
                resword = &wordlist[116];
                goto compare;
              case 206:
                resword = &wordlist[117];
                goto compare;
              case 207:
                resword = &wordlist[118];
                goto compare;
              case 208:
                resword = &wordlist[119];
                goto compare;
              case 211:
                resword = &wordlist[120];
                goto compare;
              case 213:
                resword = &wordlist[121];
                goto compare;
              case 214:
                resword = &wordlist[122];
                goto compare;
              case 217:
                resword = &wordlist[123];
                goto compare;
              case 218:
                resword = &wordlist[124];
                goto compare;
              case 219:
                resword = &wordlist[125];
                goto compare;
              case 220:
                resword = &wordlist[126];
                goto compare;
              case 223:
                resword = &wordlist[127];
                goto compare;
              case 228:
                resword = &wordlist[128];
                goto compare;
              case 229:
                resword = &wordlist[129];
                goto compare;
              case 233:
                resword = &wordlist[130];
                goto compare;
              case 245:
                resword = &wordlist[131];
                goto compare;
              case 248:
                resword = &wordlist[132];
                goto compare;
              case 249:
                resword = &wordlist[133];
                goto compare;
              case 254:
                resword = &wordlist[134];
                goto compare;
              case 256:
                resword = &wordlist[135];
                goto compare;
              case 259:
                resword = &wordlist[136];
                goto compare;
              case 260:
                resword = &wordlist[137];
                goto compare;
              case 262:
                resword = &wordlist[138];
                goto compare;
              case 265:
                resword = &wordlist[139];
                goto compare;
              case 267:
                resword = &wordlist[140];
                goto compare;
              case 268:
                resword = &wordlist[141];
                goto compare;
              case 271:
                resword = &wordlist[142];
                goto compare;
              case 273:
                resword = &wordlist[143];
                goto compare;
              case 276:
                resword = &wordlist[144];
                goto compare;
              case 277:
                resword = &wordlist[145];
                goto compare;
              case 278:
                resword = &wordlist[146];
                goto compare;
              case 280:
                resword = &wordlist[147];
                goto compare;
              case 281:
                resword = &wordlist[148];
                goto compare;
              case 282:
                resword = &wordlist[149];
                goto compare;
              case 289:
                resword = &wordlist[150];
                goto compare;
              case 292:
                resword = &wordlist[151];
                goto compare;
              case 294:
                resword = &wordlist[152];
                goto compare;
              case 298:
                resword = &wordlist[153];
                goto compare;
              case 299:
                resword = &wordlist[154];
                goto compare;
              case 304:
                resword = &wordlist[155];
                goto compare;
              case 320:
                resword = &wordlist[156];
                goto compare;
              case 322:
                resword = &wordlist[157];
                goto compare;
              case 323:
                resword = &wordlist[158];
                goto compare;
              case 329:
                resword = &wordlist[159];
                goto compare;
              case 330:
                resword = &wordlist[160];
                goto compare;
              case 339:
                resword = &wordlist[161];
                goto compare;
              case 343:
                resword = &wordlist[162];
                goto compare;
              case 349:
                resword = &wordlist[163];
                goto compare;
              case 358:
                resword = &wordlist[164];
                goto compare;
              case 367:
                resword = &wordlist[165];
                goto compare;
              case 373:
                resword = &wordlist[166];
                goto compare;
              case 387:
                resword = &wordlist[167];
                goto compare;
              case 396:
                resword = &wordlist[168];
                goto compare;
              case 397:
                resword = &wordlist[169];
                goto compare;
              case 464:
                resword = &wordlist[170];
                goto compare;
              case 465:
                resword = &wordlist[171];
                goto compare;
              case 478:
                resword = &wordlist[172];
                goto compare;
              case 488:
                resword = &wordlist[173];
                goto compare;
              case 490:
                resword = &wordlist[174];
                goto compare;
              case 509:
                resword = &wordlist[175];
                goto compare;
              case 514:
                resword = &wordlist[176];
                goto compare;
              case 543:
                resword = &wordlist[177];
                goto compare;
              case 546:
                resword = &wordlist[178];
                goto compare;
            }
          return 0;
        compare:
          {
            register const char *s = resword->name;

            if ((((unsigned char)*str ^ (unsigned char)*s) & ~32) == 0 && !gperf_case_strncmp (str, s, len) && s[len] == '\0')
              return resword;
          }
        }
    }
  return 0;
}
#line 195 "webutil/css/property.gperf"


//
// Constructor.
//

Property::Property(UnicodeText s)
  : prop_(PropFromText(s.utf8_data(), s.utf8_length()))
{
    if (prop_ == OTHER)
      other_ = LowercaseAscii(s);
}

//
// Static methods mapping Prop's to strings
//

Property::Prop Property::PropFromText(const char* str, int len) {
  const props* a = PropertyMapper::in_word_set(str, len);
  if (a)
    return a->id;
  else
    return OTHER;
}

static const char* name_lookup[TOTAL_KEYWORDS];

static void InitializeNameLookupTable() {
  for (int i = 0; i < TOTAL_KEYWORDS; ++i)
    name_lookup[wordlist[i].id] = wordlist[i].name;
}

const char* Property::TextFromProp(Prop p) {
  if (p == OTHER) {
    return "OTHER";
  } else {
    DCHECK_LT(p, OTHER);
    return name_lookup[p];
  }
}

} // namespace

REGISTER_MODULE_INITIALIZER(property, {
  Css::InitializeNameLookupTable();
});
