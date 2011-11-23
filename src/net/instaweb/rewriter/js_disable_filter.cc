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

// Author: gagansingh@google.com (Gagan Singh)

#include "net/instaweb/rewriter/public/js_disable_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"

namespace net_instaweb {

const char kDisabledAttribute[] = "psa_disabled";

JsDisableFilter::JsDisableFilter(RewriteDriver* driver)
    : rewrite_driver_(driver) {
}

JsDisableFilter::~JsDisableFilter() {
}

void JsDisableFilter::StartElement(HtmlElement* element) {
  // TODO(gagansingh): Make sure Flush does not happen before this element's end
  // event is reached.
}

void JsDisableFilter::EndElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kScript) {
    HtmlElement* noscript = rewrite_driver_->NewElement(
        element->parent(), HtmlName::kNoscript);
    rewrite_driver_->InsertElementBeforeElement(element, noscript);

    HtmlName attr = rewrite_driver_->MakeName(kDisabledAttribute);
    noscript->AddAttribute(attr, "true", "\"");

    if (!rewrite_driver_->MoveCurrentInto(noscript)) {
      LOG(DFATAL) << "Could not move";
    }
  }
}

}  // namespace net_instaweb
