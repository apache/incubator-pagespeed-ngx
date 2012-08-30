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
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/util/public/null_message_handler.h"

namespace net_instaweb {

const char JsDeferDisabledFilter::kSuffix[] =
      "\npagespeed.deferInit();\n"
      "pagespeed.deferJsStarted = false;\n"
      "var startDeferJs = function() {\n"
      "  if (pagespeed.deferJsStarted) return;\n"
      "  pagespeed.deferJsStarted = true;\n"
      "  pagespeed.deferJs.registerScriptTags();\n"
      "  pagespeed.deferJs.execute();\n"
      "}\n"
      "pagespeed.addHandler(document, 'DOMContentLoaded', startDeferJs);\n"
      "pagespeed.addOnload(window, startDeferJs);\n";

JsDeferDisabledFilter::JsDeferDisabledFilter(RewriteDriver* driver)
    : rewrite_driver_(driver),
      script_written_(false),
      defer_js_enabled_(false),
      debug_(driver->options()->Enabled(RewriteOptions::kDebug)) {
}

JsDeferDisabledFilter::~JsDeferDisabledFilter() { }

void JsDeferDisabledFilter::StartDocument() {
  script_written_ = false;
  defer_js_enabled_ = rewrite_driver_->UserAgentSupportsJsDefer();
}

void JsDeferDisabledFilter::StartElement(HtmlElement* element) {
  if (defer_js_enabled_ && element->keyword() == HtmlName::kBody &&
      !script_written_) {
    HtmlElement* head_node =
        rewrite_driver_->NewElement(element->parent(), HtmlName::kHead);
    rewrite_driver_->InsertElementBeforeCurrent(head_node);
    InsertJsDeferCode(head_node);
  }
}

void JsDeferDisabledFilter::EndElement(HtmlElement* element) {
  if (defer_js_enabled_ && element->keyword() == HtmlName::kHead &&
      !script_written_) {
    InsertJsDeferCode(element);
  }
}

void JsDeferDisabledFilter::InsertJsDeferCode(HtmlElement* element) {
  HtmlElement* script_node =
      rewrite_driver_->NewElement(element, HtmlName::kScript);
  rewrite_driver_->AddAttribute(script_node, HtmlName::kType,
                                "text/javascript");
  StaticJavascriptManager* static_js_manager =
      rewrite_driver_->server_context()->static_javascript_manager();
  StringPiece defer_js_script =
      static_js_manager->GetJsSnippet(
          StaticJavascriptManager::kDeferJs, rewrite_driver_->options());
  const GoogleString& defer_js =
      StrCat(defer_js_script, JsDeferDisabledFilter::kSuffix);
  HtmlNode* script_code =
      rewrite_driver_->NewCharactersNode(script_node, defer_js);
  rewrite_driver_->AppendChild(element, script_node);
  rewrite_driver_->AppendChild(script_node, script_code);
  script_written_ = true;
}

void JsDeferDisabledFilter::EndDocument() {
  if (defer_js_enabled_ && !script_written_) {
    // Scripts never get executed if this happen.
    rewrite_driver_->InfoHere("HEAD tag didn't close or no BODY tag found");
    // TODO(atulvasu): Try to write here.
  }
}

}  // namespace net_instaweb
