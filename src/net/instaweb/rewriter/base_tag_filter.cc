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

#include "public/base_tag_filter.h"

#include "public/add_head_filter.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include <string>

namespace net_instaweb {
BaseTagFilter::BaseTagFilter(HtmlParse* html_parse) {
  found_head_ = false;
  html_parse_ = html_parse;
}

void BaseTagFilter::StartDocument() {
  found_head_ = false;
}

// In a proxy server, we will want to set a base tag according to the current
// URL being processed.  But we need to add the BaseTagFilter upstream of
// the HtmlWriterFilter, so we'll need to establish it at init time before
// we know a URL.  So in that mode, where we've installed the filter but
// have no specific URL to set the base tag to, then we should avoid
// adding an empty base tag.
void BaseTagFilter::StartElement(HtmlElement* element) {
  if ((element->keyword() == HtmlName::kHead) && !found_head_) {
    found_head_ = true;
    HtmlElement* new_element =
        html_parse_->NewElement(element, HtmlName::kBase);
    new_element->set_close_style(HtmlElement::IMPLICIT_CLOSE);
    html_parse_->AddAttribute(new_element, HtmlName::kHref, base_url_);
    html_parse_->InsertElementAfterCurrent(new_element);
  } else if (element->keyword() == HtmlName::kBase) {
    // There is was a pre-existing base tag.  See if it specifies an href.
    // If so, delete it, as it's now superseded by the one we added above.
    for (int i = 0; i < element->attribute_size(); ++i) {
      HtmlElement::Attribute& attribute = element->attribute(i);
      if (attribute.keyword() == HtmlName::kHref) {
        html_parse_->DeleteElement(element);
        break;
      }
    }
  }
}

}  // namespace net_instaweb
