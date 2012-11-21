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

const char JsDeferDisabledFilter::kIsJsDeferScriptInsertedPropertyName[] =
    "is_js_defer_script_inserted";

JsDeferDisabledFilter::JsDeferDisabledFilter(RewriteDriver* driver)
    : rewrite_driver_(driver) {
}

JsDeferDisabledFilter::~JsDeferDisabledFilter() { }

void JsDeferDisabledFilter::DetermineEnabled() {
  set_is_enabled(ShouldApply(rewrite_driver_));
}

bool JsDeferDisabledFilter::ShouldApply(RewriteDriver* driver) {
  return driver->UserAgentSupportsJsDefer() && !driver->flushing_early();
}

void JsDeferDisabledFilter::StartDocument() {
}

void JsDeferDisabledFilter::InsertJsDeferCode() {
  if (!rewrite_driver_->is_defer_javascript_script_flushed()) {
    StaticJavascriptManager* static_js_manager =
        rewrite_driver_->server_context()->static_javascript_manager();
    const RewriteOptions* options = rewrite_driver_->options();
    // Insert script node with deferJs code as outlined.
    HtmlElement* defer_js_url_node =
        rewrite_driver_->NewElement(NULL, HtmlName::kScript);
    rewrite_driver_->AddAttribute(defer_js_url_node, HtmlName::kType,
                                  "text/javascript");
    rewrite_driver_->AddAttribute(defer_js_url_node, HtmlName::kSrc,
                                  static_js_manager->GetDeferJsUrl(options));
    rewrite_driver_->InsertElementAfterCurrent(defer_js_url_node);

    // Insert additional snippet needed for deferJs instantiation.
    HtmlElement* script_node =
        rewrite_driver_->NewElement(NULL, HtmlName::kScript);
    rewrite_driver_->InsertElementAfterElement(defer_js_url_node, script_node);

    static_js_manager->AddJsToElement(JsDeferDisabledFilter::kSuffix,
                                      script_node, rewrite_driver_);
    rewrite_driver_->set_is_defer_javascript_script_flushed(true);
  }
}

void JsDeferDisabledFilter::EndDocument() {
  if (!ShouldApply(rewrite_driver_)) {
    return;
  }

  InsertJsDeferCode();
  rewrite_driver_->UpdatePropertyValueInDomCohort(
      kIsJsDeferScriptInsertedPropertyName, "1");
}

}  // namespace net_instaweb
