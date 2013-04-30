/*
 * Copyright 2013 Google Inc.
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

// Author: bharathbhushan@google.com (Bharath Bhushan)

#include "net/instaweb/rewriter/public/dom_stats_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

DomStatsFilter::DomStatsFilter(RewriteDriver* driver)
  : CommonFilter(driver), script_tag_scanner_(driver) {
  Clear();
}

DomStatsFilter::~DomStatsFilter() {}

void DomStatsFilter::Clear() {
  num_img_tags_ = 0;
  num_inlined_img_tags_ = 0;
  num_external_css_ = 0;
  num_scripts_ = 0;
  num_critical_images_used_ = 0;
}

void DomStatsFilter::StartDocumentImpl() {
  Clear();
}

void DomStatsFilter::EndElementImpl(HtmlElement* element) {
  if (element->keyword() == HtmlName::kImg) {
    ++num_img_tags_;

    HtmlElement::Attribute* src = element->FindAttribute(HtmlName::kSrc);
    StringPiece url(src == NULL ? "" : src->DecodedValueOrNull());
    if (!url.empty()) {
      if (IsDataUrl(url)) {
        ++num_inlined_img_tags_;
      } else {
        CriticalImagesFinder* finder =
            driver_->server_context()->critical_images_finder();
        if (finder->IsMeaningful(driver())) {
          GoogleUrl image_gurl(driver()->base_url(), url);
          if (finder->IsHtmlCriticalImage(image_gurl.spec_c_str(), driver())) {
            ++num_critical_images_used_;
          }
        }
      }
    }
  } else if (element->keyword() == HtmlName::kLink &&
      CssTagScanner::IsStylesheetOrAlternate(
          element->AttributeValue(HtmlName::kRel)) &&
      element->FindAttribute(HtmlName::kHref) != NULL) {
    ++num_external_css_;
  } else {
    HtmlElement::Attribute* src;
    if (script_tag_scanner_.ParseScriptElement(element, &src) ==
        ScriptTagScanner::kJavaScript) {
      ++num_scripts_;
    }
  }
}

}  // namespace net_instaweb
