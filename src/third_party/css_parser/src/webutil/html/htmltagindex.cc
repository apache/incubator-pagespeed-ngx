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

// Copyright 2006, Google Inc.  All rights reserved.
// Author: mec@google.com  (Michael Chastain)

#include <ctype.h>
#include <string.h>
#include "webutil/html/htmltagindex.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/paranoid.h"
#include "strings/ascii_ctype.h"
#include "util/gtl/dense_hash_map.h"
#include "util/hash/hash.h"

// Assert character properties that the fast path depends on.
//   (Uppercase letter | 0x20) == lowercase letter
//   All digits have 0x20 bit set
//   Several special chars have 0x20 bit set
COMPILE_ASSERT(('A'|0x20)=='a', A_a);
COMPILE_ASSERT(('B'|0x20)=='b', B_b);
COMPILE_ASSERT(('C'|0x20)=='c', C_c);
COMPILE_ASSERT(('D'|0x20)=='d', D_d);
COMPILE_ASSERT(('E'|0x20)=='e', E_e);
COMPILE_ASSERT(('F'|0x20)=='f', F_f);
COMPILE_ASSERT(('G'|0x20)=='g', G_g);
COMPILE_ASSERT(('H'|0x20)=='h', H_h);
COMPILE_ASSERT(('I'|0x20)=='i', I_i);
COMPILE_ASSERT(('J'|0x20)=='j', J_j);
COMPILE_ASSERT(('K'|0x20)=='k', K_k);
COMPILE_ASSERT(('L'|0x20)=='l', L_l);
COMPILE_ASSERT(('M'|0x20)=='m', M_m);
COMPILE_ASSERT(('N'|0x20)=='n', N_n);
COMPILE_ASSERT(('O'|0x20)=='o', O_o);
COMPILE_ASSERT(('P'|0x20)=='p', P_p);
COMPILE_ASSERT(('Q'|0x20)=='q', Q_q);
COMPILE_ASSERT(('R'|0x20)=='r', R_r);
COMPILE_ASSERT(('S'|0x20)=='s', S_s);
COMPILE_ASSERT(('T'|0x20)=='t', T_t);
COMPILE_ASSERT(('U'|0x20)=='u', U_u);
COMPILE_ASSERT(('V'|0x20)=='v', V_v);
COMPILE_ASSERT(('W'|0x20)=='w', W_w);
COMPILE_ASSERT(('X'|0x20)=='x', X_x);
COMPILE_ASSERT(('Y'|0x20)=='y', Y_y);
COMPILE_ASSERT(('Z'|0x20)=='z', Z_z);
COMPILE_ASSERT('0'&0x20, d0);
COMPILE_ASSERT('1'&0x20, d1);
COMPILE_ASSERT('2'&0x20, d2);
COMPILE_ASSERT('3'&0x20, d3);
COMPILE_ASSERT('4'&0x20, d4);
COMPILE_ASSERT('5'&0x20, d5);
COMPILE_ASSERT('6'&0x20, d6);
COMPILE_ASSERT('7'&0x20, d7);
COMPILE_ASSERT('8'&0x20, d8);
COMPILE_ASSERT('9'&0x20, d9);
COMPILE_ASSERT('!'&0x20, cBang);
COMPILE_ASSERT('-'&0x20, cDash);
COMPILE_ASSERT('?'&0x20, cQuestion);

// Ctor.

HtmlTagIndex::HtmlTagIndex()
  : case_sensitive_fixed_(false),
    index_max_(kHtmlTagBuiltinMax),
    custom_tag_map_(NULL) {
  this->SetCaseSensitive(false);
}

// Dtor.

HtmlTagIndex::~HtmlTagIndex() {
}

// Case sensitivity.

void HtmlTagIndex::SetCaseSensitive(bool case_sensitive) {
  CHECK(!case_sensitive_fixed_);
  case_sensitive_ = SanitizeBool(case_sensitive);
  if (case_sensitive_) {
    case_mask_1_ = case_mask_2_ = case_mask_3_ = case_mask_4_ = 0;
    case_mask_5_ = case_mask_6_ = case_mask_7_ = case_mask_8_ = 0;
  } else {
    case_mask_1_ = 0x20;
    case_mask_2_ = 0x2020;
    case_mask_3_ = 0x202020;
    case_mask_4_ = 0x20202020;
    case_mask_5_ = 0x2020202020ull;
    case_mask_6_ = 0x202020202020ull;
    case_mask_7_ = 0x20202020202020ull;
    case_mask_8_ = 0x2020202020202020ull;
  }
}

// Return a case-aware string.  If case-sensitive, this is the same string.
// If case-insensitive, this is the lower-case version.

