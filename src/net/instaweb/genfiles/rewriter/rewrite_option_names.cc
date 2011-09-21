/* C++ code produced by gperf version 3.0.3 */
/* Command-line: /usr/bin/gperf -m 10 rewriter/rewrite_option_names.gperf  */
/* Computed positions: -k'2-3' */

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

#line 1 "rewriter/rewrite_option_names.gperf"

// rewrite_options_names.cc is automatically generated from
// rewrite_options_names.gperf.
// Author: jmarantz@google.com

#include "base/basictypes.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
#line 23 "rewriter/rewrite_option_names.gperf"
struct FilterMap {
  const char* name;
  net_instaweb::RewriteOptions::Filter filter;
};
#include <string.h>

#define TOTAL_KEYWORDS 33
#define MIN_WORD_LENGTH 8
#define MAX_WORD_LENGTH 33
#define MIN_HASH_VALUE 12
#define MAX_HASH_VALUE 51
/* maximum key range = 40, duplicates = 0 */

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

class FilterMapper
{
private:
  static inline unsigned int hash (const char *str, unsigned int len);
public:
  static const struct FilterMap *Lookup (const char *str, unsigned int len);
};

inline unsigned int
FilterMapper::hash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52,  8, 52, 23, 16,  2,
      25, 52, 52,  1, 52,  8, 11,  1,  1,  0,
      21, 52,  5,  9, 14,  2, 22,  0, 11, 52,
      52, 52, 52, 52, 52, 52, 52,  8, 52, 23,
      16,  2, 25, 52, 52,  1, 52,  8, 11,  1,
       1,  0, 21, 52,  5,  9, 14,  2, 22,  0,
      11, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52
    };
  return len + asso_values[(unsigned char)str[2]] + asso_values[(unsigned char)str[1]];
}

static const struct FilterMap kHtmlNameTable[] =
  {
    {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
    {""}, {""}, {""},
#line 31 "rewriter/rewrite_option_names.gperf"
    {"combine_css",                       RewriteOptions::kCombineCss},
#line 53 "rewriter/rewrite_option_names.gperf"
    {"rewrite_css",                       RewriteOptions::kRewriteCss},
#line 33 "rewriter/rewrite_option_names.gperf"
    {"combine_heads",                     RewriteOptions::kCombineHeads},
#line 60 "rewriter/rewrite_option_names.gperf"
    {"trim_urls",                         RewriteOptions::kLeftTrimUrls},
#line 51 "rewriter/rewrite_option_names.gperf"
    {"remove_quotes",                     RewriteOptions::kRemoveQuotes},
#line 54 "rewriter/rewrite_option_names.gperf"
    {"rewrite_domains",                   RewriteOptions::kRewriteDomains},
#line 50 "rewriter/rewrite_option_names.gperf"
    {"remove_comments",                   RewriteOptions::kRemoveComments},
#line 32 "rewriter/rewrite_option_names.gperf"
    {"combine_javascript",                RewriteOptions::kCombineJavascript},
#line 55 "rewriter/rewrite_option_names.gperf"
    {"rewrite_javascript",                RewriteOptions::kRewriteJavascript},
#line 34 "rewriter/rewrite_option_names.gperf"
    {"convert_jpeg_to_webp",              RewriteOptions::kConvertJpegToWebp},
#line 39 "rewriter/rewrite_option_names.gperf"
    {"inline_css",                        RewriteOptions::kInlineCss},
#line 38 "rewriter/rewrite_option_names.gperf"
    {"flush_html",                        RewriteOptions::kFlushHtml},
#line 52 "rewriter/rewrite_option_names.gperf"
    {"resize_images",                     RewriteOptions::kResizeImages},
#line 40 "rewriter/rewrite_option_names.gperf"
    {"inline_images",                     RewriteOptions::kInlineImages},
#line 56 "rewriter/rewrite_option_names.gperf"
    {"rewrite_style_attributes",          RewriteOptions::kRewriteStyleAttributes},
#line 47 "rewriter/rewrite_option_names.gperf"
    {"outline_css",                       RewriteOptions::kOutlineCss},
#line 36 "rewriter/rewrite_option_names.gperf"
    {"elide_attributes",                  RewriteOptions::kElideAttributes},
#line 41 "rewriter/rewrite_option_names.gperf"
    {"inline_javascript",                 RewriteOptions::kInlineJavascript},
#line 30 "rewriter/rewrite_option_names.gperf"
    {"collapse_whitespace",               RewriteOptions::kCollapseWhitespace},
#line 42 "rewriter/rewrite_option_names.gperf"
    {"insert_img_dimensions",             RewriteOptions::kInsertImageDimensions},
#line 59 "rewriter/rewrite_option_names.gperf"
    {"strip_scripts",                     RewriteOptions::kStripScripts},
#line 43 "rewriter/rewrite_option_names.gperf"
    {"insert_image_dimensions",           RewriteOptions::kInsertImageDimensions},
#line 48 "rewriter/rewrite_option_names.gperf"
    {"outline_javascript",                RewriteOptions::kOutlineJavascript},
#line 57 "rewriter/rewrite_option_names.gperf"
    {"rewrite_style_attributes_with_url", RewriteOptions::kRewriteStyleAttributesWithUrl},
#line 35 "rewriter/rewrite_option_names.gperf"
    {"div_structure",                     RewriteOptions::kDivStructure},
#line 37 "rewriter/rewrite_option_names.gperf"
    {"extend_cache",                      RewriteOptions::kExtendCache},
#line 46 "rewriter/rewrite_option_names.gperf"
    {"move_css_to_head",                  RewriteOptions::kMoveCssToHead},
#line 58 "rewriter/rewrite_option_names.gperf"
    {"sprite_images",                     RewriteOptions::kSpriteImages},
#line 28 "rewriter/rewrite_option_names.gperf"
    {"add_head",                          RewriteOptions::kAddHead},
#line 44 "rewriter/rewrite_option_names.gperf"
    {"left_trim_urls",                    RewriteOptions::kLeftTrimUrls},
#line 49 "rewriter/rewrite_option_names.gperf"
    {"recompress_images",                 RewriteOptions::kRecompressImages},
#line 45 "rewriter/rewrite_option_names.gperf"
    {"make_google_analytics_async",       RewriteOptions::kMakeGoogleAnalyticsAsync},
    {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 29 "rewriter/rewrite_option_names.gperf"
    {"add_instrumentation",               RewriteOptions::kAddInstrumentation}
  };

const struct FilterMap *
FilterMapper::Lookup (register const char *str, register unsigned int len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = kHtmlNameTable[key].name;

          if ((((unsigned char)*str ^ (unsigned char)*s) & ~32) == 0 && !gperf_case_strncmp (str, s, len) && s[len] == '\0')
            return &kHtmlNameTable[key];
        }
    }
  return 0;
}
#line 61 "rewriter/rewrite_option_names.gperf"


RewriteOptions::Filter RewriteOptions::Lookup(const StringPiece& filter_name) {
  const FilterMap* entry = FilterMapper::Lookup(
      filter_name.data(), filter_name.size());
  if (entry != NULL) {
    return entry->filter;
  }
  return RewriteOptions::kEndOfFilters;
}

}  // namespace net_instaweb
