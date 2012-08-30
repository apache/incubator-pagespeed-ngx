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

// Author: atulvasu@google.com (Atul Vasu)
//         sriharis@google.com (Srihari Sukumaran)

#include "net/instaweb/rewriter/public/detect_reflow_js_defer_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/string_util.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/util/public/null_message_handler.h"

namespace net_instaweb {

DetectReflowJsDeferFilter::DetectReflowJsDeferFilter(RewriteDriver* driver)
    : rewrite_driver_(driver),
      script_written_(false),
      defer_js_enabled_(false),
      debug_(driver->options()->Enabled(RewriteOptions::kDebug)) {
}

DetectReflowJsDeferFilter::~DetectReflowJsDeferFilter() { }

void DetectReflowJsDeferFilter::StartDocument() {
  script_written_ = false;
  defer_js_enabled_ = rewrite_driver_->UserAgentSupportsJsDefer();
}

void DetectReflowJsDeferFilter::StartElement(HtmlElement* element) {
  if (defer_js_enabled_ && element->keyword() == HtmlName::kBody &&
      !script_written_) {
    HtmlElement* head_node =
        rewrite_driver_->NewElement(element->parent(), HtmlName::kHead);
    rewrite_driver_->InsertElementBeforeCurrent(head_node);
    InsertDetectReflowCode(head_node);
  }
}

void DetectReflowJsDeferFilter::EndElement(HtmlElement* element) {
  if (defer_js_enabled_ && element->keyword() == HtmlName::kHead &&
      !script_written_) {
    InsertDetectReflowCode(element);
  }
}

void DetectReflowJsDeferFilter::InsertDetectReflowCode(HtmlElement* element) {
  StaticJavascriptManager* static_js_manager =
      rewrite_driver_->server_context()->static_javascript_manager();
  // Detect reflow functions script node.
  HtmlElement* script_node = rewrite_driver_->NewElement(element,
                                                         HtmlName::kScript);
  rewrite_driver_->AddAttribute(script_node, HtmlName::kType,
                                "text/javascript");
  rewrite_driver_->AddAttribute(script_node, HtmlName::kPagespeedNoDefer, "");
  StringPiece detect_reflow_script = static_js_manager->GetJsSnippet(
      StaticJavascriptManager::kDetectReflowJs, rewrite_driver_->options());
  HtmlNode* script_code = rewrite_driver_->NewCharactersNode(
      script_node, detect_reflow_script);
  rewrite_driver_->AppendChild(element, script_node);
  rewrite_driver_->AppendChild(script_node, script_code);
  script_written_ = true;
}

void DetectReflowJsDeferFilter::EndDocument() {
  if (defer_js_enabled_ && !script_written_) {
    // Scripts never get executed if this happen.
    rewrite_driver_->InfoHere("BODY tag didn't close after last script");
    // TODO(atulvasu): Try to write here.
  }
}

}  // namespace net_instaweb
