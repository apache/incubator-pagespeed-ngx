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

// Author: pulkitg@google.com (Pulkit Goyal)

#include "net/instaweb/rewriter/public/defer_iframe_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/http/public/device_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

const char DeferIframeFilter::kDeferIframeInit[] =
    "pagespeed.deferIframeInit();";
const char DeferIframeFilter::kDeferIframeIframeJs[] =
    "\npagespeed.deferIframe.convertToIframe();";

DeferIframeFilter::DeferIframeFilter(RewriteDriver* driver)
    : driver_(driver),
      static_asset_manager_(
          driver->server_context()->static_asset_manager()),
      script_inserted_(false) {}

DeferIframeFilter::~DeferIframeFilter() {
}

void DeferIframeFilter::DetermineEnabled() {
  set_is_enabled(driver_->device_properties()->SupportsJsDefer(
      driver_->options()->enable_aggressive_rewriters_for_mobile()));
}

void DeferIframeFilter::StartDocument() {
  script_inserted_ = false;
}

void DeferIframeFilter::StartElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kIframe) {
    if (!script_inserted_) {
      HtmlElement* script = driver_->NewElement(element->parent(),
                                                HtmlName::kScript);
      driver_->InsertElementBeforeElement(element, script);

      GoogleString js = StrCat(
          static_asset_manager_->GetAsset(
              StaticAssetManager::kDeferIframe, driver_->options()),
              kDeferIframeInit);
      static_asset_manager_->AddJsToElement(js, script, driver_);
      script_inserted_ = true;
    }
    element->set_name(HtmlName(HtmlName::kPagespeedIframe, "pagespeed_iframe"));
  }
}

void DeferIframeFilter::EndElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kPagespeedIframe) {
    HtmlElement* script = driver_->NewElement(element, HtmlName::kScript);
    driver_->AddAttribute(script, HtmlName::kType, "text/javascript");
    HtmlCharactersNode* script_content = driver_->NewCharactersNode(
        script, kDeferIframeIframeJs);
    driver_->AppendChild(element, script);
    driver_->AppendChild(script, script_content);
  }
}

}  // namespace net_instaweb
