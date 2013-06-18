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
#include "net/instaweb/htmlparse/public/html_keywords.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/rewriter/public/flush_early_content_writer_filter.h"
#include "net/instaweb/rewriter/public/js_defer_disabled_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/util/public/escaping.h"
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
const char JsDisableFilter::kElementOnloadCode[] =
    "var elem=this;"
    "if (this==window) elem=document.body;"
    "elem.setAttribute('data-pagespeed-loaded', 1)";

JsDisableFilter::JsDisableFilter(RewriteDriver* driver)
    : rewrite_driver_(driver),
      script_tag_scanner_(driver),
      index_(0),
      defer_js_experimental_script_written_(false),
      ie_meta_tag_written_(false),
      max_prefetch_js_elements_(0) {
}

JsDisableFilter::~JsDisableFilter() {
}

void JsDisableFilter::DetermineEnabled() {
  bool should_apply = JsDeferDisabledFilter::ShouldApply(rewrite_driver_);
  set_is_enabled(should_apply);
  AbstractLogRecord* log_record = rewrite_driver_->log_record();
  if (should_apply) {
    log_record->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kDisableJavascript),
        RewriterHtmlApplication::ACTIVE);
  } else if (!rewrite_driver_->flushing_early()) {
    log_record->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kDisableJavascript),
        RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED);
  }
}

void JsDisableFilter::StartDocument() {
  index_ = 0;
  defer_js_experimental_script_written_ = false;
  ie_meta_tag_written_ = false;
  should_look_for_prefetch_js_elements_ = false;
  prefetch_js_elements_.clear();
  prefetch_js_elements_count_ = 0;
  max_prefetch_js_elements_ =
      rewrite_driver_->options()->max_prefetch_js_elements();
  prefetch_mechanism_ =
      rewrite_driver_->user_agent_matcher()->GetPrefetchMechanism(
          rewrite_driver_->user_agent());
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
          script_node, GetJsDisableScriptSnippet(rewrite_driver_->options()));
  rewrite_driver_->AppendChild(element, script_node);
  rewrite_driver_->AppendChild(script_node, script_code);
  defer_js_experimental_script_written_ = true;
}

