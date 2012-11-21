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
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

BlinkBackgroundFilter::BlinkBackgroundFilter(
    RewriteDriver* rewrite_driver)
    : rewrite_driver_(rewrite_driver),
      rewrite_options_(rewrite_driver->options()),
      script_tag_scanner_(rewrite_driver) {
}

BlinkBackgroundFilter::~BlinkBackgroundFilter() {}

void BlinkBackgroundFilter::StartElement(HtmlElement* element) {
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
}

}   // namespace net_instaweb
