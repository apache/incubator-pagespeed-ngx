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

// Author: jkarlin@google.com (Josh Karlin)

#include "net/instaweb/rewriter/public/pedantic_filter.h"

#include "net/instaweb/htmlparse/public/doctype.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

PedanticFilter::PedanticFilter(HtmlParse* html_parse)
    : html_parse_(html_parse), script_scanner_(html_parse) {
}

PedanticFilter::~PedanticFilter() {}

void PedanticFilter::StartElement(HtmlElement* element) {
  const DocType& doctype = html_parse_->doctype();

  // If it's a <style> tag with no type then replace it with
  // <style type="script/css">. This is necessary for HTML4 but not HTML5.
  // http://www.w3.org/TR/html4/present/styles.html#edef-STYLE
  // http://www.w3.org/TR/html5/the-style-element.html#attr-style-type
  if (!doctype.IsVersion5() && element->keyword() == HtmlName::kStyle) {
    HtmlElement::Attribute* type_attr = element->FindAttribute(HtmlName::kType);
    if (type_attr == NULL) {
      // We have a style tag without a type attribute, add one.
      html_parse_->AddAttribute(element, HtmlName::kType, "text/css");
    }
  }

  // If it's a <script> tag with no type or language, replace it with
  // <script type="text/javascript">. This is necessary for HTML4 but not HTML5.
  // http://www.w3.org/TR/html4/interact/scripts.html#adef-type-SCRIPT
  // http://www.w3.org/TR/html5/the-script-element.html#attr-script-type
  if (!doctype.IsVersion5() && element->keyword() == HtmlName::kScript) {
    HtmlElement::Attribute* type_attr = element->FindAttribute(HtmlName::kType);

    if (type_attr == NULL) {
      // No type and no language attributes, let's double check with
      // ScriptTagScanner that it thinks we're looking at javascript.

      HtmlElement::Attribute* src = NULL;
      ScriptTagScanner::ScriptClassification classification =
          script_scanner_.ParseScriptElement(element, &src);
      if (classification == ScriptTagScanner::kJavaScript) {
        html_parse_->AddAttribute(element, HtmlName::kType,
                                  "text/javascript");
      }
    }
  }
}

}  // namespace net_instaweb
