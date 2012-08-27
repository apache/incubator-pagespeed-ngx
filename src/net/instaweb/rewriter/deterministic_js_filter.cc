/*
 * Copyright 2012 Google Inc.
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

// Author: mmohabey@google.com (Megha Mohabey)

#include "net/instaweb/rewriter/public/deterministic_js_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

DeterministicJsFilter::DeterministicJsFilter(RewriteDriver* driver)
    : driver_(driver),
      found_head_(false) {
}

DeterministicJsFilter::~DeterministicJsFilter() {}

void DeterministicJsFilter::StartDocument() {
  found_head_ = false;
}

void DeterministicJsFilter::StartElement(HtmlElement* element) {
  if (!found_head_ && element->keyword() == HtmlName::kHead) {
    found_head_ = true;
    HtmlElement* script = driver_->NewElement(element, HtmlName::kScript);
    driver_->AddAttribute(script, HtmlName::kType, "text/javascript");
    script->AddAttribute(
        driver_->MakeName(HtmlName::kPagespeedNoDefer), NULL,
        HtmlElement::NO_QUOTE);
    StaticJavascriptManager* static_js_manager =
        driver_->resource_manager()->static_javascript_manager();
    StringPiece deterministic_js =
        static_js_manager->GetJsSnippet(
            StaticJavascriptManager::kDeterministicJs, driver_->options());
    HtmlCharactersNode* script_code = driver_->NewCharactersNode(script,
        deterministic_js);
    driver_->InsertElementAfterCurrent(script);
    driver_->AppendChild(script, script_code);
  }
}

}  // namespace net_instaweb
