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
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/rewriter/public/flush_early_content_writer_filter.h"
#include "net/instaweb/rewriter/public/js_defer_disabled_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/escaping.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

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
      ie_meta_tag_written_(false),
      max_prefetch_js_elements_(0) {
}

JsDisableFilter::~JsDisableFilter() {
}

void JsDisableFilter::DetermineEnabled() {
  bool should_apply = JsDeferDisabledFilter::ShouldApply(driver());
  set_is_enabled(should_apply);
  AbstractLogRecord* log_record = driver()->log_record();
  if (should_apply) {
    log_record->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kDisableJavascript),
        RewriterHtmlApplication::ACTIVE);
  } else if (!driver()->flushing_early()) {
    log_record->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kDisableJavascript),
        RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED);
  }
}

void JsDisableFilter::StartDocumentImpl() {
  index_ = 0;
  ie_meta_tag_written_ = false;
  should_look_for_prefetch_js_elements_ = false;
  prefetch_js_elements_.clear();
  prefetch_js_elements_count_ = 0;
  max_prefetch_js_elements_ =
      driver()->options()->max_prefetch_js_elements();
  prefetch_mechanism_ =
      driver()->user_agent_matcher()->GetPrefetchMechanism(
          driver()->user_agent());
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
  driver()->AddAttribute(script_node, HtmlName::kPagespeedNoDefer, "");
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
    should_look_for_prefetch_js_elements_ = true;
  } else if (element->keyword() == HtmlName::kBody) {
    if (!ie_meta_tag_written_) {
      InsertMetaTagForIE(element);
    }
    if (prefetch_js_elements_count_ != 0) {
      // We have collected some script elements that can be downloaded early.
      should_look_for_prefetch_js_elements_ = false;
      // The method to download the scripts differs based on the user agent.
      // Iframe is used for non-chrome UAs whereas for Chrome, the scripts are
      // downloaded as Image.src().
      if (prefetch_mechanism_ == UserAgentMatcher::kPrefetchImageTag) {
        HtmlElement* script = driver()->NewElement(element, HtmlName::kScript);
        driver()->AddAttribute(script, HtmlName::kPagespeedNoDefer, "");
        GoogleString script_data = StrCat("(function(){", prefetch_js_elements_,
                                          "})()");
        driver()->PrependChild(element, script);
        HtmlNode* script_code =
            driver()->NewCharactersNode(script, script_data);
        driver()->AppendChild(script, script_code);
      }
    }
  } else {
    HtmlElement::Attribute* src;
    if (script_tag_scanner_.ParseScriptElement(element, &src) ==
        ScriptTagScanner::kJavaScript) {
      if (element->FindAttribute(HtmlName::kPagespeedNoDefer)) {
        driver()->log_record()->LogJsDisableFilter(
            RewriteOptions::FilterId(RewriteOptions::kDisableJavascript), true);
        return;
      }

      // TODO(rahulbansal): Add a separate bool to track the inline
      // scripts till first external script which aren't deferred.1
      driver()->log_record()->LogJsDisableFilter(
          RewriteOptions::FilterId(RewriteOptions::kDisableJavascript), false);

      // TODO(rahulbansal): Add logging for prioritize scripts
      if (src != NULL) {
        if (should_look_for_prefetch_js_elements_ &&
            prefetch_js_elements_count_ < max_prefetch_js_elements_) {
          GoogleString escaped_source;
          if (prefetch_mechanism_ == UserAgentMatcher::kPrefetchImageTag) {
            EscapeToJsStringLiteral(src->DecodedValueOrNull(), false,
                                    &escaped_source);
            StrAppend(&prefetch_js_elements_, StringPrintf(
                      FlushEarlyContentWriterFilter::kPrefetchImageTagHtml,
                      escaped_source.c_str()));
          }
          prefetch_js_elements_count_++;
        }
      }
      HtmlElement::Attribute* type = element->FindAttribute(HtmlName::kType);
      if (type != NULL) {
        type->set_name(driver()->MakeName(HtmlName::kPagespeedOrigType));
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
      element->AddAttribute(driver()->MakeName(HtmlName::kOrigIndex),
                            IntegerToString(index_++),
                            HtmlElement::DOUBLE_QUOTE);
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
    onload->set_name(driver()->MakeName("data-pagespeed-onload"));
    driver()->AddEscapedAttribute(element, HtmlName::kOnload,
                                  kElementOnloadCode);
  }
}

void JsDisableFilter::EndElementImpl(HtmlElement* element) {
  if (element->keyword() == HtmlName::kHead) {
    should_look_for_prefetch_js_elements_ = false;
  }
}

void JsDisableFilter::EndDocument() {
  InsertJsDeferExperimentalScript();
}

}  // namespace net_instaweb
