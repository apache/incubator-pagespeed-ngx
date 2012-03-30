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

#ifndef NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_KEYWORDS_H_
#define NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_KEYWORDS_H_

#include <algorithm>
#include <map>
#include <vector>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class HtmlKeywords {
 public:
  // Initialize a singleton instance of this class.  This call is
  // inherently thread unsafe, but only the first time it is called.
  // If multi-threaded programs call this function before spawning
  // threads then there will be no races.
  static void Init();

  // Tear down the singleton instance of this class, freeing any
  // allocated memory. This call is inherently thread unsafe.
  static void ShutDown();

  // Returns an HTML keyword as a string, or NULL if not a keyword.
  static const char* KeywordToString(HtmlName::Keyword keyword) {
    return singleton_->keyword_vector_[keyword];
  }

  // Take a raw text and escape it so it's safe for an HTML attribute,
  // e.g.    a&b --> a&amp;b
  static StringPiece Escape(const StringPiece& unescaped, GoogleString* buf) {
    return singleton_->EscapeHelper(unescaped, buf);
  }

  // Take escaped text and unescape it so its value can be interpreted,
  // e.g.    "http://myhost.com/p?v&amp;w"  --> "http://myhost.com/p?v&w"
  //
  // *decoding_error is set to true if the escaped string could not be
  // safely transformed into a simple stream of bytes.
  //
  // TODO(jmarantz): Support a variant where we unescape to UTF-8.
  static StringPiece Unescape(const StringPiece& escaped, GoogleString* buf,
                              bool* decoding_error) {
    return singleton_->UnescapeHelper(escaped, buf, decoding_error);
  }

  // Note that Escape and Unescape are not guaranteed to be inverses of
  // one another.  For example, Unescape("&#26;")=="&", but Escape("&")="&amp;".
  // However, note that Unescape(Escape(s)) == s.
  //
  // Another case to be wary of is when the argument to Unescape is not
  // properly escaped.  The result will be that the string is returned
  // unmodified.  For example, Unescape("a&b")=="a&b", butthen re-escaping
  // that will give "a&amp;b".  Hence, the careful maintainer of an HTML
  // parsing and rewriting system will need to maintain the original escaped
  // text parsed from HTML files, and pass that to browsers.

  // Determines whether an open tag of type k1 should be automatically closed
  // if a StartElement for tag k2 is encountered.  E.g. <tr><tbody> should
  // be transformed to <tr></tr><tbody>.
  static bool IsAutoClose(HtmlName::Keyword k1, HtmlName::Keyword k2) {
    return std::binary_search(singleton_->auto_close_.begin(),
                              singleton_->auto_close_.end(),
                              MakeKeywordPair(k1, k2));
  }

  // Determines whether an open tag of type k1 should be automatically closed
  // if an EndElement for tag k2 is encountered.  E.g. <tbody></table> should
  // be transformed into <tbody></tbody></table>.
  static bool IsContained(HtmlName::Keyword k1, HtmlName::Keyword k2) {
    return std::binary_search(singleton_->contained_.begin(),
                              singleton_->contained_.end(),
                              MakeKeywordPair(k1, k2));
  }

  // Determines whether the specified HTML keyword is closed automatically
  // by the parser if the close-tag is omitted.  E.g. <head> must be closed,
  // but formatting elements such as <p> do not need to be closed.  Also note
  // the distinction with tags which are *implicitly* closed in HTML such as
  // <img> and <br>.
  static bool IsOptionallyClosedTag(HtmlName::Keyword keyword) {
    return std::binary_search(singleton_->optionally_closed_.begin(),
                              singleton_->optionally_closed_.end(),
                              keyword);
  }

 private:
  typedef int32 KeywordPair;  // Encoded via shift & OR.
  typedef std::vector<KeywordPair> KeywordPairVec;
  typedef std::vector<HtmlName::Keyword> KeywordVec;

  HtmlKeywords();
  const char* UnescapeAttributeValue();
  void InitEscapeSequences();
  void InitAutoClose();
  void InitContains();
  void InitOptionallyClosedKeywords();

  // Translate the escape sequence and append the corresponding character
  // into *buf.
  //
  // accumulate_numeric_code==true means that the sequence has been accumulated
  // into numeric_value and that will be used to form a character for appending
  // to *buf.
  //
  // accumulate_numeric_code==false means that the sequence is in 'escape' and
  // that will be looked up in the keyword tables to get the character to append
  // to *buf.
  //
  // was_terminated indicates that the escape-sequence was properly terminated
  // by a semicolon.  This affects handling of unknown escape sequences, where
  // we will need to retain the ";".
  //
  // Returns false iff the escape-sequence is a valid multi-byte sequence,
  // which we can't currently represent in our 8-bit format.
  bool TryUnescape(bool accumulate_numeric_code,
                   uint32 numeric_value,
                   const GoogleString& escape,
                   bool was_terminated,
                   GoogleString* buf) const;

  // Encodes two keyword enums as a KeywordPair, represented as an int32.
  static KeywordPair MakeKeywordPair(HtmlName::Keyword k1,
                                     HtmlName::Keyword k2) {
    return (static_cast<KeywordPair>(k1) << 16) | static_cast<KeywordPair>(k2);
  }

  // Adds all combinations of the members of k1_list and k2_list to
  // kmap.  The lists are represented as space-delimited keywords.
  // E.g. if k1_list="a b" and k2_list="c d", then this adds (a,c),
  // (b,c), (a,d), (b,d) to kmap.
  void AddCrossProduct(const StringPiece& k1_list, const StringPiece& k2_list,
                       KeywordPairVec* kmap);
  void AddAutoClose(const StringPiece& k1_list, const StringPiece& k2_list) {
    AddCrossProduct(k1_list, k2_list, &auto_close_);
  }
  void AddContained(const StringPiece& k1_list, const StringPiece& k2_list) {
    AddCrossProduct(k1_list, k2_list, &contained_);
  }

  // Adds every space-delimited token in klist to kset.
  void AddToSet(const StringPiece& klist, KeywordVec* kset);

  static HtmlKeywords* singleton_;

  StringPiece EscapeHelper(const StringPiece& unescaped,
                           GoogleString* buf) const;
  StringPiece UnescapeHelper(const StringPiece& escaped,
                             GoogleString* buf,
                             bool* decoding_error) const;

  typedef std::map<GoogleString, GoogleString,
                   StringCompareInsensitive> StringStringMapInsensitive;
  typedef std::map<GoogleString, GoogleString> StringStringMapSensitive;
  StringStringMapInsensitive unescape_insensitive_map_;
  StringStringMapSensitive unescape_sensitive_map_;
  StringStringMapSensitive escape_map_;
  CharStarVector keyword_vector_;

  // These vectors of KeywordPair and Keyword are sorted numerically during
  // construction to enable binary-search during parsing.
  KeywordPairVec auto_close_;
  KeywordPairVec contained_;
  KeywordVec optionally_closed_;

  DISALLOW_COPY_AND_ASSIGN(HtmlKeywords);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_KEYWORDS_H_
