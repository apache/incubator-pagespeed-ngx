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

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string_util.h"

namespace Css {
class Stylesheet;
class Charsets;
class Import;
class MediaQuery;
class MediaQueries;
class MediaExpression;
class Ruleset;
class Selector;
class SimpleSelector;
class SimpleSelectors;
class Declaration;
class Declarations;
class Value;
class Values;
class FunctionParameters;
class UnparsedRegion;
}  // namespace Css

class UnicodeText;

namespace net_instaweb {

class MessageHandler;
class Writer;

// TODO(nikhilmadan): Move to pagespeed/kernel/css/.
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

  // We name all of these methods identically to simplify the writing of the
  // templated Join* methods.
  void Minify(const Css::Stylesheet& stylesheet);
  void Minify(const Css::Charsets& charsets);
  void Minify(const Css::Import& import);
  void Minify(const Css::MediaQuery& media_query);
  void Minify(const Css::MediaExpression& expression);
  void Minify(const Css::Selector& selector);
  void Minify(const Css::SimpleSelectors& sselectors, bool isfirst = false);
  void Minify(const Css::SimpleSelector& sselector);
  void Minify(const Css::Declaration& declaration);
  void Minify(const Css::Value& value);
  void Minify(const Css::FunctionParameters& parameters);
  void Minify(const Css::UnparsedRegion& unparsed_region);

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

  // Font requires special output format.
  void MinifyFont(const Css::Values& font_values);

  bool Equals(const Css::MediaQueries& a, const Css::MediaQueries& b) const;
  bool Equals(const Css::MediaQuery& a, const Css::MediaQuery& b) const;
  bool Equals(const Css::MediaExpression& a,
              const Css::MediaExpression& b) const;

  Writer* writer_;
  MessageHandler* handler_;
  bool ok_;

  DISALLOW_COPY_AND_ASSIGN(CssMinify);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_MINIFY_H_
