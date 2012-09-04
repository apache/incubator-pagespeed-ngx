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
#include "net/instaweb/http/public/user_agent_matcher.h"
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
      defer_js_experimental_script_written_(false),
      defer_js_enabled_(false),
      body_element_(NULL) {
}

JsDisableFilter::~JsDisableFilter() {
}

void JsDisableFilter::StartDocument() {
  index_ = 0;
  defer_js_experimental_script_written_ = false;
  defer_js_enabled_ = rewrite_driver_->UserAgentSupportsJsDefer();
  defer_js_experimental_ =
      rewrite_driver_->options()->enable_defer_js_experimental();
  prefetch_scripts_.clear();
  body_element_ = NULL;
}

void JsDisableFilter::InsertJsDeferExperimentalScript(HtmlElement* element) {
  // We are not adding this code in js_defer_disabled_filter to avoid
  // duplication of code for blink and critical line code.
  HtmlElement* script_node =
      rewrite_driver_->NewElement(element, HtmlName::kScript);

  rewrite_driver_->AddAttribute(script_node, HtmlName::kType,
                                "text/javascript");
  rewrite_driver_->AddAttribute(script_node, HtmlName::kPagespeedNoDefer, "");
  HtmlNode* script_code =
      rewrite_driver_->NewCharactersNode(
          script_node,
          (defer_js_experimental_ ?
           JsDisableFilter::kEnableJsExperimental :
           JsDisableFilter::kDisableJsExperimental));
  rewrite_driver_->AppendChild(element, script_node);
  rewrite_driver_->AppendChild(script_node, script_code);
  defer_js_experimental_script_written_ = true;
}

void JsDisableFilter::InsertPrefetchScriptsContainer(HtmlElement* element) {
  // The following code works in webkit based browsers only when look ahead
  // parser finds these scripts.
  // Client side code will try to prefetch them again for webkit based browsers.
  HtmlElement* prefetch_scripts_container = rewrite_driver_->NewElement(
      element, HtmlName::kDiv);
  rewrite_driver_->AddAttribute(prefetch_scripts_container,
                                HtmlName::kClass,
                                "psa_prefetch_container");
  rewrite_driver_->AppendChild(element, prefetch_scripts_container);
  for (int i = 0, n = prefetch_scripts_.size(); i < n; ++i) {
    rewrite_driver_->AppendChild(prefetch_scripts_container,
                                 prefetch_scripts_[i]);
  }
}

void JsDisableFilter::InsertMetaTagForIE(HtmlElement* element) {
  if (!rewrite_driver_->options()->override_ie_document_mode() ||
      !rewrite_driver_->user_agent_matcher().IsIe(
          rewrite_driver_->user_agent())) {
    return;
  }
  // TODO(ksimbili): Don't add the following if there is already a meta tag
  // and if it's content is greater than IE8 (deferJs supported version).
  HtmlElement* meta_tag =
      rewrite_driver_->NewElement(element, HtmlName::kMeta);

  rewrite_driver_->AddAttribute(meta_tag, HtmlName::kHttpEquiv,
                                "X-UA-Compatible");
  rewrite_driver_->AddAttribute(meta_tag, HtmlName::kContent, "IE=edge");
  rewrite_driver_->AppendChild(element, meta_tag);
}

void JsDisableFilter::StartElement(HtmlElement* element) {
  if (!defer_js_enabled_) {
    return;
  }

  if (element->keyword() == HtmlName::kBody) {
    if (body_element_ == NULL) {
      body_element_ = element;
    }
    if (!defer_js_experimental_script_written_) {
      HtmlElement* head_node =
          rewrite_driver_->NewElement(element->parent(), HtmlName::kHead);
      rewrite_driver_->InsertElementBeforeCurrent(head_node);
      InsertJsDeferExperimentalScript(head_node);
      InsertMetaTagForIE(head_node);
    }
  } else {
    HtmlElement::Attribute* src;
    if (script_tag_scanner_.ParseScriptElement(element, &src) ==
        ScriptTagScanner::kJavaScript) {
      if (element->FindAttribute(HtmlName::kPagespeedNoDefer)) {
        return;
      }
      if (src != NULL) {
        if (src->escaped_value() != NULL) {
          // Clone node for prefetching the resource.
          HtmlElement* clone = rewrite_driver_->NewElement(NULL,
                                                           HtmlName::kScript);
          clone->AddAttribute(*src);
          clone->AddAttribute(
              rewrite_driver_->MakeName(HtmlName::kType), "psa_prefetch",
              HtmlElement::DOUBLE_QUOTE);
          prefetch_scripts_.push_back(clone);
        }
        // Disable Execution of script in the original element.
        src->set_name(rewrite_driver_->MakeName(HtmlName::kPagespeedOrigSrc));
      } else if (index_ == 0 &&
                 rewrite_driver_->options()->Enabled(
                     RewriteOptions::kDeferJavascript)) {
        return;
      }
      HtmlElement::Attribute* type = element->FindAttribute(HtmlName::kType);
      if (type != NULL) {
        type->set_name(rewrite_driver_->MakeName(HtmlName::kPagespeedOrigType));
      }
      element->AddAttribute(
          rewrite_driver_->MakeName(HtmlName::kType), "text/psajs",
          HtmlElement::DOUBLE_QUOTE);
      element->AddAttribute(
          rewrite_driver_->MakeName("orig_index"), IntegerToString(index_++),
          HtmlElement::DOUBLE_QUOTE);
    }
  }

  HtmlElement::Attribute* onload = element->FindAttribute(HtmlName::kOnload);
  if ((onload != NULL) && (onload->DecodedValueOrNull() != NULL)) {
    // The onload value can be any script. It's not necessary that it is
    // always javascript. But we don't have any way of identifying it.
    // For now let us assume it is JS, which is the case in majority.
    // TODO(ksimbili): Try fixing not adding non-Js code, if we can.
    GoogleString deferred_onload = StrCat(
        "pagespeed.deferJs.addOnloadListeners(this, function() {",
        onload->DecodedValueOrNull(),
        "});");
    onload->SetValue(deferred_onload);
  }
}

void JsDisableFilter::EndElement(HtmlElement* element) {
  if (defer_js_enabled_ && element->keyword() == HtmlName::kHead &&
      !defer_js_experimental_script_written_) {
    InsertJsDeferExperimentalScript(element);
    InsertMetaTagForIE(element);
  }

  // Add the prefetch container if either of following conditions is met.
  // If we found the end body tag we were tracking.
  // if we reached end of html with scripts after body tag/or no body tag.
  if (body_element_ == element ||
      (body_element_ == NULL && element->parent() == NULL)) {
    if (prefetch_scripts_.size() > 0) {
      InsertPrefetchScriptsContainer(element);
      prefetch_scripts_.clear();
    }
    body_element_ = NULL;
  }
}

void JsDisableFilter::EndDocument() {
  if (defer_js_enabled_ && !defer_js_experimental_script_written_) {
    rewrite_driver_->InfoHere("Experimental flag code is not written");
  }
}

}  // namespace net_instaweb
