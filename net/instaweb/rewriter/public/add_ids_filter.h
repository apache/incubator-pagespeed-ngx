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
// Author: jmaessen@google.com (Jan-Willem Maessen)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_ADD_IDS_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_ADD_IDS_FILTER_H_

#include <vector>

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/html/empty_html_filter.h"
#include "pagespeed/kernel/html/html_element.h"

namespace net_instaweb {

// This filter as it stands adds an id to all div-like DOM elements that lack
// one.  The ids represent the div's location in the DOM, based on the tag
// structure of the page.  The hope is that this is moderately stable between
// page accesses.
//
// Ids are of the form PageSpeed-nearestParentId-n-n-n... where -n-n-n is a path
// encoded as a series of indexes at that depth below nearestParentId.  For
// example:
// PageSpeed-7-0 : The 0th child of the 7th child of the root of the document
// PageSpeed-content-11: The 11th child of the node with id='content'.
class AddIdsFilter : public EmptyHtmlFilter {
 public:
  static const char kIdPrefix[];

  explicit AddIdsFilter(RewriteDriver* driver);
  virtual ~AddIdsFilter();

  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);
  virtual const char* Name() const { return "AddIdsFilter"; }

 private:
  GoogleString GetDivCountStackEncoding();

  // We represent our current DOM location with two stacks.  The
  // div_count_stack_ contains our path through the divs in the DOM.  When a div
  // has an id, kIsId is pushed immediately after its position, and the id is
  // pushed onto id_stack_.  So if we erase all kIsId entries, we obtain a pure
  // path through the tree; to create an encoded id we use the top entry of
  // id_stack_ followed by the encoding of the topmost elements of
  // div_count_stack_ above the topmost kIsId.
  static const int kIsId;
  std::vector<int> div_count_stack_;
  std::vector<const HtmlElement::Attribute*> id_stack_;
  RewriteDriver* driver_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_ADD_IDS_FILTER_H_
