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

#include "net/instaweb/rewriter/public/common_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"

namespace net_instaweb {

CommonFilter::CommonFilter(HtmlParse* html_parse)
    : html_parse_(html_parse),
      s_base_(html_parse->Intern("base")),
      s_href_(html_parse->Intern("href")),
      s_noscript_(html_parse->Intern("noscript")) {}

CommonFilter::~CommonFilter() {}

void CommonFilter::StartDocument() {
  // Base URL starts as document URL.
  base_gurl_ = html_parse_->gurl();
  noscript_element_ = NULL;
  // Run the actual filter's StartDocumentImpl.
  StartDocumentImpl();
}

void CommonFilter::StartElement(HtmlElement* element) {
  // <base>
  if (element->tag() == s_base_) {
    HtmlElement::Attribute* href = element->FindAttribute(s_href_);
    if (href != NULL) {
      GURL temp_url(href->value());
      if (temp_url.is_valid()) {
        base_gurl_.Swap(&temp_url);
      }
    }

  // <noscript>
  } else if (element->tag() == s_noscript_) {
    if (noscript_element_ == NULL) {
      noscript_element_ = element;  // Record top-level <noscript>
    }
  }

  // Run actual filter's StartElementImpl
  StartElementImpl(element);
}

void CommonFilter::EndElement(HtmlElement* element) {
  if (element == noscript_element_) {
    noscript_element_ = NULL;  // We are exitting the top-level <noscript>
  }

  // Run actual filter's EndElementImpl
  EndElementImpl(element);
}

}  // namespace net_instaweb
