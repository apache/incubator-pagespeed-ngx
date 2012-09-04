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

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/blink_background_filter.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

BlinkBackgroundFilter::BlinkBackgroundFilter(
    RewriteDriver* rewrite_driver)
    : rewrite_driver_(rewrite_driver),
      rewrite_options_(rewrite_driver->options()),
      script_tag_scanner_(rewrite_driver),
      script_written_(false) {
}

BlinkBackgroundFilter::~BlinkBackgroundFilter() {}

void BlinkBackgroundFilter::StartDocument() {
  script_written_ = false;
}

void BlinkBackgroundFilter::StartElement(HtmlElement* element) {
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
  // We currently serve rewritten HTML using UTF-8 and indicate it in a
  // response header - if there is a "content-type" META tag that specifies a
  // charset, delete it.
  // TODO(rmathew): Remove this when we start returning content in the
  // original charset.
  if (element->keyword() == HtmlName::kMeta) {
    GoogleString content, mime_type, charset;
    if (CommonFilter::ExtractMetaTagDetails(*element, NULL, &content,
                                            &mime_type, &charset)) {
      if (!charset.empty()) {
        rewrite_driver_->DeleteElement(element);
      }
    }
  }
}

void BlinkBackgroundFilter::EndElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kHead && !script_written_) {
    InsertBlinkJavascript(element);
  }
}

void BlinkBackgroundFilter::InsertBlinkJavascript(HtmlElement* element) {
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
      rewrite_driver_->server_context()->static_javascript_manager();
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

}   // namespace net_instaweb
