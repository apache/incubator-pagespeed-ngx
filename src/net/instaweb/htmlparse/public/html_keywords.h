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

#include <map>
#include "base/basictypes.h"
#include <string>
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

  // Take a raw text and escape it so it's safe for an HTML attribute,
  // e.g.    a&b --> a&amp;b
  static StringPiece Escape(const StringPiece& unescaped, std::string* buf) {
    return singleton_->EscapeHelper(unescaped, buf);
  }

  // Take escaped text and unescape it so its value can be interpreted,
  // e.g.    "http://myhost.com/p?v&amp;w"  --> "http://myhost.com/p?v&w"
  static StringPiece Unescape(const StringPiece& escaped, std::string* buf) {
    return singleton_->UnescapeHelper(escaped, buf);
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

 private:
  HtmlKeywords();
  const char* UnescapeAttributeValue();

  static HtmlKeywords* singleton_;

  StringPiece EscapeHelper(const StringPiece& unescaped,
                           std::string* buf) const;
  StringPiece UnescapeHelper(const StringPiece& escaped,
                             std::string* buf) const;

  typedef std::map<std::string, std::string,
                   StringCompareInsensitive> StringStringMapInsensitive;
  typedef std::map<std::string, std::string> StringStringMapSensitive;
  StringStringMapInsensitive unescape_insensitive_map_;
  StringStringMapSensitive unescape_sensitive_map_;
  StringStringMapSensitive escape_map_;

  DISALLOW_COPY_AND_ASSIGN(HtmlKeywords);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_KEYWORDS_H_
