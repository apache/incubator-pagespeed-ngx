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

// Author: sligocki@google.com (Shawn Ligocki)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_MINIFY_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_MINIFY_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace Css {
class Stylesheet;
class Charsets;
class Import;
class Ruleset;
class Selector;
class SimpleSelector;
class SimpleSelectors;
class Declaration;
class Declarations;
class Value;
class FunctionParameters;
}

class UnicodeText;

namespace net_instaweb {

class GoogleUrl;
class RewriteDriver;
class MessageHandler;
class Writer;

class CssMinify {
 public:
  // Write minified Stylesheet.
  static bool Stylesheet(const Css::Stylesheet& stylesheet,
                         Writer* writer,
                         MessageHandler* handler);

  // Write minified Declarations (style attribute contents).
  static bool Declarations(const Css::Declarations& declarations,
                           Writer* writer,
                           MessageHandler* handler);

  // Absolutify all relative URLs in the stylesheet's imports using the given
  // base URL. The Import structures are modified in-situ. Returns true if any
  // URLs were absolutified, false if not.
  static bool AbsolutifyImports(Css::Stylesheet* stylesheet,
                                const GoogleUrl& base);

  // Absolutify all relative URLs in the stylesheet using the given base URL.
  // The Declaration structures are modified in-situ. You can control whether
  // URLs in parseable sections (BACKGROUND, BACKGROUND_IMAGE, LIST_STYLE,
  // LIST_STYLE_IMAGE) and/or unparseable sections (UNPARSEABLE) are handled.
  // Returns true if any URLs were absolutified, false if not.
  static bool AbsolutifyUrls(Css::Stylesheet* stylesheet,
                             const GoogleUrl& base,
                             bool handle_parseable_sections,
                             bool handle_unparseable_sections,
                             RewriteDriver* driver,
                             MessageHandler* handler);

  // Escape [() \t\r\n\\'"].  Also escape , for non-URLs.  Escaping , in
  // URLs causes IE8 to interpret the backslash as a forward slash.
  static GoogleString EscapeString(const StringPiece& src, bool in_url);

 private:
  CssMinify(Writer* writer, MessageHandler* handler);
  ~CssMinify();

  void Write(const StringPiece& str);

  void WriteURL(const UnicodeText& url);

  template<typename Container>
  void JoinMinify(const Container& container, const StringPiece& sep);
  template<typename Iterator>
  void JoinMinifyIter(const Iterator& begin, const Iterator& end,
                      const StringPiece& sep);
  template<typename Container>
  void JoinMediaMinify(const Container& container, const StringPiece& sep);

  // We name all of these methods identically to simplify the writing of the
  // templated Join* methods.
  void Minify(const Css::Stylesheet& stylesheet);
  void Minify(const Css::Charsets& charsets);
  void Minify(const Css::Import& import);
  void Minify(const Css::Selector& selector);
  void Minify(const Css::SimpleSelectors& sselectors, bool isfirst = false);
  void Minify(const Css::SimpleSelector& sselector);
  void Minify(const Css::Declaration& declaration);
  void Minify(const Css::Value& value);
  void Minify(const Css::FunctionParameters& parameters);

  // Specializations for Ruleset to handle common @media rules.
  // Start followed by Ignoring followed by End gives the same result as the
  // Ruleset version of Minify above.

  // Emits the ruleset's selectors and declarations without wrapping them in
  // an @media rule.
  void MinifyRulesetIgnoringMedia(const Css::Ruleset& ruleset);
  // Emits the start of the @media rule iff required (non-empty media set).
  void MinifyRulesetMediaStart(const Css::Ruleset& ruleset);
  // Emits the end of the @media rule iff required (non-empty media set).
  void MinifyRulesetMediaEnd(const Css::Ruleset& ruleset);

  Writer* writer_;
  MessageHandler* handler_;
  bool ok_;

  DISALLOW_COPY_AND_ASSIGN(CssMinify);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_MINIFY_H_