static string CaseAwareString(bool case_sensitive,
                              const char* tag,
                              int length) {
  CHECK_GE(length, 0);
  string str;
  if (case_sensitive) {
    str.assign(tag, length);
  } else {
    for (int i = 0; i < length; ++i) {
      str += ascii_tolower(tag[i]);
    }
  }
  return str;
}

// Add a tag.

int HtmlTagIndex::AddHtmlTag(const char* tag, int length) {
  // No more changing the case sensitivity.
  case_sensitive_fixed_ = true;

  // Look for existing tag.
  const int tag_id = FindHtmlTag(tag, length);
  if (tag_id != kHtmlTagUnknown)
    return tag_id;

  // Add to the custom table.
  if (custom_tag_map_.get() == NULL) {
    custom_tag_map_.reset(new CustomTagMap);
    custom_tag_map_->set_empty_key(string(""));
  }
  string tag_copy(CaseAwareString(case_sensitive_, tag, length));
  (*custom_tag_map_)[tag_copy] = index_max_;
  return index_max_++;
}

// Find a tag.
// This is hard-wired to go fast.

int HtmlTagIndex::FindHtmlTag(const char* tag, int length) const {
// These macros convert a string to a uint32 or uint64
#define S1(s) ((s)[0])
#define S2(s) (S1(s) | ((s)[1] << 8))
#define S3(s) (S2(s) | ((s)[2] << 16))
#define S4(s) (S3(s) | ((s)[3] << 24))
#define S5(s) (S4(s) | (static_cast<uint64>((s)[4]) << 32))
#define S6(s) (S5(s) | (static_cast<uint64>((s)[5]) << 40))
#define S7(s) (S6(s) | (static_cast<uint64>((s)[6]) << 48))
#define S8(s) (S7(s) | (static_cast<uint64>((s)[7]) << 56))

// These macros convert a sequence of characters to a uint32 or uint64
#define C1(c0)             (c0)
#define C2(c0, c1)         (C1(c0) | ((c1) << 8))
#define C3(c0, c1, c2)     (C2(c0, c1) | ((c2) << 16))
#define C4(c0, c1, c2, c3) (C3(c0, c1, c2) | ((c3) << 24))
#define C5(c0, c1, c2, c3, c4) \
  (C4(c0, c1, c2, c3) | (static_cast<uint64>(c4) << 32))
#define C6(c0, c1, c2, c3, c4, c5) \
  (C5(c0, c1, c2, c3, c4) | (static_cast<uint64>(c5) << 40))
#define C7(c0, c1, c2, c3, c4, c5, c6) \
  (C6(c0, c1, c2, c3, c4, c5) | (static_cast<uint64>(c6) << 48))
#define C8(c0, c1, c2, c3, c4, c5, c6, c7) \
  (C7(c0, c1, c2, c3, c4, c5, c6) | (static_cast<uint64>(c7) << 56))

  switch (length) {
  case 0:
    // Empty tag
    return kHtmlTagZeroLength;
  case 1:
    {
      switch (S1(tag)|case_mask_1_) {
        default: break;
        // From html 4.01 spec
        case C1('a'): return kHtmlTagA;
        case C1('b'): return kHtmlTagB;
        case C1('i'): return kHtmlTagI;
        case C1('p'): return kHtmlTagP;
        case C1('q'): return kHtmlTagQ;
        case C1('s'): return kHtmlTagS;
        case C1('u'): return kHtmlTagU;
      }
    }
    break;
  case 2:
    {
      switch (S2(tag)|case_mask_2_) {
        default: break;
        // From html 4.01 spec
        case C2('b','r'): return kHtmlTagBr;
        case C2('d','d'): return kHtmlTagDd;
        case C2('d','l'): return kHtmlTagDl;
        case C2('d','t'): return kHtmlTagDt;
        case C2('e','m'): return kHtmlTagEm;
        // Beware of matching "h\x11" or "H\x11" in case-insensitive mode.
        case C2('h','1'): if (tag[1] == '1') return kHtmlTagH1; else break;
        case C2('h','2'): if (tag[1] == '2') return kHtmlTagH2; else break;
        case C2('h','3'): if (tag[1] == '3') return kHtmlTagH3; else break;
        case C2('h','4'): if (tag[1] == '4') return kHtmlTagH4; else break;
        case C2('h','5'): if (tag[1] == '5') return kHtmlTagH5; else break;
        case C2('h','6'): if (tag[1] == '6') return kHtmlTagH6; else break;
        case C2('h','r'): return kHtmlTagHr;
        case C2('l','i'): return kHtmlTagLi;
        case C2('o','l'): return kHtmlTagOl;
        case C2('t','d'): return kHtmlTagTd;
        case C2('t','h'): return kHtmlTagTh;
        case C2('t','r'): return kHtmlTagTr;
        case C2('t','t'): return kHtmlTagTt;
        case C2('u','l'): return kHtmlTagUl;
      }
    }
    break;
  case 3:
    {
      switch (S3(tag)|case_mask_3_) {
        default: break;
        // From html 4.01 spec
        case C3('b','d','o'): return kHtmlTagBdo;
        case C3('b','i','g'): return kHtmlTagBig;
        case C3('c','o','l'): return kHtmlTagCol;
        case C3('d','e','l'): return kHtmlTagDel;
        case C3('d','i','r'): return kHtmlTagDir;
        case C3('d','i','v'): return kHtmlTagDiv;
        case C3('d','f','n'): return kHtmlTagDfn;
        case C3('i','m','g'): return kHtmlTagImg;
        case C3('i','n','s'): return kHtmlTagIns;
        case C3('k','b','d'): return kHtmlTagKbd;
        case C3('m','a','p'): return kHtmlTagMap;
        case C3('p','r','e'): return kHtmlTagPre;
        case C3('s','u','b'): return kHtmlTagSub;
        case C3('s','u','p'): return kHtmlTagSup;
        case C3('v','a','r'): return kHtmlTagVar;
        case C3('w','b','r'): return kHtmlTagWbr;
        case C3('x','m','p'): return kHtmlTagXmp;
        // Used in repository/lexer/html_lexer.cc
        case C3('!','-','-'):
          // These chars have no upper-lower case form so I haev to rematch.
          if (S3(tag) == C3('!','-','-'))
            return kHtmlTagBangDashDash;
          break;
      }
    }
    break;
  case 4:
    {
      switch (S4(tag)|case_mask_4_) {
        default: break;
        // From html 4.01 spec
        case C4('a','b','b','r'): return kHtmlTagAbbr;
        case C4('a','r','e','a'): return kHtmlTagArea;
        case C4('b','a','s','e'): return kHtmlTagBase;
        case C4('b','o','d','y'): return kHtmlTagBody;
        case C4('c','i','t','e'): return kHtmlTagCite;
        case C4('c','o','d','e'): return kHtmlTagCode;
        case C4('f','o','n','t'): return kHtmlTagFont;
        case C4('f','o','r','m'): return kHtmlTagForm;
        case C4('h','e','a','d'): return kHtmlTagHead;
        case C4('h','t','m','l'): return kHtmlTagHtml;
        case C4('l','i','n','k'): return kHtmlTagLink;
        case C4('m','e','n','u'): return kHtmlTagMenu;
        case C4('m','e','t','a'): return kHtmlTagMeta;
        case C4('s','a','m','p'): return kHtmlTagSamp;
        case C4('s','p','a','n'): return kHtmlTagSpan;
        case C4('n','o','b','r'): return kHtmlTagNobr;
      }
    }
    break;
  case 5:
    {
      switch (S5(tag)|case_mask_5_) {
        default: break;
        // From html 4.01 spec
        case C5('f','r','a','m','e'): return kHtmlTagFrame;
        case C5('i','n','p','u','t'): return kHtmlTagInput;
        case C5('l','a','b','e','l'): return kHtmlTagLabel;
        case C5('p','a','r','a','m'): return kHtmlTagParam;
        case C5('s','m','a','l','l'): return kHtmlTagSmall;
        case C5('s','t','y','l','e'): return kHtmlTagStyle;
        case C5('t','a','b','l','e'): return kHtmlTagTable;
        case C5('t','b','o','d','y'): return kHtmlTagTbody;
        case C5('t','f','o','o','t'): return kHtmlTagTfoot;
        case C5('t','h','e','a','d'): return kHtmlTagThead;
        case C5('t','i','t','l','e'): return kHtmlTagTitle;
        // Used in repository/lexer/html_lexer.cc
        case C5('b','l','i','n','k'): return kHtmlTagBlink;
        // Used in repository/parsers/base/handler-parser.cc
        case C5('e','m','b','e','d'): return kHtmlTagEmbed;
        case C5('i','m','a','g','e'): return kHtmlTagImage;
        // From Netscape Navigator 4.0
        case C5('l','a','y','e','r'): return kHtmlTagLayer;
      }
    }
    break;
  case 6:
    {
      switch (S6(tag)|case_mask_6_) {
        default: break;
        // From html 4.01 spec
        case C6('a','p','p','l','e','t'): return kHtmlTagApplet;
        case C6('b','u','t','t','o','n'): return kHtmlTagButton;
        case C6('c','e','n','t','e','r'): return kHtmlTagCenter;
        case C6('i','f','r','a','m','e'): return kHtmlTagIframe;
        case C6('l','e','g','e','n','d'): return kHtmlTagLegend;
        case C6('o','b','j','e','c','t'): return kHtmlTagObject;
        case C6('o','p','t','i','o','n'): return kHtmlTagOption;
        case C6('s','c','r','i','p','t'): return kHtmlTagScript;
        case C6('s','e','l','e','c','t'): return kHtmlTagSelect;
        case C6('s','t','r','i','k','e'): return kHtmlTagStrike;
        case C6('s','t','r','o','n','g'): return kHtmlTagStrong;
        case C6('s','p','a','c','e','r'): return kHtmlTagSpacer;
        // From Netscape Navigator 4.0
        case C6('i','l','a','y','e','r'): return kHtmlTagIlayer;
        case C6('k','e','y','g','e','n'): return kHtmlTagKeygen;
        case C6('s','e','r','v','e','r'): return kHtmlTagServer;
      }
    }
    break;
  case 7:
    {
      switch (S7(tag)|case_mask_7_) {
        default: break;
        // From html 4.01 spec
        case C7('a','c','r','o','n','y','m'): return kHtmlTagAcronym;
        case C7('a','d','d','r','e','s','s'): return kHtmlTagAddress;
        case C7('c','a','p','t','i','o','n'): return kHtmlTagCaption;
        case C7('i','s','i','n','d','e','x'): return kHtmlTagIsindex;
        // Used in repository/parsers/base/handler-parser.cc
        case C7('m','a','r','q','u','e','e'): return kHtmlTagMarquee;
        case C7('b','g','s','o','u','n','d'): return kHtmlTagBgsound;
        case C7('l','i','s','t','i','n','g'): return kHtmlTagListing;
        case C7('n','o','e','m','b','e','d'): return kHtmlTagNoembed;
        // From Netscape Navigator 4.0
        case C7('n','o','l','a','y','e','r'): return kHtmlTagNolayer;
        // Legacy tag used mostly by Russian sites
        case C7('n','o','i','n','d','e','x'): return kHtmlTagNoindex;
      }
    }
    break;
  case 8:
    {
      switch (S8(tag)|case_mask_8_) {
        default: break;
        // From html 4.01 spec
        case C8('b','a','s','e','f','o','n','t'): return kHtmlTagBasefont;
        case C8('c','o','l','g','r','o','u','p'): return kHtmlTagColgroup;
        case C8('f','i','e','l','d','s','e','t'): return kHtmlTagFieldset;
        case C8('f','r','a','m','e','s','e','t'): return kHtmlTagFrameset;
        case C8('n','o','f','r','a','m','e','s'): return kHtmlTagNoframes;
        case C8('n','o','s','c','r','i','p','t'): return kHtmlTagNoscript;
        case C8('o','p','t','g','r','o','u','p'): return kHtmlTagOptgroup;
        case C8('t','e','x','t','a','r','e','a'): return kHtmlTagTextarea;
        // From Netscape Navigator 4.0
        case C8('m','u','l','t','i','c','o','l'): return kHtmlTagMulticol;
      }
    }
    break;
  case 9:
    {
      if ((S4(tag+0)|case_mask_4_) == C4('p','l','a','i') &&
          (S4(tag+4)|case_mask_4_) == C4('n','t','e','x') &&
          (S1(tag+8)|case_mask_1_) == C1('t'))
        return kHtmlTagPlaintext;
    }
    break;
  case 10:
    {
      // From html 4.01 spec
      if ((S4(tag+0)|case_mask_4_) == C4('b','l','o','c') &&
          (S4(tag+4)|case_mask_4_) == C4('k','q','u','o') &&
          (S2(tag+8)|case_mask_2_) == C2('t','e'))
        return kHtmlTagBlockquote;
    }
    break;
  }

  // !doctype is special. Any tag name starting with !doctype (no need for
  // exact match) is considered !doctype. Tested on IE 7.0 and Firefox 2.0.
  // You can also refer to (ongoing work):
  //  http://whatwg.org/specs/web-apps/current-work/#markup
  if (length >= 8 && S1(tag) == C1('!') &&
      (S8(tag)|case_mask_8_) == C8('!','d','o','c','t','y','p','e'))
    return kHtmlTagBangDoctype;

  // Otherwise, !blahblah and ?blahblah are comments.
  if (S1(tag) == C1('!') || S1(tag) == C1('?'))
    return kHtmlTagBogusComment;

#undef C8
#undef C7
#undef C6
#undef C5
#undef C4
#undef C3
#undef C2
#undef C1
#undef S8
#undef S7
#undef S6
#undef S5
#undef S4
#undef S3
#undef S2
#undef S1

  // Look in the custom table.
  if (custom_tag_map_.get() != NULL) {
    string tag_copy(CaseAwareString(case_sensitive_, tag, length));
    CustomTagMap::const_iterator it = custom_tag_map_->find(tag_copy);
    if (it != custom_tag_map_->end()) {
      return it->second;
    }
  }

  // Unknown tag.
  return kHtmlTagUnknown;
}
