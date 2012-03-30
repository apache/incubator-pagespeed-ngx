/*
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/htmlparse/public/html_keywords.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

struct HtmlKeywordsSequence {
  const char* sequence;
  const unsigned char value[3];
};

// TODO(jmarantz): the multi-byte sequences are not working yet.
HtmlKeywordsSequence kHtmlKeywordsSequences[] = {
  { "AElig", {0xC6, 0x0} },
  { "Aacute", {0xC1, 0x0} },
  { "Acirc", {0xC2, 0x0} },
  { "Agrave", {0xC0, 0x0} },
  { "Aring", {0xC5, 0x0} },
  { "Atilde", {0xC3, 0x0} },
  { "Auml", {0xC4, 0x0} },
  { "Ccedil", {0xC7, 0x0} },
  { "ETH", {0xD0, 0x0} },
  { "Eacute", {0xC9, 0x0} },
  { "Ecirc", {0xCA, 0x0} },
  { "Egrave", {0xC8, 0x0} },
  { "Euml", {0xCB, 0x0} },
  { "Iacute", {0xCD, 0x0} },
  { "Icirc", {0xCE, 0x0} },
  { "Igrave", {0xCC, 0x0} },
  { "Iuml", {0xCF, 0x0} },
  { "Ntilde", {0xD1, 0x0} },
  { "Oacute", {0xD3, 0x0} },
  { "Ocirc", {0xD4, 0x0} },
  { "Ograve", {0xD2, 0x0} },
  { "Oslash", {0xD8, 0x0} },
  { "Otilde", {0xD5, 0x0} },
  { "Ouml", {0xD6, 0x0} },
  { "THORN", {0xDE, 0x0} },
  { "Uacute", {0xDA, 0x0} },
  { "Ucirc", {0xDB, 0x0} },
  { "Ugrave", {0xD9, 0x0} },
  { "Uuml", {0xDC, 0x0} },
  { "Yacute", {0xDD, 0x0} },
  { "aacute", {0xE1, 0x0} },
  { "acirc", {0xE2, 0x0} },
  { "acute", {0xB4, 0x0} },
  { "aelig", {0xE6, 0x0} },
  { "agrave", {0xE0, 0x0} },
  { "amp", {0x26, 0x0} },
  { "aring", {0xE5, 0x0} },
  { "atilde", {0xE3, 0x0} },
  { "auml", {0xE4, 0x0} },
  { "brvbar", {0xA6, 0x0} },
  { "ccedil", {0xE7, 0x0} },
  { "cedil", {0xB8, 0x0} },
  { "cent", {0xA2, 0x0} },
  { "copy", {0xA9, 0x0} },
  { "curren", {0xA4, 0x0} },
  { "deg", {0xB0, 0x0} },
  { "divide", {0xF7, 0x0} },
  { "eacute", {0xE9, 0x0} },
  { "ecirc", {0xEA, 0x0} },
  { "egrave", {0xE8, 0x0} },
  { "eth", {0xF0, 0x0} },
  { "euml", {0xEB, 0x0} },
  { "frac12", {0xBD, 0x0} },
  { "frac14", {0xBC, 0x0} },
  { "frac34", {0xBE, 0x0} },
  { "gt", {0x3E, 0x0} },
  { "iacute", {0xED, 0x0} },
  { "icirc", {0xEE, 0x0} },
  { "iexcl", {0xA1, 0x0} },
  { "igrave", {0xEC, 0x0} },
  { "iquest", {0xBF, 0x0} },
  { "iuml", {0xEF, 0x0} },
  { "laquo", {0xAB, 0x0} },
  { "lt", {0x3C, 0x0} },
  { "macr", {0xAF, 0x0} },
  { "micro", {0xB5, 0x0} },
  { "middot", {0xB7, 0x0} },
  { "nbsp", {0xA0, 0x0} },
  { "not", {0xAC, 0x0} },
  { "ntilde", {0xF1, 0x0} },
  { "oacute", {0xF3, 0x0} },
  { "ocirc", {0xF4, 0x0} },
  { "ograve", {0xF2, 0x0} },
  { "ordf", {0xAA, 0x0} },
  { "ordm", {0xBA, 0x0} },
  { "oslash", {0xF8, 0x0} },
  { "otilde", {0xF5, 0x0} },
  { "ouml", {0xF6, 0x0} },
  { "para", {0xB6, 0x0} },
  { "plusmn", {0xB1, 0x0} },
  { "pound", {0xA3, 0x0} },
  { "quot", {0x22, 0x0} },
  { "raquo", {0xBB, 0x0} },
  { "reg", {0xAE, 0x0} },
  { "sect", {0xA7, 0x0} },
  { "shy", {0xAD, 0x0} },
  { "sup1", {0xB9, 0x0} },
  { "sup2", {0xB2, 0x0} },
  { "sup3", {0xB3, 0x0} },
  { "szlig", {0xDF, 0x0} },
  { "thorn", {0xFE, 0x0} },
  { "times", {0xD7, 0x0} },
  { "uacute", {0xFA, 0x0} },
  { "ucirc", {0xFB, 0x0} },
  { "ugrave", {0xF9, 0x0} },
  { "uml", {0xA8, 0x0} },
  { "uuml", {0xFC, 0x0} },
  { "yacute", {0xFD, 0x0} },
  { "yen", {0xA5, 0x0} },
  { "yuml", {0xFF, 0x0} }
};

// http://www.w3.org/TR/html4/sgml/entities.html contains a list of multi-byte
// codes.  When we see any of these in an HTML attribute, we cannot currently
// unescape it, because we have no general strategy for multi-byte encoding.
//
// TODO(jmarantz): 1. We could adopt UTF-8 as our in-memory representation for
// manipulating attribute values, independent of the source document encoding.
//
// TODO(jmarantz): 2. We could sense UTF-8 encoding on documents based
// on Content-Type &/or meta-tags & thereby allow parsing of HTML
// attributes with utf-8 encodings.  This is not strictly necessary
// for URL attributes where this is not allowed.  See
// http://www.w3.org/TR/html401/appendix/notes.html#h-B.2.
//
//
// See also: http://www.w3.org/TR/2011/WD-html5-20110113/
// named-character-references.html#named-character-references
// which appears to contradict other references in the validity of &apos; and
// likely other details.  This may be issue of HTML5 vs HTML4.

struct HtmlMultiByteSequence {
  const char* sequence;
  int value;  // ignored for now.
};

struct HtmlMultiByteSequenceCompare {
  // TODO(jmarantz): Write a version of StringCaseCompare based on const char*
  // to avoid extra calls to strlen.
  bool operator()(const HtmlMultiByteSequence& a, const char* b) const {
    return StringCaseCompare(a.sequence, b) < 0;
  }
  bool operator()(const char* a, const HtmlMultiByteSequence& b) const {
    return StringCaseCompare(a, b.sequence) < 0;
  }
};

HtmlMultiByteSequence kHtmlKeywordsSequencesMultiByte[] = {
  { "alefsym", 8501 },
  { "Alpha", 913 },
  { "alpha", 945 },
  { "and", 8743 },
  { "ang", 8736 },
  { "asymp", 8776 },
  { "bdquo", 8222 },
  { "Beta", 914 },
  { "beta", 946 },
  { "bull", 8226 },
  { "cap", 8745 },
  { "Chi", 935 },
  { "chi", 967 },
  { "circ", 710 },
  { "clubs", 9827 },
  { "cong", 8773 },
  { "crarr", 8629 },
  { "cup", 8746 },
  { "dagger", 8224 },
  { "Dagger", 8225 },
  { "darr", 8595 },
  { "dArr", 8659 },
  { "Delta", 916 },
  { "delta", 948 },
  { "diams", 9830 },
  { "empty", 8709 },
  { "emsp", 8195 },
  { "ensp", 8194 },
  { "Epsilon", 917 },
  { "epsilon", 949 },
  { "equiv", 8801 },
  { "Eta", 919 },
  { "eta", 951 },
  { "euro", 8364 },
  { "exist", 8707 },
  { "fnof", 402 },
  { "forall", 8704 },
  { "frasl", 8260 },
  { "Gamma", 915 },
  { "gamma", 947 },
  { "ge", 8805 },
  { "harr", 8596 },
  { "hArr", 8660 },
  { "hearts", 9829 },
  { "hellip", 8230 },
  { "image", 8465 },
  { "infin", 8734 },
  { "int", 8747 },
  { "Iota", 921 },
  { "iota", 953 },
  { "isin", 8712 },
  { "Kappa", 922 },
  { "kappa", 954 },
  { "Lambda", 923 },
  { "lambda", 955 },
  { "lang", 9001 },
  { "larr", 8592 },
  { "lArr", 8656 },
  { "lceil", 8968 },
  { "ldquo", 8220 },
  { "le", 8804 },
  { "lfloor", 8970 },
  { "lowast", 8727 },
  { "loz", 9674 },
  { "lrm", 8206 },
  { "lsaquo", 8249 },
  { "lsquo", 8216 },
  { "mdash", 8212 },
  { "minus", 8722 },
  { "Mu", 924 },
  { "mu", 956 },
  { "nabla", 8711 },
  { "ndash", 8211 },
  { "ne", 8800 },
  { "ni", 8715 },
  { "notin", 8713 },
  { "nsub", 8836 },
  { "Nu", 925 },
  { "nu", 957 },
  { "OElig", 338 },
  { "oelig", 339 },
  { "oline", 8254 },
  { "Omega", 937 },
  { "omega", 969 },
  { "Omicron", 927 },
  { "omicron", 959 },
  { "oplus", 8853 },
  { "or", 8744 },
  { "otimes", 8855 },
  { "part", 8706 },
  { "permil", 8240 },
  { "perp", 8869 },
  { "Phi", 934 },
  { "phi", 966 },
  { "Pi", 928 },
  { "pi", 960 },
  { "piv", 982 },
  { "prime", 8242 },
  { "Prime", 8243 },
  { "prod", 8719 },
  { "prop", 8733 },
  { "Psi", 936 },
  { "psi", 968 },
  { "radic", 8730 },
  { "rang", 9002 },
  { "rarr", 8594 },
  { "rArr", 8658 },
  { "rceil", 8969 },
  { "rdquo", 8221 },
  { "real", 8476 },
  { "rfloor", 8971 },
  { "Rho", 929 },
  { "rho", 961 },
  { "rlm", 8207 },
  { "rsaquo", 8250 },
  { "rsquo", 8217 },
  { "sbquo", 8218 },
  { "Scaron", 352 },
  { "scaron", 353 },
  { "sdot", 8901 },
  { "Sigma", 931 },
  { "sigma", 963 },
  { "sigmaf", 962 },
  { "sim", 8764 },
  { "spades", 9824 },
  { "sub", 8834 },
  { "sube", 8838 },
  { "sum", 8721 },
  { "sup", 8835 },
  { "supe", 8839 },
  { "Tau", 932 },
  { "tau", 964 },
  { "there4", 8756 },
  { "Theta", 920 },
  { "theta", 952 },
  { "thetasym", 977 },
  { "thinsp", 8201 },
  { "tilde", 732 },
  { "trade", 8482 },
  { "uarr", 8593 },
  { "uArr", 8657 },
  { "upsih", 978 },
  { "Upsilon", 933 },
  { "upsilon", 965 },
  { "weierp", 8472 },
  { "Xi", 926 },
  { "xi", 958 },
  { "Yuml", 376 },
  { "Zeta", 918 },
  { "zeta", 950 },
  { "zwj", 8205 },
  { "zwnj", 8204 },
};

// String constants used to populate maps at initialization time.
// These are a little more expressive than static arrays of keywords.
// The penalty for this expressiveness is lack of compile-time checking,
// and startup time.  But the compile-time checking is replaced by
// debug-only init checks.

// Tables are a 4-level hierarchy:
//   table > [thead tbody tfoot] > tr > [td th]
//
// Note: we use trailing spaces in all these strings so that they can
// be concatenated more easily.  Note that we use 'omit_empty_strings'
// when we interpret via SplitStringPieceToVector.
const char kTableLeaves[] = "td th ";
const char kTableSections[] = "tbody tfoot thead ";
const char kTableElements[] = "td th tbody tfoot thead table tr ";
// TODO(jmarantz): consider caption, col, colgroup.

// Formatting elements are terminated by many other tags.
const char kFormattingElements[] =
    "b i em font strong small s cite q dfn abbr time code var "
    "samp kbd sub u mark bdi bdo ";
// TODO(jmarantz): consider ins and del & potentially lots more.

const char kListElements[] = "li ol ul ";
const char kDeclarationElements[] = "dl dt dd ";

const char kParagraphTerminators[] =
    "address article aside blockquote dir div dl fieldset "
    "footer form h1 h2 h3 h4 h5 h6 header hgroup hr menu nav ol p "
    "pre section table ul";

// TODO(jmarantz): handle & test Ruby containment.
// const char kRubyElements[] = "ruby rt rp ";

}  // namespace

HtmlKeywords* HtmlKeywords::singleton_ = NULL;

HtmlKeywords::HtmlKeywords() {
  InitEscapeSequences();
  InitAutoClose();
  InitContains();
  InitOptionallyClosedKeywords();
}

void HtmlKeywords::InitEscapeSequences() {
  StringSetInsensitive case_sensitive_symbols;
  for (size_t i = 0; i < arraysize(kHtmlKeywordsSequences); ++i) {
    // Put all symbols in the case-sensitive map
    const HtmlKeywordsSequence& seq = kHtmlKeywordsSequences[i];
    unescape_sensitive_map_[seq.sequence] =
        reinterpret_cast<const char*>(seq.value);

    // Don't populate the case-insensitive map for symbols that we've
    // already determined are case-sensitive.
    if (case_sensitive_symbols.find(seq.sequence) ==
        case_sensitive_symbols.end()) {
      // If this symbol is already present in the insensitive map, then it
      // must be case-sensitive.  E.g. &AElig; and &aelig; are distinct.
      StringStringMapInsensitive::iterator p =
          unescape_insensitive_map_.find(seq.sequence);
      if (p != unescape_insensitive_map_.end()) {
        // As this symbol is case-sensitive, we must remove it from the
        // case-insensitive map.  This way we will report an error for
        // &Aelig;, rather than &AElig; or &aelig; unpredictably.
        unescape_insensitive_map_.erase(p);
        case_sensitive_symbols.insert(seq.sequence);
      } else {
        unescape_insensitive_map_[seq.sequence] =
            reinterpret_cast<const char*>(seq.value);
      }

      // For now, we will only generate symbolic escaped-names for
      // single-byte sequences
      if (strlen(reinterpret_cast<const char*>(seq.value)) == 1) {
        escape_map_[reinterpret_cast<const char*>(seq.value)] = seq.sequence;
      }
    }
  }

  // Initialialize the keywords from HtmlName into a reverse table.  This could
  // have been generated by gperf, but it isn't.  It's easy enough to build it
  // given an iterator.
  keyword_vector_.resize(HtmlName::num_keywords() + 1);
  for (HtmlName::Iterator iter; !iter.AtEnd(); iter.Next()) {
    DCHECK_GE(HtmlName::num_keywords(), iter.keyword());
    keyword_vector_[iter.keyword()] = iter.name();
  }
  keyword_vector_[HtmlName::kNotAKeyword] = NULL;

  // Make sure the multi-byte sequences are in order (case-insensitive).  Ties
  // are OK to account for case differences.
  for (size_t i = 1; i < arraysize(kHtmlKeywordsSequencesMultiByte); ++i) {
    DCHECK(StringCaseCompare(kHtmlKeywordsSequencesMultiByte[i - 1].sequence,
                             kHtmlKeywordsSequencesMultiByte[i].sequence)
           <= 0);
  }
}

void HtmlKeywords::Init() {
  if (singleton_ == NULL) {
    singleton_ = new HtmlKeywords();
  }
}

void HtmlKeywords::ShutDown() {
  if (singleton_ != NULL) {
    delete singleton_;
    singleton_ = NULL;
  }
}

StringPiece HtmlKeywords::UnescapeHelper(const StringPiece& escaped,
                                         GoogleString* buf,
                                         bool* decoding_error) const {
  if (escaped.empty()) {
    *decoding_error = false;
    return escaped;
  }
  *decoding_error = true;

  // It would be nice to short-circuit the loop below via a memchr
  // looking for "&".  Unfortunately we must at least also scan for
  // 8-bit characters, as we cannot unescape those in a manner that's
  // bidirectionally safe.  Consider CanonicalAttributesTest.Spanish,
  // where a non-utf8 multi-byte 8-bit character is present.  If we
  // short-circuit looking for "&" we'll wind up escaping each piece
  // of the multi-byte sequence individually and that will not reverse
  // properly.
  // if (memchr(escaped.data(), '&', escaped.size()) == NULL) {
  //   return escaped;
  // }

  buf->clear();

  // Attribute values may have HTML escapes in them, e.g.
  //    href="host.com/path?v1&amp;v2"
  // Un-escape the attribute value here before populating the
  // attribute data structure.
  GoogleString escape;
  uint32 numeric_value = 0;
  bool accumulate_numeric_code = false;
  bool hex_mode = false;
  bool in_escape = false;
  bool found_ampersand = false;
  for (size_t i = 0; i < escaped.size(); ++i) {
    uint8 ch = static_cast<uint8>(escaped[i]);
    if (!in_escape) {
      if (ch == '&') {
        if (!found_ampersand) {
          found_ampersand = true;
          buf->append(escaped.data(), i);
        }
        in_escape = true;
        escape.clear();
        numeric_value = 0;
        accumulate_numeric_code = false;
        hex_mode = false;
      } else if (ch > 127) {
        return StringPiece(NULL, 0);
      } else if (found_ampersand) {
        *buf += ch;
      }
    } else if (escape.empty() && (ch == '#')) {
      escape += ch;
      accumulate_numeric_code = true;
      if (((i + 1) < escaped.size()) && (UpperChar(escaped[i + 1]) == 'X')) {
        hex_mode = true;
        ++i;
      }
    } else if (ch == ';') {
      if (!TryUnescape(accumulate_numeric_code, numeric_value, escape,
                       true, buf)) {
        return StringPiece(NULL, 0);
      }
      in_escape = false;
    } else if (ch > 127) {
      return StringPiece(NULL, 0);
    } else {
      // Accumulate an escape code based on the current mode (symbolic, decimal,
      // or hex).  If the character is not plausible to accumulate into the
      // escape sequence, then consider it improperly terminated and try to
      // look it up.
      bool improperly_terminated = false;
      if (accumulate_numeric_code) {
        improperly_terminated =
            ((hex_mode && !AccumulateHexValue(ch, &numeric_value)) ||
             (!hex_mode && !AccumulateDecimalValue(ch, &numeric_value)));
      } else if (!IsAsciiAlphaNumeric(ch)) {
        improperly_terminated = true;
      }
      if (improperly_terminated) {
        if (TryUnescape(accumulate_numeric_code, numeric_value, escape, false,
                        buf)) {
          DCHECK_LT(static_cast<size_t>(0), i);
          --i;  // re-examine ch now that we've unescaped what came before it.
          in_escape = false;
        } else {
          return StringPiece(NULL, 0);
        }
      } else {
        escape += ch;
      }
    }
  }
  if (!found_ampersand) {
    *decoding_error = false;
    return escaped;
  }
  if (in_escape) {
    if (escape.empty()) {
      buf->push_back('&');
    } else if (!TryUnescape(accumulate_numeric_code, numeric_value,
                            escape, false, buf)) {
      return StringPiece(NULL, 0);
    }
  }
  *decoding_error = false;
  return StringPiece(*buf);
}

bool HtmlKeywords::TryUnescape(bool accumulate_numeric_code,
                               uint32 numeric_value,
                               const GoogleString& escape,
                               bool was_terminated,
                               GoogleString* buf) const {
  if (accumulate_numeric_code && (escape.size() > 1)) {
    // See the table in the Description in
    // http://en.wikipedia.org/wiki/UTF-8#Codepage_layout .
    if (numeric_value <= 0xff) {
      *buf += static_cast<char>(numeric_value);
    } else {
      return false;
    }
  } else {
    // Some symbols are case-sensitive (AElig vs aelig are different
    // code-points) whereas some are case-insensitive (&quot; and
    // &QUOT; both work.  So do the case-sensitive lookup first, and
    // if that fails, do an insensitive lookup.
    StringStringMapSensitive::const_iterator p =
        unescape_sensitive_map_.find(escape);
    if (p != unescape_sensitive_map_.end()) {
      *buf += p->second;
    } else {
      // The sensitive lookup failed, but allow, for example, &QUOT; to work
      // in place of &quot;.  However, note that "yuml" is single
      // byte and "Yuml" is multi-byte, and we first look in our multi-byte
      // set before allowing doing the case-insensitive lookup in our
      // single-byte encodings list.
      if (std::binary_search(
              kHtmlKeywordsSequencesMultiByte,
              (kHtmlKeywordsSequencesMultiByte +
               arraysize(kHtmlKeywordsSequencesMultiByte)),
              escape.c_str(), HtmlMultiByteSequenceCompare())) {
        // Reject known multi-byte
        // sequences e.g. (&hellip;), as we would wind up adding an extra &amp;
        // e.g. (&amp;&hellip;).
        return false;
      } else {
        // Handle the case the user has used the wrong case for a
        // valid escape sequence, e.g. QUOT;, but there is no
        // multi-byte match (e.g. Yuml;).  We can allow sloppy
        // interpretation with a case insensitive lookup here.
        StringStringMapInsensitive::const_iterator q =
            unescape_insensitive_map_.find(escape);
        if (q != unescape_insensitive_map_.end()) {
          *buf += q->second;
        } else {
          // &apos; is a special case.  It is *not* legal HTML but many
          // web designers think it is.  It does not work on IE.  So
          // recognize that & fix it.
          // http://fishbowl.pastiche.org/2003/07/01/the_curse_of_apos/
          //
          // This is special-cased here because we want to unescape &apos;
          // into a single-quote, but do not want to re-escape ' into &apos;,
          // but instead into &#39;.  If we put that in the keyword map
          // above then we'd get the reverse mapping we don't want.
          //
          // TODO(jmarantz): consider allowing ' to &apos; for XHTML.
          if (StringCaseEqual(escape, "apos")) {
            *buf += '\'';
          } else {
            // Let random words through (e.g. accept a&b literal, which we would
            // re-escape & "improve" as a&amp;b).
            *buf += "&";
            *buf += escape;
            if (was_terminated) {
              *buf += ";";
            }
          }
        }
      }
    }
  }
  return true;
}

StringPiece HtmlKeywords::EscapeHelper(const StringPiece& unescaped,
                                       GoogleString* buf) const {
  if (unescaped.data() == NULL) {
    return unescaped;
  }
  buf->clear();

  GoogleString char_to_escape;
  for (size_t i = 0; i < unescaped.size(); ++i) {
    int ch = static_cast<unsigned char>(unescaped[i]);
    // According to http://www.htmlescape.net/htmlescape_tool.html,
    // single-quote does not need to be escaped.  However, input HTML
    // might have used single-quote to quote attribute values, in
    // which case we better escape any single-quotes in the value.
    //
    // This function, unfortunately, does not know what quoting was used.
    // TODO(jmarantz): in remove_quotes filter, switch between ' and " for
    // quoting based on whatever is in the attr value.
    if ((ch > 127) || (ch < 32) || (ch == '"') || (ch == '\'') || (ch == '&') ||
        (ch == '<') || (ch == '>')) {
      char_to_escape.clear();
      char_to_escape += ch;
      StringStringMapSensitive::const_iterator p =
          escape_map_.find(char_to_escape);
      if (p == escape_map_.end()) {
        StringAppendF(buf, "&#%02d;", static_cast<int>(ch));
      } else {
        *buf += '&';
        *buf += p->second;
        *buf += ';';
      }
    } else {
      *buf += unescaped[i];
    }
  }
  return StringPiece(*buf);
}

void HtmlKeywords::AddCrossProduct(const StringPiece& k1_list,
                                   const StringPiece& k2_list,
                                   KeywordPairVec* kmap) {
  StringPieceVector v1, v2;
  SplitStringPieceToVector(k1_list, " ", &v1, true);
  SplitStringPieceToVector(k2_list, " ", &v2, true);
  for (int i = 0, n1 = v1.size(); i < n1; ++i) {
    HtmlName::Keyword k1 = HtmlName::Lookup(v1[i]);
    DCHECK_NE(HtmlName::kNotAKeyword, k1) << v1[i];
    for (int j = 0, n2 = v2.size(); j < n2; ++j) {
      HtmlName::Keyword k2 = HtmlName::Lookup(v2[j]);
      DCHECK_NE(HtmlName::kNotAKeyword, k2) << v2[j];
      KeywordPair k1_k2 = MakeKeywordPair(k1, k2);
      kmap->push_back(k1_k2);
    }
  }
}

void HtmlKeywords::AddToSet(const StringPiece& klist, KeywordVec* kset) {
  StringPieceVector v;
  SplitStringPieceToVector(klist, " ", &v, true);
  for (int i = 0, n = v.size(); i < n; ++i) {
    HtmlName::Keyword k = HtmlName::Lookup(v[i]);
    DCHECK_NE(HtmlName::kNotAKeyword, k) << v[i];
    kset->push_back(k);
  }
}

namespace {

// Sorts the passed-in vector to enable binary_search.  The vector is
// sorted by T::operator<.  If in the future a binary search requires
// a custom comparator then this function should also be changed to
// take that comparator.
//
// vec must not be empty.
template<class T>
void PrepareForBinarySearch(std::vector<T>* vec) {
  CHECK(!vec->empty());
  std::sort(vec->begin(), vec->end());
  // Make sure there are no duplicates
#ifndef NDEBUG
  typename std::vector<T>::iterator p = std::unique(vec->begin(), vec->end());
  if (p != vec->end()) {
    T duplicate_value = *p;
    LOG(DFATAL) << "Duplicate set element " << duplicate_value;
  }
#endif
}

}  // namespace

void HtmlKeywords::InitAutoClose() {
  // The premise of our lookup machinery is that HtmlName::Keyword can
  // be represented in a 16-bit int, so that we can make a pair using
  // SHIFT+OR.
  DCHECK_EQ(HtmlName::num_keywords(), HtmlName::num_keywords() & 0xffff);

  // TODO(jmarantz): these deserve another pass through the HTML5 spec.
  // Note that http://www.w3.org/TR/html5/syntax.html#optional-tags
  // covers many of these cases, but omits the general situation that
  // formatting elements should be automatically closed when they
  // hit most other tags.
  //
  // However, there is discussion of relevance in and around:
  // http://www.w3.org/TR/html5/the-end.html#misnested-tags:-b-i-b-i

  AddAutoClose(kTableLeaves, kTableLeaves);
  AddAutoClose(kTableLeaves, "tr");
  AddAutoClose("tr", kTableSections);
  AddAutoClose("tr", "tr");
  AddAutoClose(kTableSections, kTableSections);

  AddAutoClose("p", kParagraphTerminators);

  AddAutoClose("li", "li");
  AddAutoClose("dd dt", "dd dt");
  AddAutoClose("rp rt", "rp rt");
  AddAutoClose("optgroup", "optgroup");
  AddAutoClose("option", "optgroup option");
  AddAutoClose(kFormattingElements, StrCat("tr ", kListElements,
                                           kDeclarationElements));
  PrepareForBinarySearch(&auto_close_);
}

void HtmlKeywords::InitContains() {
  // TODO(jmarantz): these deserve another pass through the HTML5 spec.  Note
  // that the HTML5 spec doesn't have a 'containment' section but there is
  // discussion of the context in which tags can reside in the doc for each
  // tag, and discussion of relevance in and around:
  // http://www.w3.org/TR/html5/the-end.html#misnested-tags:-b-i-b-i
  //
  // Also see http://www.whatwg.org/specs/web-apps/current-work
  // /multipage/syntax.html#optional-tags which describes auto-closing
  // elements whose parents have no more content.

  AddContained(kTableLeaves, "table");
  AddContained("tr", "table");
  AddContained(kTableSections, "table");
  AddContained("li", "ul ol");
  AddContained("dd dt", "dl");
  AddContained("rt rp", "ruby");
  AddContained(kFormattingElements, "td th");
  PrepareForBinarySearch(&contained_);
}

// These tags do not need to be explicitly closed, but can be.  See
// http://www.w3.org/TR/html5/syntax.html#optional-tags .  Note that
// this is *not* consistent with
// http://www.w3schools.com/tags/tag_p.asp which claims that the <p>
// tag works the same in XHTML as HTML.  This is clearly wrong since
// real XHTML has XML syntax which requires explicit closing tags.
//
// Note that we will close any of these tags without warning.
void HtmlKeywords::InitOptionallyClosedKeywords() {
  AddToSet(kFormattingElements, &optionally_closed_);
  AddToSet("body colgroup dd dt html optgroup option p", &optionally_closed_);
  AddToSet(kListElements, &optionally_closed_);
  AddToSet(kTableElements, &optionally_closed_);
  PrepareForBinarySearch(&optionally_closed_);
}

}  // namespace
