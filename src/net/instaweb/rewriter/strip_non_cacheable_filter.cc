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

// Author: rahulbansal@google.com (Rahul Bansal)

#include <vector>

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/strip_non_cacheable_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

StripNonCacheableFilter::StripNonCacheableFilter(
    RewriteDriver* rewrite_driver)
    : rewrite_driver_(rewrite_driver),
      rewrite_options_(rewrite_driver->options()),
      script_tag_scanner_(rewrite_driver),
      script_written_(false) {
}

StripNonCacheableFilter::~StripNonCacheableFilter() {}

void StripNonCacheableFilter::StartDocument() {
  BlinkUtil::PopulateAttributeToNonCacheableValuesMap(
      rewrite_options_, rewrite_driver_->google_url(),
      &attribute_non_cacheable_values_map_, &panel_number_num_instances_);
  script_written_ = false;
}

void StripNonCacheableFilter::StartElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kBody && !script_written_) {
    InsertBlinkJavascript(element);
  }

  HtmlElement::Attribute* src;
  if (script_tag_scanner_.ParseScriptElement(element, &src) ==
      ScriptTagScanner::kJavaScript) {
    if (!element->FindAttribute(HtmlName::kPagespeedNoDefer)) {
      LOG(DFATAL) << "Script which is not deferred is found!!!";
    }
  }

  int panel_number = BlinkUtil::GetPanelNumberForNonCacheableElement(
      attribute_non_cacheable_values_map_, element);
  if (panel_number != -1) {
    GoogleString panel_id = BlinkUtil::GetPanelId(
        panel_number, panel_number_num_instances_[panel_number]);
    panel_number_num_instances_[panel_number]++;
    InsertPanelStub(element, panel_id);
    rewrite_driver_->DeleteElement(element);
  }

  if (element->keyword() == HtmlName::kBody) {
    HtmlCharactersNode* comment = rewrite_driver_->NewCharactersNode(
        element, BlinkUtil::kStartBodyMarker);
    rewrite_driver_->PrependChild(element, comment);
  }
  // Webkit output escapes the contents of noscript tags on the page. This
  // breaks the functionality of the noscript tags. Removing them from the
  // page since in case javascript is turned off, we anyway redirect
  // the user to the page with blink disabled.
  if (element->keyword() == HtmlName::kNoscript) {
    rewrite_driver_->DeleteElement(element);
  }
}

void StripNonCacheableFilter::EndElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kHead && !script_written_) {
    InsertBlinkJavascript(element);
  }
}

void StripNonCacheableFilter::InsertBlinkJavascript(HtmlElement* element) {
  HtmlElement* head_node =
      (element->keyword() != HtmlName::kHead) ? NULL : element;
  if (!head_node) {
    head_node = rewrite_driver_->NewElement(element, HtmlName::kHead);
    rewrite_driver_->InsertElementBeforeElement(element, head_node);
  }

  // insert blink.js .
  HtmlElement* script_node =
      rewrite_driver_->NewElement(element, HtmlName::kScript);

  rewrite_driver_->AddAttribute(script_node, HtmlName::kType,
                                "text/javascript");
  rewrite_driver_->AddAttribute(script_node, HtmlName::kPagespeedNoDefer, "");
  StaticJavascriptManager* js_manager =
      rewrite_driver_->resource_manager()->static_javascript_manager();
  rewrite_driver_->AddAttribute(script_node, HtmlName::kSrc,
                                js_manager->GetBlinkJsUrl(rewrite_options_));
  rewrite_driver_->AppendChild(head_node, script_node);

  // insert <script>pagespeed.deferInit();</script> .
  script_node = rewrite_driver_->NewElement(element, HtmlName::kScript);
  rewrite_driver_->AddAttribute(script_node, HtmlName::kType,
                                "text/javascript");
  rewrite_driver_->AddAttribute(script_node, HtmlName::kPagespeedNoDefer, "");

  HtmlNode* script_code =
      rewrite_driver_->NewCharactersNode(script_node, "pagespeed.deferInit();");

  rewrite_driver_->AppendChild(head_node, script_node);
  rewrite_driver_->AppendChild(script_node, script_code);

  script_written_ = true;
}

void StripNonCacheableFilter::InsertPanelStub(HtmlElement* element,
                                              const GoogleString& panel_id) {
  HtmlCommentNode* comment = rewrite_driver_->NewCommentNode(
      element->parent(),
      StrCat(RewriteOptions::kPanelCommentPrefix, " begin ", panel_id));
  rewrite_driver_->InsertElementBeforeCurrent(comment);
  // Append end stub to json.
  comment = rewrite_driver_->NewCommentNode(
      element->parent(),
      StrCat(RewriteOptions::kPanelCommentPrefix, " end ", panel_id));
  rewrite_driver_->InsertElementBeforeCurrent(comment);
}

}   // namespace net_instaweb
