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

#include "public/add_head_filter.h"

#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_element.h"

namespace net_instaweb {

AddHeadFilter::AddHeadFilter(HtmlParse* html_parse, bool combine_multiple_heads)
    : html_parse_(html_parse),
      combine_multiple_heads_(combine_multiple_heads),
      found_head_(false),
      s_head_(html_parse->Intern("head")),
      s_body_(html_parse->Intern("body")),
      head_element_(NULL) {
}

void AddHeadFilter::StartDocument() {
  found_head_ = false;
  head_element_ = NULL;
}

void AddHeadFilter::StartElement(HtmlElement* element) {
  if (!found_head_) {
    if (element->tag() == s_body_) {
      head_element_ = html_parse_->NewElement(
          element->parent(), s_head_);
      html_parse_->InsertElementBeforeElement(element, head_element_);
      found_head_ = true;
    } else if (element->tag() == s_head_) {
      found_head_ = true;
      head_element_ = element;
    }
  }
}

void AddHeadFilter::EndElement(HtmlElement* element) {
  // Detect elements that are children of a subsequent head.  Move them
  // into the first head if possible.
  if (combine_multiple_heads_ && found_head_ && (head_element_ != NULL) &&
      html_parse_->IsRewritable(head_element_)) {
    if ((element->tag() == s_head_) && (element != head_element_)) {
      // There should be no elements left in the subsequent head, so
      // remove it.
      bool moved = html_parse_->MoveCurrentInto(head_element_);
      CHECK(moved) << "failed to move new head under old head";
      bool deleted = html_parse_->DeleteSavingChildren(element);
      CHECK(deleted) << "failed to delete extra head";
    }
  }
}

void AddHeadFilter::Flush() {
  head_element_ = NULL;
}

void AddHeadFilter::EndDocument() {
  if (!found_head_) {
    // In order to insert a <head> in a docucument that lacks one, we
    // must first find the body.  If we get through the whole doc without
    // finding a <head> or a <body> then this filter will have failed to
    // add a head.
    html_parse_->InfoHere("Reached end of document without finding <body>");
  }
}

}  // namespace net_instaweb
