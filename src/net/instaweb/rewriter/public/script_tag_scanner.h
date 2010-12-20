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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_SCRIPT_TAG_SCANNER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_SCRIPT_TAG_SCANNER_H_

#include <set>

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/atom.h"

namespace net_instaweb {
class HtmlParse;

class ScriptTagScanner {
 public:
  enum ScriptClassification {
    kNonScript,
    kUnknownScript,
    kJavaScript
  };

  // Bit flags that specify when the script is to be run
  enum ExecutionModeFlags {
    kExecuteSync = 0,
    kExecuteDefer = 1,
    kExecuteAsync = 2,
    kExecuteForEvent = 4 // IE extension. If this is set,
                         // script will not run in browsers following HTML5,
                         // and will run at hard-to-describe time in IE.
  };

  explicit ScriptTagScanner(HtmlParse* html_parse);

  // Examines an HTML element and determine if it is a script.
  // If it's not, it returns kNonScript and doesn't touch *src.
  // If it is a script, it returns whether it is JavaScript or not,
  // and sets *src to the src attribute (perhaps NULL)
  ScriptClassification ParseScriptElement(HtmlElement* element,
                                          HtmlElement::Attribute** src);

  // Returns which execution model attributes are set.
  // Keep in mind, however, that HTML5 browsers will ignore
  // kExecuteDefer and kExecuteAsync on elements without src=''
  int ExecutionMode(const HtmlElement* element) const;
 private:
  // Normalizes the input str by trimming whitespace and lowercasing.
  static std::string Normalized(const StringPiece& str);

  bool IsJsMime(const std::string& type_str);

  const Atom s_async_;
  const Atom s_defer_;
  const Atom s_event_;
  const Atom s_for_;
  const Atom s_language_;
  const Atom s_script_;
  const Atom s_src_;
  const Atom s_type_;
  std::set<std::string> javascript_mimetypes_;

  DISALLOW_COPY_AND_ASSIGN(ScriptTagScanner);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SCRIPT_TAG_SCANNER_H_
