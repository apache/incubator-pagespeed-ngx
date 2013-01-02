/*
 * Copyright 2011 Google Inc.
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

// Author: jhoch@google.com (Jason R. Hoch)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DIV_STRUCTURE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DIV_STRUCTURE_FILTER_H_

#include <vector>
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class HtmlElement;

// This filter as it stands adds to all anchor href's a special query parameter,
// unique for each link, representing vaguely the link's location on a page,
// based on the div structure of the page.
//
// In its current simple/functional form, the query parameters are of the form
// "0.1.0.3", a sort of series of DOM-coordinates of a DOM restricted to <div>
// and <a> elements.  This example could be the 4th link in the first div
// of the second div of the first main div, or the 2nd link (following 2 divs)
// in the first div in the first div (following one link) in the first top-level
// div.
//
// TODO(jhoch): Next step is to encode/condense these parameter values (at the
// very least use a base higher than 10).
class DivStructureFilter : public EmptyHtmlFilter {
 public:
  static const char kParamName[];

  explicit DivStructureFilter();
  virtual ~DivStructureFilter();

  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);
  virtual const char* Name() const { return "DivStructureFilter"; }

  static GoogleString GetDivCountStackEncoding(
      const std::vector<int>& div_count_stack);

 private:
  std::vector<int> div_count_stack_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DIV_STRUCTURE_FILTER_H_
