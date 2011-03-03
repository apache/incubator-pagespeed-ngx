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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/scan_filter.h"
#include "net/instaweb/rewriter/public/common_filter.h"

namespace net_instaweb {

ScanFilter::~ScanFilter() {
}

void ScanFilter::StartDocument() {
  // TODO(jmarantz): consider having rewrite_driver access the url in this
  // class, rather than poking it into rewrite_driver.
  driver_->InitBaseUrl();
}

void ScanFilter::StartElement(HtmlElement* element) {
  // <base>
  if (element->keyword() == HtmlName::kBase) {
    HtmlElement::Attribute* href = element->FindAttribute(HtmlName::kHref);
    // See http://www.whatwg.org/specs/web-apps/current-work/multipage
    // /semantics.html#the-base-element
    if (href != NULL) {
      // TODO(jmarantz): consider having rewrite_driver access the url in this
      // class, rather than poking it into rewrite_driver.
      driver_->SetBaseUrlIfUnset(href->value());
    }
    // TODO(jmarantz): handle base targets in addition to hrefs.
  }
}

}  // namespace net_instaweb
