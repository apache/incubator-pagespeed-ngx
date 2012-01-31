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
    : rewrite_driver_(driver),
      script_tag_scanner_(driver) {
}

JsDisableFilter::~JsDisableFilter() {
}

void JsDisableFilter::StartElement(HtmlElement* element) {
  if (!rewrite_driver_->UserAgentSupportsJsDefer()) {
    return;
  }
  HtmlElement::Attribute* src;
  if (script_tag_scanner_.ParseScriptElement(element, &src) ==
      ScriptTagScanner::kJavaScript) {
    if (src != NULL) {
      GoogleString url(src->value());
      element->AddAttribute(
          rewrite_driver_->MakeName("orig_src"), url, "\"");
      element->DeleteAttribute(HtmlName::kSrc);
    }
    HtmlElement::Attribute* type = element->FindAttribute(HtmlName::kType);
    if (type != NULL) {
      GoogleString jstype(type->value());
      element->DeleteAttribute(HtmlName::kType);
      element->AddAttribute(
          rewrite_driver_->MakeName("orig_type"), jstype, "\"");
    }
    element->AddAttribute(
        rewrite_driver_->MakeName(HtmlName::kType), "text/psajs", "\"");
  }
}

}  // namespace net_instaweb
