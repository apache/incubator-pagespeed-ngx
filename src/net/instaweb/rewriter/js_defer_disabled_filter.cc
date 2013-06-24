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
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/util/public/null_message_handler.h"

namespace net_instaweb {

JsDeferDisabledFilter::JsDeferDisabledFilter(RewriteDriver* driver)
    : CommonFilter(driver) {
}

JsDeferDisabledFilter::~JsDeferDisabledFilter() { }

void JsDeferDisabledFilter::DetermineEnabled() {
  set_is_enabled(ShouldApply(driver()) &&
                 !driver()->flushing_cached_html() &&
                 !driver()->flushed_cached_html());
}

bool JsDeferDisabledFilter::ShouldApply(RewriteDriver* driver) {
  return driver->request_properties()->SupportsJsDefer(
      driver->options()->enable_aggressive_rewriters_for_mobile()) &&
      !driver->flushing_early();
}

void JsDeferDisabledFilter::InsertJsDeferCode() {
  StaticAssetManager* static_asset_manager =
      driver()->server_context()->static_asset_manager();
  const RewriteOptions* options = driver()->options();
  // Insert script node with deferJs code as outlined.
  HtmlElement* defer_js_url_node =
      driver()->NewElement(NULL, HtmlName::kScript);
  driver()->AddAttribute(defer_js_url_node, HtmlName::kType,
                                "text/javascript");
  driver()->AddAttribute(
      defer_js_url_node, HtmlName::kSrc,
      static_asset_manager->GetAssetUrl(StaticAssetManager::kDeferJs, options));

  InsertNodeAtBodyEnd(defer_js_url_node);
}

void JsDeferDisabledFilter::EndDocument() {
  if (!ShouldApply(driver())) {
    return;
  }

  InsertJsDeferCode();
}

}  // namespace net_instaweb
