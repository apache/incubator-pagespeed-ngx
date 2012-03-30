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
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

const char JsDisableFilter::kEnableJsExperimental[] =
    "if (window.localStorage) {"
    "  window.localStorage[\'defer_js_experimental\'] = \'1\';"
    "}";
const char JsDisableFilter::kDisableJsExperimental[] =
    "if (window.localStorage &&"
    "    window.localStorage[\'defer_js_experimental\']) {"
    "  window.localStorage.removeItem(\'defer_js_experimental\');"
    "}";

JsDisableFilter::JsDisableFilter(RewriteDriver* driver)
    : rewrite_driver_(driver),
      script_tag_scanner_(driver),
      index_(0),
      defer_js_experimental_script_written_(false) {
}

JsDisableFilter::~JsDisableFilter() {
}

void JsDisableFilter::StartDocument() {
  index_ = 0;
  defer_js_experimental_script_written_ = false;
}

void JsDisableFilter::StartElement(HtmlElement* element) {
  if (!rewrite_driver_->UserAgentSupportsJsDefer()) {
    return;
  }

  if (element->keyword() == HtmlName::kBody &&
      !defer_js_experimental_script_written_) {
    // We are not adding this code in js_defer_disabled_filter to avoid
    // duplication of code for blink and critical line code.
    bool defer_js_experimental =
        rewrite_driver_->options()->enable_defer_js_experimental();
    HtmlElement* script_node =
        rewrite_driver_->NewElement(element, HtmlName::kScript);

    rewrite_driver_->AddAttribute(script_node, HtmlName::kType,
                                  "text/javascript");
    rewrite_driver_->AddAttribute(script_node, HtmlName::kPagespeedNoDefer, "");
    HtmlNode* script_code =
        rewrite_driver_->NewCharactersNode(
            script_node,
            (defer_js_experimental ?
             JsDisableFilter::kEnableJsExperimental :
             JsDisableFilter::kDisableJsExperimental));
    rewrite_driver_->PrependChild(element, script_node);
    rewrite_driver_->AppendChild(script_node, script_code);
    defer_js_experimental_script_written_ = true;
  } else {
    HtmlElement::Attribute* src;
    if (script_tag_scanner_.ParseScriptElement(element, &src) ==
        ScriptTagScanner::kJavaScript) {
      if (element->FindAttribute(HtmlName::kPagespeedNoDefer)) {
        return;
      }
      if ((src != NULL) && (src->DecodedValueOrNull() != NULL)) {
        GoogleString url(src->DecodedValueOrNull());
        element->AddAttribute(
            rewrite_driver_->MakeName("orig_src"), url, "\"");
        element->DeleteAttribute(HtmlName::kSrc);
      }
      HtmlElement::Attribute* type = element->FindAttribute(HtmlName::kType);
      if ((type != NULL) && (type->DecodedValueOrNull() != NULL)) {
        GoogleString jstype(type->DecodedValueOrNull());
        element->DeleteAttribute(HtmlName::kType);
        element->AddAttribute(
            rewrite_driver_->MakeName("orig_type"), jstype, "\"");
      }
      element->AddAttribute(
          rewrite_driver_->MakeName(HtmlName::kType), "text/psajs", "\"");
      element->AddAttribute(rewrite_driver_->MakeName("orig_index"),
                            IntegerToString(index_++), "\"");
    }
  }
}

}  // namespace net_instaweb
