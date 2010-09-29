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

// Author: sligocki@google.com (Shawn Ligocki)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_MINIFY_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_MINIFY_H_

#include "base/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace Css {
class Stylesheet;
class Import;
class Ruleset;
class Selector;
class SimpleSelector;
class SimpleSelectors;
class Declaration;
class Value;
}

namespace net_instaweb {

class MessageHandler;
class Writer;

class CssMinify {
 public:
  // Write minified Stylesheet.
  static bool Stylesheet(const Css::Stylesheet& stylesheet,
                         Writer* writer,
                         MessageHandler* handler);

 private:
  CssMinify(Writer* writer, MessageHandler* handler);
  ~CssMinify();

  void Write(const StringPiece& str);

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
  void Minify(const Css::Import& import);
  void Minify(const Css::Ruleset& ruleset);
  void Minify(const Css::Selector& selector);
  void Minify(const Css::SimpleSelectors& sselectors, bool isfirst = false);
  void Minify(const Css::SimpleSelector& sselector);
  void Minify(const Css::Declaration& declaration);
  void Minify(const Css::Value& value);

  Writer* writer_;
  MessageHandler* handler_;
  bool ok_;

  DISALLOW_COPY_AND_ASSIGN(CssMinify);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_MINIFY_H_
