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
  driver_->InitBaseUrl();
  seen_refs_ = false;
  seen_base_ = false;

  for (int i = 0, n = filters_.size(); i < n; ++i) {
    filters_[i]->ScanStartDocument();
  }
}

void ScanFilter::EndDocument() {
  for (int i = 0, n = filters_.size(); i < n; ++i) {
    filters_[i]->ScanEndDocument();
  }
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

  for (int i = 0, n = filters_.size(); i < n; ++i) {
    filters_[i]->ScanStartElement(element);
  }
}

void ScanFilter::EndElement(HtmlElement* element) {
  for (int i = 0, n = filters_.size(); i < n; ++i) {
    filters_[i]->ScanEndElement(element);
  }
}

void ScanFilter::Cdata(HtmlCdataNode* cdata) {
  for (int i = 0, n = filters_.size(); i < n; ++i) {
    filters_[i]->ScanCdata(cdata);
  }
}

void ScanFilter::Comment(HtmlCommentNode* comment) {
  for (int i = 0, n = filters_.size(); i < n; ++i) {
    filters_[i]->ScanComment(comment);
  }
}

void ScanFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  for (int i = 0, n = filters_.size(); i < n; ++i) {
    filters_[i]->ScanIEDirective(directive);
  }
}

void ScanFilter::Characters(HtmlCharactersNode* characters) {
  for (int i = 0, n = filters_.size(); i < n; ++i) {
    filters_[i]->ScanCharacters(characters);
  }
}

void ScanFilter::Directive(HtmlDirectiveNode* directive) {
  for (int i = 0, n = filters_.size(); i < n; ++i) {
    filters_[i]->ScanDirective(directive);
  }
}

void ScanFilter::Flush() {
}

}  // namespace net_instaweb
