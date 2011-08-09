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

#include <cstddef>
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/statistics.h"

namespace net_instaweb {

ScanFilter::ScanFilter(RewriteDriver* driver)
    : driver_(driver),
      tag_scanner_(driver) {
  tag_scanner_.set_find_a_tags(true);
}

ScanFilter::~ScanFilter() {
}

void ScanFilter::StartDocument() {
  // TODO(jmarantz): consider having rewrite_driver access the url in this
  // class, rather than poking it into rewrite_driver.
  seen_refs_ = false;
  seen_base_ = false;
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
      seen_base_ = true;
      if (seen_refs_) {
        driver_->set_refs_before_base();
      }
    }
    // TODO(jmarantz): handle base targets in addition to hrefs.
  } else if (!seen_refs_ && !seen_base_ &&
             tag_scanner_.ScanElement(element) != NULL) {
    seen_refs_ = true;
  }
}

void ScanFilter::Flush() {
  driver_->resource_manager()->num_flushes()->Add(1);
}

}  // namespace net_instaweb
