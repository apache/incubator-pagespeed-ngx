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

#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/rewriter/public/js_defer_disabled_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/escaping.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"
#include "pagespeed/opt/logging/enums.pb.h"

namespace net_instaweb {

const char JsDisableFilter::kEnableJsExperimental[] =
    "window.pagespeed = window.pagespeed || {};"
    "window.pagespeed.defer_js_experimental=true;";
const char JsDisableFilter::kElementOnloadCode[] =
    "var elem=this;"
    "if (this==window) elem=document.body;"
    "elem.setAttribute('data-pagespeed-loaded', 1)";

JsDisableFilter::JsDisableFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      script_tag_scanner_(driver),
      index_(0),
      ie_meta_tag_written_(false) {
}

JsDisableFilter::~JsDisableFilter() {
}

void JsDisableFilter::DetermineEnabled(GoogleString* disabled_reason) {
  bool should_apply = JsDeferDisabledFilter::ShouldApply(driver());
  set_is_enabled(should_apply);
  AbstractLogRecord* log_record = driver()->log_record();
  if (should_apply) {
    log_record->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kDisableJavascript),
        RewriterHtmlApplication::ACTIVE);
  } else {
    log_record->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kDisableJavascript),
        RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED);
  }
}

void JsDisableFilter::StartDocumentImpl() {
  index_ = 0;
  ie_meta_tag_written_ = false;
}

void JsDisableFilter::InsertJsDeferExperimentalScript() {
  bool defer_js_experimental =
      driver()->options()->enable_defer_js_experimental();
  if (!defer_js_experimental) {
    return;
  }
  // We are not adding this code in js_defer_disabled_filter to avoid
  // duplication of code for blink and critical line code.
  HtmlElement* script_node =
      driver()->NewElement(NULL, HtmlName::kScript);

  driver()->AddAttribute(script_node, HtmlName::kType, "text/javascript");
  driver()->AddAttribute(script_node, HtmlName::kDataPagespeedNoDefer,
                         StringPiece());
  HtmlNode* script_code =
      driver()->NewCharactersNode(script_node, kEnableJsExperimental);
  InsertNodeAtBodyEnd(script_node);
  driver()->AppendChild(script_node, script_code);
}

void JsDisableFilter::InsertMetaTagForIE(HtmlElement* element) {
  if (ie_meta_tag_written_) {
    return;
  }
  ie_meta_tag_written_ = true;
  if (!driver()->user_agent_matcher()->IsIe(driver()->user_agent())) {
    return;
  }

  HtmlElement* head_node = element;
  if (element->keyword() != HtmlName::kHead) {
    head_node =
        driver()->NewElement(element->parent(), HtmlName::kHead);
    driver()->InsertNodeBeforeCurrent(head_node);
  }
  // TODO(ksimbili): Don't add the following if there is already a meta tag
  // and if it's content is greater than IE8 (deferJs supported version).
  HtmlElement* meta_tag =
      driver()->NewElement(head_node, HtmlName::kMeta);

  driver()->AddAttribute(meta_tag, HtmlName::kHttpEquiv, "X-UA-Compatible");
  driver()->AddAttribute(meta_tag, HtmlName::kContent, "IE=edge");
  driver()->PrependChild(head_node, meta_tag);
}

void JsDisableFilter::StartElementImpl(HtmlElement* element) {
  if (element->keyword() == HtmlName::kHead) {
    if (!ie_meta_tag_written_) {
      InsertMetaTagForIE(element);
    }
  } else if (element->keyword() == HtmlName::kBody) {
    if (!ie_meta_tag_written_) {
      InsertMetaTagForIE(element);
    }
  } else {
    HtmlElement::Attribute* src;
    if (script_tag_scanner_.ParseScriptElement(element, &src) ==
        ScriptTagScanner::kJavaScript) {
      if (element->FindAttribute(HtmlName::kDataPagespeedNoDefer) ||
          element->FindAttribute(HtmlName::kPagespeedNoDefer)) {
        driver()->log_record()->LogJsDisableFilter(
            RewriteOptions::FilterId(RewriteOptions::kDisableJavascript), true);
        return;
      }

      // Honor disallow.
      if (src != NULL && src->DecodedValueOrNull() != NULL) {
        GoogleUrl abs_url(driver()->base_url(), src->DecodedValueOrNull());
        if (abs_url.IsWebValid() &&
            !driver()->options()->IsAllowed(abs_url.Spec())) {
          driver()->log_record()->LogJsDisableFilter(
              RewriteOptions::FilterId(RewriteOptions::kDisableJavascript),
              true);
          return;
        }
      }

      // TODO(rahulbansal): Add a separate bool to track the inline
      // scripts till first external script which aren't deferred.1
      driver()->log_record()->LogJsDisableFilter(
          RewriteOptions::FilterId(RewriteOptions::kDisableJavascript), false);

      // TODO(rahulbansal): Add logging for prioritize scripts
      HtmlElement::Attribute* type = element->FindAttribute(HtmlName::kType);
      if (type != NULL) {
        type->set_name(driver()->MakeName(HtmlName::kDataPagespeedOrigType));
      }
      // Delete all type attributes if any. Some sites have more than one type
      // attribute(duplicate). Chrome and firefox picks up the first type
      // attribute for the node.
      while (element->DeleteAttribute(HtmlName::kType)) {}
      HtmlElement::Attribute* prioritize_attr = element->FindAttribute(
          HtmlName::kDataPagespeedPrioritize);
      if (prioritize_attr != NULL &&
          driver()->options()->enable_prioritizing_scripts()) {
        element->AddAttribute(
            driver()->MakeName(HtmlName::kType), "text/prioritypsajs",
            HtmlElement::DOUBLE_QUOTE);
      } else {
        element->AddAttribute(
            driver()->MakeName(HtmlName::kType), "text/psajs",
            HtmlElement::DOUBLE_QUOTE);
      }
      element->AddAttribute(
          driver()->MakeName(HtmlName::kDataPagespeedOrigIndex),
          IntegerToString(index_++), HtmlElement::DOUBLE_QUOTE);
    }
  }

  HtmlElement::Attribute* onload = element->FindAttribute(HtmlName::kOnload);
  if (onload != NULL) {
    // The onload value can be any script. It's not necessary that it is
    // always javascript. But we don't have any way of identifying it.
    // For now let us assume it is JS, which is the case in majority.
    // TODO(ksimbili): Try fixing not adding non-Js code, if we can.
    // TODO(ksimbili): Call onloads on elements in the same order as they are
    // triggered.
    // See the test file js_defer_onload_in_html.html
    onload->set_name(driver()->MakeName("data-pagespeed-onload"));
    driver()->AddEscapedAttribute(element, HtmlName::kOnload,
                                  kElementOnloadCode);
    // TODO(sligocki): Should we add onerror handler here too?
  }
}

void JsDisableFilter::EndElementImpl(HtmlElement* element) {
}

void JsDisableFilter::EndDocument() {
  InsertJsDeferExperimentalScript();
}

}  // namespace net_instaweb
