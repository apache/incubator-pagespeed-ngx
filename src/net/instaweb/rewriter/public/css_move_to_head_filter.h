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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_MOVE_TO_HEAD_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_MOVE_TO_HEAD_FILTER_H_

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/util/public/atom.h"

namespace net_instaweb {

class Statistics;
class Variable;

class CssMoveToHeadFilter : public EmptyHtmlFilter {
 public:
  CssMoveToHeadFilter(HtmlParse* html_parse, Statistics* statistics);

  static void Initialize(Statistics* statistics);
  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);
  virtual const char* Name() const { return "CssMoveToHead"; }

 private:
  Atom s_head_;
  Atom s_noscript_;
  Atom s_style_;

  HtmlParse* html_parse_;
  HtmlElement* head_element_;
  HtmlElement* noscript_element_;
  CssTagScanner css_tag_scanner_;
  Variable* counter_;

  DISALLOW_COPY_AND_ASSIGN(CssMoveToHeadFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_MOVE_TO_HEAD_FILTER_H_
