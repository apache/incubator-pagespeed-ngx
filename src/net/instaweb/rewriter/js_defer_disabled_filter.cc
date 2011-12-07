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

// Author: atulvasu@google.com (Atul Vasu)

#include "net/instaweb/rewriter/public/js_defer_disabled_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

extern const char* JS_js_defer;

// TODO(atulvasu): Minify this script if minify is turned on.
const char* JsDeferDisabledFilter::kDeferJsCode = JS_js_defer;

JsDeferDisabledFilter::JsDeferDisabledFilter(HtmlParse* html_parse)
    : html_parse_(html_parse) {
}

JsDeferDisabledFilter::~JsDeferDisabledFilter() { }

void JsDeferDisabledFilter::StartDocument() {
  script_written_ = false;
}


void JsDeferDisabledFilter::EndElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kBody && !script_written_) {
    HtmlElement* script_node =
        html_parse_->NewElement(element, HtmlName::kScript);
    html_parse_->AddAttribute(script_node, HtmlName::kType,
                              "text/javascript");
    GoogleString defer_js = StrCat(kDeferJsCode, "\n"
        "pagespeed.deferInit();\n"
        "pagespeed.deferJs.registerNoScriptTags();\n"
        "pagespeed.addOnload(window, function() {\n"
        "  pagespeed.deferJs.run();\n"
        "});\n");
    HtmlNode* script_code =
        html_parse_->NewCharactersNode(script_node, defer_js);
    html_parse_->InsertElementBeforeCurrent(script_node);
    html_parse_->AppendChild(script_node, script_code);
    script_written_ = true;
  }
}

void JsDeferDisabledFilter::EndDocument() {
  if (!script_written_) {
    // Scripts never get executed if this happen.
    html_parse_->ErrorHere("BODY tag didn't close after last script");
    // TODO(atulvasu): Try to write here.
  }
}

}  // namespace net_instaweb
