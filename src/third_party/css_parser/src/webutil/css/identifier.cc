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
/* Computed positions: -k'1-3,10,$' */

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

#line 1 "webutil/css/identifier.gperf"

#include "webutil/css/identifier.h"

#include "base/googleinit.h"
#include "base/logging.h"
#include "webutil/css/string_util.h"

namespace Css {
#line 11 "webutil/css/identifier.gperf"
struct idents {
    const char *name;
    Identifier::Ident id;
};
enum
  {
    TOTAL_KEYWORDS = 144,
    MIN_WORD_LENGTH = 3,
    MAX_WORD_LENGTH = 24,
    MIN_HASH_VALUE = 5,
    MAX_HASH_VALUE = 401
  };

/* maximum key range = 397, duplicates = 0 */

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

class IdentifierMapper
{
private:
  static inline unsigned int hash (const char *str, unsigned int len);
public:
  static const struct idents *in_word_set (const char *str, unsigned int len);
};

inline unsigned int
IdentifierMapper::hash (register const char *str, register unsigned int len)
{
  static const unsigned short asso_values[] =
    {
      402, 402, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402,  60, 125, 402, 402, 402,
      402, 402, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402,  30,   0,  50,   5,   0,
       25,  30, 145,  65, 110,  50,  10, 140,  45,  75,
       15, 155,  20,   5,   0,  45, 110,  20,  75,  95,
      402, 402, 402, 402, 402, 402, 402,  30,   0,  50,
        5,   0,  25,  30, 145,  65, 110,  50,  10, 140,
       45,  75,  15, 155,  20,   5,   0,  45, 110,  20,
       75,  95, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402, 402, 402, 402, 402, 402,
      402, 402, 402, 402, 402, 402, 402
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[9]];
      /*FALLTHROUGH*/
      case 9:
      case 8:
      case 7:
      case 6:
      case 5:
      case 4:
      case 3:
        hval += asso_values[(unsigned char)str[2]];
      /*FALLTHROUGH*/
      case 2:
        hval += asso_values[(unsigned char)str[1]+1];
      /*FALLTHROUGH*/
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

static const struct idents wordlist[] =
  {
#line 78 "webutil/css/identifier.gperf"
    {"table", Identifier::TABLE},
#line 145 "webutil/css/identifier.gperf"
    {"baseline", Identifier::BASELINE},
#line 42 "webutil/css/identifier.gperf"
    {"dashed", Identifier::DASHED},
#line 151 "webutil/css/identifier.gperf"
    {"pre", Identifier::PRE},
#line 154 "webutil/css/identifier.gperf"
    {"pre-line", Identifier::PRE_LINE},
#line 83 "webutil/css/identifier.gperf"
    {"table-row", Identifier::TABLE_ROW},
#line 86 "webutil/css/identifier.gperf"
    {"table-cell", Identifier::TABLE_CELL},
#line 41 "webutil/css/identifier.gperf"
    {"dotted", Identifier::DOTTED},
#line 82 "webutil/css/identifier.gperf"
    {"table-footer-group", Identifier::TABLE_FOOTER_GROUP},
#line 106 "webutil/css/identifier.gperf"
    {"bold", Identifier::BOLD},
#line 98 "webutil/css/identifier.gperf"
    {"large", Identifier::LARGE},
#line 81 "webutil/css/identifier.gperf"
    {"table-header-group", Identifier::TABLE_HEADER_GROUP},
#line 43 "webutil/css/identifier.gperf"
    {"solid", Identifier::SOLID},
#line 153 "webutil/css/identifier.gperf"
    {"pre-wrap", Identifier::PRE_WRAP},
#line 34 "webutil/css/identifier.gperf"
    {"scroll", Identifier::SCROLL},
#line 31 "webutil/css/identifier.gperf"
    {"top", Identifier::TOP},
#line 107 "webutil/css/identifier.gperf"
    {"bolder", Identifier::BOLDER},
#line 40 "webutil/css/identifier.gperf"
    {"separate", Identifier::SEPARATE},
#line 142 "webutil/css/identifier.gperf"
    {"lowercase", Identifier::LOWERCASE},
#line 143 "webutil/css/identifier.gperf"
    {"embed", Identifier::EMBED},
#line 102 "webutil/css/identifier.gperf"
    {"larger", Identifier::LARGER},
#line 87 "webutil/css/identifier.gperf"
    {"table-caption", Identifier::TABLE_CAPTION},
#line 56 "webutil/css/identifier.gperf"
    {"default", Identifier::DEFAULT},
#line 133 "webutil/css/identifier.gperf"
    {"relative", Identifier::RELATIVE},
#line 28 "webutil/css/identifier.gperf"
    {"left", Identifier::LEFT},
#line 26 "webutil/css/identifier.gperf"
    {"repeat", Identifier::REPEAT},
#line 44 "webutil/css/identifier.gperf"
    {"double", Identifier::DOUBLE},
#line 140 "webutil/css/identifier.gperf"
    {"capitalize", Identifier::CAPITALIZE},
#line 119 "webutil/css/identifier.gperf"
    {"square", Identifier::SQUARE},
#line 84 "webutil/css/identifier.gperf"
    {"table-column-group", Identifier::TABLE_COLUMN_GROUP},
#line 90 "webutil/css/identifier.gperf"
    {"serif", Identifier::SERIF},
#line 27 "webutil/css/identifier.gperf"
    {"collapse", Identifier::COLLAPSE},
#line 72 "webutil/css/identifier.gperf"
    {"rtl", Identifier::RTL,},
#line 68 "webutil/css/identifier.gperf"
    {"wait", Identifier::WAIT},
#line 80 "webutil/css/identifier.gperf"
    {"table-row-group", Identifier::TABLE_ROW_GROUP},
#line 36 "webutil/css/identifier.gperf"
    {"transparent", Identifier::TRANSPARENT},
#line 134 "webutil/css/identifier.gperf"
    {"absolute", Identifier::ABSOLUTE},
#line 97 "webutil/css/identifier.gperf"
    {"small", Identifier::SMALL},
#line 20 "webutil/css/identifier.gperf"
    {"normal", Identifier::NORMAL},
#line 120 "webutil/css/identifier.gperf"
    {"decimal", Identifier::DECIMAL},
#line 71 "webutil/css/identifier.gperf"
    {"ltr", Identifier::LTR,},
#line 63 "webutil/css/identifier.gperf"
    {"se-resize", Identifier::SE_RESIZE},
#line 105 "webutil/css/identifier.gperf"
    {"small-caps", Identifier::SMALL_CAPS},
#line 152 "webutil/css/identifier.gperf"
    {"nowrap", Identifier::NOWRAP},
#line 85 "webutil/css/identifier.gperf"
    {"table-column", Identifier::TABLE_COLUMN},
#line 137 "webutil/css/identifier.gperf"
    {"overline", Identifier::OVERLINE},
#line 67 "webutil/css/identifier.gperf"
    {"text", Identifier::TEXT},
#line 124 "webutil/css/identifier.gperf"
    {"lower-greek", Identifier::LOWER_GREEK},
#line 101 "webutil/css/identifier.gperf"
    {"smaller", Identifier::SMALLER},
#line 70 "webutil/css/identifier.gperf"
    {"progress", Identifier::PROGRESS},
#line 18 "webutil/css/identifier.gperf"
    {"none", Identifier::NONE},
#line 91 "webutil/css/identifier.gperf"
    {"sans-serif", Identifier::SANS_SERIF},
#line 45 "webutil/css/identifier.gperf"
    {"groove", Identifier::GROOVE},
#line 109 "webutil/css/identifier.gperf"
    {"caption", Identifier::CAPTION},
#line 146 "webutil/css/identifier.gperf"
    {"sub", Identifier::SUB},
#line 57 "webutil/css/identifier.gperf"
    {"pointer", Identifier::POINTER},
#line 148 "webutil/css/identifier.gperf"
    {"text-top", Identifier::TEXT_TOP},
#line 39 "webutil/css/identifier.gperf"
    {"no-repeat", Identifier::NO_REPEAT},
#line 114 "webutil/css/identifier.gperf"
    {"status-bar", Identifier::STATUS_BAR},
#line 122 "webutil/css/identifier.gperf"
    {"lower-roman", Identifier::LOWER_ROMAN},
#line 136 "webutil/css/identifier.gperf"
    {"underline", Identifier::UNDERLINE},
#line 24 "webutil/css/identifier.gperf"
    {"avoid", Identifier::AVOID},
#line 132 "webutil/css/identifier.gperf"
    {"static", Identifier::STATIC},
#line 113 "webutil/css/identifier.gperf"
    {"small-caption", Identifier::SMALL_CAPTION},
#line 60 "webutil/css/identifier.gperf"
    {"ne-resize", Identifier::NE_RESIZE},
#line 46 "webutil/css/identifier.gperf"
    {"ridge", Identifier::RIDGE},
#line 104 "webutil/css/identifier.gperf"
    {"oblique", Identifier::OBLIQUE},
#line 37 "webutil/css/identifier.gperf"
    {"repeat-x", Identifier::REPEAT_X},
#line 29 "webutil/css/identifier.gperf"
    {"center", Identifier::CENTER},
#line 144 "webutil/css/identifier.gperf"
    {"bidi-override", Identifier::BIDI_OVERRIDE},
#line 64 "webutil/css/identifier.gperf"
    {"sw-resize", Identifier::SW_RESIZE},
#line 47 "webutil/css/identifier.gperf"
    {"inset", Identifier::INSET},
#line 115 "webutil/css/identifier.gperf"
    {"inside", Identifier::INSIDE},
#line 59 "webutil/css/identifier.gperf"
    {"e-resize", Identifier::E_RESIZE},
#line 147 "webutil/css/identifier.gperf"
    {"super", Identifier::SUPER},
#line 73 "webutil/css/identifier.gperf"
    {"inline", Identifier::INLINE},
#line 65 "webutil/css/identifier.gperf"
    {"s-resize", Identifier::S_RESIZE},
#line 55 "webutil/css/identifier.gperf"
    {"crosshair", Identifier::CROSSHAIR},
#line 32 "webutil/css/identifier.gperf"
    {"bottom", Identifier::BOTTOM},
#line 79 "webutil/css/identifier.gperf"
    {"inline-table", Identifier::INLINE_TABLE},
#line 38 "webutil/css/identifier.gperf"
    {"repeat-y", Identifier::REPEAT_Y},
#line 33 "webutil/css/identifier.gperf"
    {"both", Identifier::BOTH},
#line 30 "webutil/css/identifier.gperf"
    {"right", Identifier::RIGHT},
#line 125 "webutil/css/identifier.gperf"
    {"lower-latin", Identifier::LOWER_LATIN,},
#line 88 "webutil/css/identifier.gperf"
    {"show", Identifier::SHOW},
#line 93 "webutil/css/identifier.gperf"
    {"fantasy", Identifier::FANTASY},
#line 66 "webutil/css/identifier.gperf"
    {"w-resize", Identifier::W_RESIZE},
#line 117 "webutil/css/identifier.gperf"
    {"disc", Identifier::DISC},
#line 121 "webutil/css/identifier.gperf"
    {"decimal-leading-zero", Identifier::DECIMAL_LEADING_ZERO},
#line 108 "webutil/css/identifier.gperf"
    {"lighter", Identifier::LIGHTER},
#line 53 "webutil/css/identifier.gperf"
    {"no-open-quote", Identifier::NO_OPEN_QUOTE},
#line 49 "webutil/css/identifier.gperf"
    {"thin", Identifier::THIN},
#line 128 "webutil/css/identifier.gperf"
    {"georgian", Identifier::GEORGIAN},
#line 50 "webutil/css/identifier.gperf"
    {"thick", Identifier::THICK},
#line 118 "webutil/css/identifier.gperf"
    {"circle", Identifier::CIRCLE},
#line 92 "webutil/css/identifier.gperf"
    {"cursive", Identifier::CURSIVE},
#line 61 "webutil/css/identifier.gperf"
    {"nw-resize", Identifier::NW_RESIZE},
#line 48 "webutil/css/identifier.gperf"
    {"outset", Identifier::OUTSET},
#line 116 "webutil/css/identifier.gperf"
    {"outside", Identifier::OUTSIDE},
#line 110 "webutil/css/identifier.gperf"
    {"icon", Identifier::ICON},
#line 103 "webutil/css/identifier.gperf"
    {"italic", Identifier::ITALIC},
#line 62 "webutil/css/identifier.gperf"
    {"n-resize", Identifier::N_RESIZE},
#line 69 "webutil/css/identifier.gperf"
    {"help", Identifier::HELP},
#line 23 "webutil/css/identifier.gperf"
    {"always", Identifier::ALWAYS},
#line 94 "webutil/css/identifier.gperf"
    {"monospace", Identifier::MONOSPACE},
#line 99 "webutil/css/identifier.gperf"
    {"x-large", Identifier::X_LARGE},
#line 19 "webutil/css/identifier.gperf"
    {"auto", Identifier::AUTO},
#line 35 "webutil/css/identifier.gperf"
    {"fixed", Identifier::FIXED},
#line 96 "webutil/css/identifier.gperf"
    {"x-small", Identifier::X_SMALL},
#line 141 "webutil/css/identifier.gperf"
    {"uppercase", Identifier::UPPERCASE},
#line 76 "webutil/css/identifier.gperf"
    {"run-in", Identifier::RUN_IN},
#line 127 "webutil/css/identifier.gperf"
    {"armenian", Identifier::ARMENIAN},
#line 129 "webutil/css/identifier.gperf"
    {"lower-alpha", Identifier::LOWER_ALPHA},
#line 21 "webutil/css/identifier.gperf"
    {"visible", Identifier::VISIBLE},
#line 100 "webutil/css/identifier.gperf"
    {"xx-large", Identifier::XX_LARGE},
#line 51 "webutil/css/identifier.gperf"
    {"open-quote", Identifier::OPEN_QUOTE},
#line 95 "webutil/css/identifier.gperf"
    {"xx-small", Identifier::XX_SMALL},
#line 131 "webutil/css/identifier.gperf"
    {"invert", Identifier::INVERT},
#line 111 "webutil/css/identifier.gperf"
    {"menu", Identifier::MENU},
#line 139 "webutil/css/identifier.gperf"
    {"blink", Identifier::BLINK},
#line 149 "webutil/css/identifier.gperf"
    {"middle", Identifier::MIDDLE},
#line 89 "webutil/css/identifier.gperf"
    {"hide", Identifier::HIDE},
#line 58 "webutil/css/identifier.gperf"
    {"move", Identifier::MOVE},
#line 74 "webutil/css/identifier.gperf"
    {"block", Identifier::BLOCK},
#line 75 "webutil/css/identifier.gperf"
    {"list-item", Identifier::LIST_ITEM},
#line 52 "webutil/css/identifier.gperf"
    {"close-quote", Identifier::CLOSE_QUOTE},
#line 77 "webutil/css/identifier.gperf"
    {"inline-block", Identifier::INLINE_BLOCK},
#line 54 "webutil/css/identifier.gperf"
    {"no-close-quote", Identifier::NO_CLOSE_QUOTE},
#line 17 "webutil/css/identifier.gperf"
    {"inherit", Identifier::INHERIT},
#line 156 "webutil/css/identifier.gperf"
    {"--goog-body-color--", Identifier::GOOG_BODY_COLOR},
#line 123 "webutil/css/identifier.gperf"
    {"upper-roman", Identifier::UPPER_ROMAN},
#line 157 "webutil/css/identifier.gperf"
    {"--goog-body-link-color--", Identifier::GOOG_BODY_LINK_COLOR},
#line 22 "webutil/css/identifier.gperf"
    {"hidden", Identifier::HIDDEN},
#line 25 "webutil/css/identifier.gperf"
    {"medium", Identifier::MEDIUM},
#line 158 "webutil/css/identifier.gperf"
    {"--goog-big--", Identifier::GOOG_BIG},
#line 159 "webutil/css/identifier.gperf"
    {"--goog-small--", Identifier::GOOG_SMALL},
#line 150 "webutil/css/identifier.gperf"
    {"text-bottom", Identifier::TEXT_BOTTOM},
#line 135 "webutil/css/identifier.gperf"
    {"justify", Identifier::JUSTIFY},
#line 112 "webutil/css/identifier.gperf"
    {"message-box", Identifier::MESSAGE_BOX},
#line 126 "webutil/css/identifier.gperf"
    {"upper-latin", Identifier::UPPER_LATIN},
#line 16 "webutil/css/identifier.gperf"
    {"--goog-unknown--", Identifier::GOOG_UNKNOWN},
#line 155 "webutil/css/identifier.gperf"
    {"--goog-initial--", Identifier::GOOG_INITIAL},
#line 138 "webutil/css/identifier.gperf"
    {"line-through", Identifier::LINE_THROUGH},
#line 130 "webutil/css/identifier.gperf"
    {"upper-alpha", Identifier::UPPER_ALPHA}
  };

const struct idents *
IdentifierMapper::in_word_set (register const char *str, register unsigned int len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= MIN_HASH_VALUE)
        {
          register const struct idents *resword;

          switch (key - 5)
            {
              case 0:
                resword = &wordlist[0];
                goto compare;
              case 8:
                resword = &wordlist[1];
                goto compare;
              case 16:
                resword = &wordlist[2];
                goto compare;
              case 18:
                resword = &wordlist[3];
                goto compare;
              case 23:
                resword = &wordlist[4];
                goto compare;
              case 24:
                resword = &wordlist[5];
                goto compare;
              case 25:
                resword = &wordlist[6];
                goto compare;
              case 26:
                resword = &wordlist[7];
                goto compare;
              case 28:
                resword = &wordlist[8];
                goto compare;
              case 29:
                resword = &wordlist[9];
                goto compare;
              case 30:
                resword = &wordlist[10];
                goto compare;
              case 33:
                resword = &wordlist[11];
                goto compare;
              case 35:
                resword = &wordlist[12];
                goto compare;
              case 38:
                resword = &wordlist[13];
                goto compare;
              case 41:
                resword = &wordlist[14];
                goto compare;
              case 43:
                resword = &wordlist[15];
                goto compare;
              case 46:
                resword = &wordlist[16];
                goto compare;
              case 48:
                resword = &wordlist[17];
                goto compare;
              case 49:
                resword = &wordlist[18];
                goto compare;
              case 50:
                resword = &wordlist[19];
                goto compare;
              case 51:
                resword = &wordlist[20];
                goto compare;
              case 53:
                resword = &wordlist[21];
                goto compare;
              case 57:
                resword = &wordlist[22];
                goto compare;
              case 58:
                resword = &wordlist[23];
                goto compare;
              case 59:
                resword = &wordlist[24];
                goto compare;
              case 61:
                resword = &wordlist[25];
                goto compare;
              case 66:
                resword = &wordlist[26];
                goto compare;
              case 70:
                resword = &wordlist[27];
                goto compare;
              case 71:
                resword = &wordlist[28];
                goto compare;
              case 73:
                resword = &wordlist[29];
                goto compare;
              case 75:
                resword = &wordlist[30];
                goto compare;
              case 78:
                resword = &wordlist[31];
                goto compare;
              case 83:
                resword = &wordlist[32];
                goto compare;
              case 84:
                resword = &wordlist[33];
                goto compare;
              case 85:
                resword = &wordlist[34];
                goto compare;
              case 86:
                resword = &wordlist[35];
                goto compare;
              case 88:
                resword = &wordlist[36];
                goto compare;
              case 90:
                resword = &wordlist[37];
                goto compare;
              case 91:
                resword = &wordlist[38];
                goto compare;
              case 92:
                resword = &wordlist[39];
                goto compare;
              case 93:
                resword = &wordlist[40];
                goto compare;
              case 94:
                resword = &wordlist[41];
                goto compare;
              case 95:
                resword = &wordlist[42];
                goto compare;
              case 96:
                resword = &wordlist[43];
                goto compare;
              case 97:
                resword = &wordlist[44];
                goto compare;
              case 98:
                resword = &wordlist[45];
                goto compare;
              case 99:
                resword = &wordlist[46];
                goto compare;
              case 101:
                resword = &wordlist[47];
                goto compare;
              case 102:
                resword = &wordlist[48];
                goto compare;
              case 103:
                resword = &wordlist[49];
                goto compare;
              case 104:
                resword = &wordlist[50];
                goto compare;
              case 105:
                resword = &wordlist[51];
                goto compare;
              case 111:
                resword = &wordlist[52];
                goto compare;
              case 112:
                resword = &wordlist[53];
                goto compare;
              case 113:
                resword = &wordlist[54];
                goto compare;
              case 117:
                resword = &wordlist[55];
                goto compare;
              case 118:
                resword = &wordlist[56];
                goto compare;
              case 124:
                resword = &wordlist[57];
                goto compare;
              case 125:
                resword = &wordlist[58];
                goto compare;
              case 126:
                resword = &wordlist[59];
                goto compare;
              case 129:
                resword = &wordlist[60];
                goto compare;
              case 130:
                resword = &wordlist[61];
                goto compare;
              case 131:
                resword = &wordlist[62];
                goto compare;
              case 133:
                resword = &wordlist[63];
                goto compare;
              case 134:
                resword = &wordlist[64];
                goto compare;
              case 135:
                resword = &wordlist[65];
                goto compare;
              case 137:
                resword = &wordlist[66];
                goto compare;
              case 138:
                resword = &wordlist[67];
                goto compare;
              case 141:
                resword = &wordlist[68];
                goto compare;
              case 143:
                resword = &wordlist[69];
                goto compare;
              case 144:
                resword = &wordlist[70];
                goto compare;
              case 145:
                resword = &wordlist[71];
                goto compare;
              case 146:
                resword = &wordlist[72];
                goto compare;
              case 148:
                resword = &wordlist[73];
                goto compare;
              case 150:
                resword = &wordlist[74];
                goto compare;
              case 151:
                resword = &wordlist[75];
                goto compare;
              case 153:
                resword = &wordlist[76];
                goto compare;
              case 154:
                resword = &wordlist[77];
                goto compare;
              case 156:
                resword = &wordlist[78];
                goto compare;
              case 157:
                resword = &wordlist[79];
                goto compare;
              case 158:
                resword = &wordlist[80];
                goto compare;
              case 159:
                resword = &wordlist[81];
                goto compare;
              case 160:
                resword = &wordlist[82];
                goto compare;
              case 161:
                resword = &wordlist[83];
                goto compare;
              case 164:
                resword = &wordlist[84];
                goto compare;
              case 167:
                resword = &wordlist[85];
                goto compare;
              case 168:
                resword = &wordlist[86];
                goto compare;
              case 169:
                resword = &wordlist[87];
                goto compare;
              case 170:
                resword = &wordlist[88];
                goto compare;
              case 172:
                resword = &wordlist[89];
                goto compare;
              case 173:
                resword = &wordlist[90];
                goto compare;
              case 174:
                resword = &wordlist[91];
                goto compare;
              case 178:
                resword = &wordlist[92];
                goto compare;
              case 180:
                resword = &wordlist[93];
                goto compare;
              case 181:
                resword = &wordlist[94];
                goto compare;
              case 182:
                resword = &wordlist[95];
                goto compare;
              case 184:
                resword = &wordlist[96];
                goto compare;
              case 186:
                resword = &wordlist[97];
                goto compare;
              case 187:
                resword = &wordlist[98];
                goto compare;
              case 189:
                resword = &wordlist[99];
                goto compare;
              case 191:
                resword = &wordlist[100];
                goto compare;
              case 193:
                resword = &wordlist[101];
                goto compare;
              case 194:
                resword = &wordlist[102];
                goto compare;
              case 196:
                resword = &wordlist[103];
                goto compare;
              case 204:
                resword = &wordlist[104];
                goto compare;
              case 212:
                resword = &wordlist[105];
                goto compare;
              case 214:
                resword = &wordlist[106];
                goto compare;
              case 215:
                resword = &wordlist[107];
                goto compare;
              case 217:
                resword = &wordlist[108];
                goto compare;
              case 219:
                resword = &wordlist[109];
                goto compare;
              case 221:
                resword = &wordlist[110];
                goto compare;
              case 223:
                resword = &wordlist[111];
                goto compare;
              case 226:
                resword = &wordlist[112];
                goto compare;
              case 227:
                resword = &wordlist[113];
                goto compare;
              case 233:
                resword = &wordlist[114];
                goto compare;
              case 235:
                resword = &wordlist[115];
                goto compare;
              case 243:
                resword = &wordlist[116];
                goto compare;
              case 251:
                resword = &wordlist[117];
                goto compare;
              case 254:
                resword = &wordlist[118];
                goto compare;
              case 255:
                resword = &wordlist[119];
                goto compare;
              case 256:
                resword = &wordlist[120];
                goto compare;
              case 259:
                resword = &wordlist[121];
                goto compare;
              case 264:
                resword = &wordlist[122];
                goto compare;
              case 265:
                resword = &wordlist[123];
                goto compare;
              case 269:
                resword = &wordlist[124];
                goto compare;
              case 271:
                resword = &wordlist[125];
                goto compare;
              case 282:
                resword = &wordlist[126];
                goto compare;
              case 284:
                resword = &wordlist[127];
                goto compare;
              case 287:
                resword = &wordlist[128];
                goto compare;
              case 294:
                resword = &wordlist[129];
                goto compare;
              case 296:
                resword = &wordlist[130];
                goto compare;
              case 299:
                resword = &wordlist[131];
                goto compare;
              case 306:
                resword = &wordlist[132];
                goto compare;
              case 311:
                resword = &wordlist[133];
                goto compare;
              case 312:
                resword = &wordlist[134];
                goto compare;
              case 314:
                resword = &wordlist[135];
                goto compare;
              case 321:
                resword = &wordlist[136];
                goto compare;
              case 322:
                resword = &wordlist[137];
                goto compare;
              case 326:
                resword = &wordlist[138];
                goto compare;
              case 331:
                resword = &wordlist[139];
                goto compare;
              case 336:
                resword = &wordlist[140];
                goto compare;
              case 351:
                resword = &wordlist[141];
                goto compare;
              case 362:
                resword = &wordlist[142];
                goto compare;
              case 396:
                resword = &wordlist[143];
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
#line 160 "webutil/css/identifier.gperf"


//
// Constructor.
//

Identifier::Identifier(const UnicodeText& s) : ident_(IdentFromText(s)) {
  if (ident_ == OTHER)
    other_ = LowercaseAscii(s);
}

//
// Static methods mapping Ident's to strings
//

Identifier::Ident Identifier::IdentFromText(const UnicodeText& s) {
  const idents* a = IdentifierMapper::in_word_set(s.utf8_data(),
                                                  s.utf8_length());
  if (a)
    return a->id;
  else
    return OTHER;
}

static struct {
  const char* name;
  int len;
} gKnownIdentifiers[TOTAL_KEYWORDS];

static void InitializeIdentifierNameLookupTable() {
  for (int i = 0; i < TOTAL_KEYWORDS; ++i) {
    gKnownIdentifiers[wordlist[i].id].name = wordlist[i].name;
    gKnownIdentifiers[wordlist[i].id].len = strlen(wordlist[i].name);
  }
}

UnicodeText Identifier::TextFromIdent(Ident p) {
  if (p == OTHER) {
    return UTF8ToUnicodeText("OTHER", 5, false);
  } else {
    DCHECK_LT(p, OTHER);
    return UTF8ToUnicodeText(gKnownIdentifiers[p].name,
                             gKnownIdentifiers[p].len,
                             false);
  }
}

} // namespace

REGISTER_MODULE_INITIALIZER(identifier, {
  Css::InitializeIdentifierNameLookupTable();
});
