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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_TAG_SCANNER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_TAG_SCANNER_H_

#include <vector>

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/html_parser_types.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/atom.h"
#include <string>

namespace net_instaweb {

class CssTagScanner {
 public:
  explicit CssTagScanner(HtmlParse* html_parse);

  // Examines an HTML element to determine if it's a CSS link,
  // extracting out the HREF and the media-type.
  bool ParseCssElement(
      HtmlElement* element, HtmlElement::Attribute** href, const char** media);

  // Scans the contents of a CSS file, looking for the pattern url(xxx).
  // If xxx is a relative URL, it absolutifies it based on the passed-in base
  // path.  If xxx is quoted with single-quotes or double-quotes, those are
  // retained and the URL inside is absolutified.
  static bool AbsolutifyUrls(const StringPiece& contents,
                             const std::string& base_url,
                             Writer* writer, MessageHandler* handler);

  // Does this CSS file contain @import? If so, it cannot be combined with
  // previous CSS files. This may give false-positives, but no false-negatives.
  static bool HasImport(const StringPiece& contents, MessageHandler* handler);

 private:
  Atom s_link_;
  Atom s_href_;
  Atom s_type_;
  Atom s_rel_;
  Atom s_media_;

  DISALLOW_COPY_AND_ASSIGN(CssTagScanner);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_TAG_SCANNER_H_
