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

#include "net/instaweb/rewriter/public/css_move_to_head_filter.h"

#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/statistics.h"

namespace {

// names for Statistics variables.
const char kCssElements[] = "css_elements";

} // namespace

namespace net_instaweb {

CssMoveToHeadFilter::CssMoveToHeadFilter(HtmlParse* html_parse,
                                         Statistics* statistics)
    : html_parse_(html_parse),
      css_tag_scanner_(html_parse),
      counter_(statistics->GetVariable(kCssElements)) {
}

void CssMoveToHeadFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kCssElements);
}

void CssMoveToHeadFilter::StartDocument() {
  head_element_ = NULL;
  noscript_element_ = NULL;
}

void CssMoveToHeadFilter::StartElement(HtmlElement* element) {
  if (noscript_element_ == NULL && element->keyword() == HtmlName::kNoscript) {
    noscript_element_ = element;  // Record top-level <noscript>.
  }
}

void CssMoveToHeadFilter::EndElement(HtmlElement* element) {
  if ((head_element_ == NULL) && (element->keyword() == HtmlName::kHead)) {
    head_element_ = element;

  } else if (element == noscript_element_) {
    noscript_element_ = NULL; // We are exitting the top level </noscript>.

  // Do not move anything out of a <noscript> element and we can only move
  // this to <head> if <head> is still rewritable.
  } else if (noscript_element_ == NULL && head_element_ != NULL &&
             html_parse_->IsRewritable(head_element_)){
    HtmlElement::Attribute* href;
    const char* media;
    if ((element->keyword() == HtmlName::kStyle) ||
        css_tag_scanner_.ParseCssElement(element, &href, &media)) {
      html_parse_->MoveCurrentInto(head_element_);
      // TODO(sligocki): It'd be nice to have this pattern simplified.
      counter_->Add(1);
    }
  }
}

}  // namespace net_instaweb