void JsDisableFilter::InsertMetaTagForIE(HtmlElement* element) {
  if (ie_meta_tag_written_) {
    return;
  }
  ie_meta_tag_written_ = true;
  if (!rewrite_driver_->user_agent_matcher()->IsIe(
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
  rewrite_driver_->PrependChild(element, meta_tag);
}

void JsDisableFilter::StartElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kHead) {
    if (!ie_meta_tag_written_) {
      InsertMetaTagForIE(element);
    }
    should_look_for_prefetch_js_elements_ = true;
  } else if (element->keyword() == HtmlName::kBody) {
    if (!defer_js_experimental_script_written_) {
      HtmlElement* head_node =
          rewrite_driver_->NewElement(element->parent(), HtmlName::kHead);
      rewrite_driver_->InsertNodeBeforeCurrent(head_node);
      InsertJsDeferExperimentalScript(head_node);
      InsertMetaTagForIE(head_node);
    }
    if (prefetch_js_elements_count_ != 0) {
      // We have collected some script elements that can be downloaded early.
      should_look_for_prefetch_js_elements_ = false;
      // The method to download the scripts differs based on the user agent.
      // Iframe is used for non-chrome UAs whereas for Chrome, the scripts are
      // downloaded as Image.src().
      if (prefetch_mechanism_ == UserAgentMatcher::kPrefetchImageTag) {
        HtmlElement* script = rewrite_driver_->NewElement(element,
            HtmlName::kScript);
        rewrite_driver_->AddAttribute(script, HtmlName::kPagespeedNoDefer,
                                      "");
        GoogleString script_data = StrCat("(function(){", prefetch_js_elements_,
                                          "})()");
        rewrite_driver_->PrependChild(element, script);
        HtmlNode* script_code = rewrite_driver_->NewCharactersNode(
            script, script_data);
        rewrite_driver_->AppendChild(script, script_code);
      } else {
        HtmlElement* iframe =
            rewrite_driver_->NewElement(element, HtmlName::kIframe);
        GoogleString encoded_uri;
        DataUrl(kContentTypeHtml, BASE64, prefetch_js_elements_, &encoded_uri);
        rewrite_driver_->AddAttribute(iframe, HtmlName::kSrc, encoded_uri);
        rewrite_driver_->AddAttribute(iframe, HtmlName::kClass,
                                      "psa_prefetch_container");
        rewrite_driver_->AddAttribute(iframe, HtmlName::kStyle, "display:none");
        rewrite_driver_->PrependChild(element, iframe);
      }
    }
  } else {
    HtmlElement::Attribute* src;
    if (script_tag_scanner_.ParseScriptElement(element, &src) ==
        ScriptTagScanner::kJavaScript) {
      if (element->FindAttribute(HtmlName::kPagespeedNoDefer)) {
        rewrite_driver_->log_record()->LogJsDisableFilter(
            RewriteOptions::FilterId(RewriteOptions::kDisableJavascript), true);
        return;
      }

      // TODO(rahulbansal): Add a separate bool to track the inline
      // scripts till first external script which aren't deferred.1
      rewrite_driver_->log_record()->LogJsDisableFilter(
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
          } else {
            HtmlKeywords::Escape(src->DecodedValueOrNull(), &escaped_source);
            StrAppend(&prefetch_js_elements_, StringPrintf(
                      FlushEarlyContentWriterFilter::kPrefetchScriptTagHtml,
                      escaped_source.c_str()));
          }
          prefetch_js_elements_count_++;
        }
        src->set_name(rewrite_driver_->MakeName(HtmlName::kPagespeedOrigSrc));
      }
      HtmlElement::Attribute* type = element->FindAttribute(HtmlName::kType);
      if (type != NULL) {
        type->set_name(rewrite_driver_->MakeName(HtmlName::kPagespeedOrigType));
      }
      // Delete all type attributes if any. Some sites have more than one type
      // attribute(duplicate). Chrome and firefox picks up the first type
      // attribute for the node.
      while (element->DeleteAttribute(HtmlName::kType)) {}
      HtmlElement::Attribute* prioritize_attr = element->FindAttribute(
          HtmlName::kDataPagespeedPrioritize);
      if (prioritize_attr != NULL &&
          rewrite_driver_->options()->enable_prioritizing_scripts()) {
        element->AddAttribute(
            rewrite_driver_->MakeName(HtmlName::kType), "text/prioritypsajs",
            HtmlElement::DOUBLE_QUOTE);
      } else {
        element->AddAttribute(
            rewrite_driver_->MakeName(HtmlName::kType), "text/psajs",
            HtmlElement::DOUBLE_QUOTE);
      }
      element->AddAttribute(
          rewrite_driver_->MakeName("orig_index"), IntegerToString(index_++),
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
    onload->set_name(rewrite_driver_->MakeName("data-pagespeed-onload"));
    rewrite_driver_->AddEscapedAttribute(element, HtmlName::kOnload,
                                         kElementOnloadCode);
  }
}

void JsDisableFilter::EndElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kHead &&
      !defer_js_experimental_script_written_) {
    InsertJsDeferExperimentalScript(element);
  }
  if (element->keyword() == HtmlName::kHead) {
    should_look_for_prefetch_js_elements_ = false;
  }
}

void JsDisableFilter::EndDocument() {
  if (!defer_js_experimental_script_written_) {
    rewrite_driver_->InfoHere("Experimental flag code is not written");
  }
}

GoogleString JsDisableFilter::GetJsDisableScriptSnippet(
    const RewriteOptions* options) {
  bool defer_js_experimental = options->enable_defer_js_experimental();
  return defer_js_experimental ? JsDisableFilter::kEnableJsExperimental :
      JsDisableFilter::kDisableJsExperimental;
}

}  // namespace net_instaweb
