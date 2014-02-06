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

#include "net/instaweb/rewriter/public/add_head_filter.h"

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_parse.h"

namespace net_instaweb {

AddHeadFilter::AddHeadFilter(HtmlParse* html_parse, bool combine_multiple_heads)
    : html_parse_(html_parse),
      combine_multiple_heads_(combine_multiple_heads),
      found_head_(false),
      head_element_(NULL) {
}

AddHeadFilter::~AddHeadFilter() {}

void AddHeadFilter::StartDocument() {
  found_head_ = false;
  head_element_ = NULL;
}

void AddHeadFilter::StartElement(HtmlElement* element) {
  if (!found_head_) {
    if (element->keyword() == HtmlName::kHead) {
      found_head_ = true;
      head_element_ = element;
    } else if (element->keyword() != HtmlName::kHtml) {
      // Add head before first non-head non-html element (if no head found yet).
      head_element_ = html_parse_->NewElement(
          element->parent(), HtmlName::kHead);
      html_parse_->InsertNodeBeforeNode(element, head_element_);
      found_head_ = true;
    }
  }
}

void AddHeadFilter::EndElement(HtmlElement* element) {
  if (combine_multiple_heads_ &&
      (element->keyword() == HtmlName::kHead) && (element != head_element_) &&
      (head_element_ != NULL) && html_parse_->IsRewritable(head_element_)) {
    // Combine heads
    if (!(html_parse_->MoveCurrentInto(head_element_) &&
          html_parse_->DeleteSavingChildren(element))) {
      LOG(DFATAL) << "Failed to move or delete head in " << html_parse_->url();
    }
  }
}

void AddHeadFilter::Flush() {
  // Cannot combine heads across flush, so we NULL the pointer.
  head_element_ = NULL;
}

void AddHeadFilter::EndDocument() {
  if (!found_head_) {
    // Degenerate case: page contains no elements (or only <html> elements).
    head_element_ = html_parse_->NewElement(NULL, HtmlName::kHead);
    html_parse_->InsertNodeBeforeCurrent(head_element_);
    found_head_ = true;
  }
}

}  // namespace net_instaweb
