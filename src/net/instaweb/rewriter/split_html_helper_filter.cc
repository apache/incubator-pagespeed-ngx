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

// Author: bharathbhushan@google.com (Bharath Bhushan Kowshik Raghupathi)
//
// Contains implementation of SplitHtmlHelper filter which marks img tags with
// appropriate attributes so that other filters (like lazyload images and inline
// preview images) can apply efficiently in the present of split html filter.

#include "net/instaweb/rewriter/public/split_html_helper_filter.h"

#include <memory>
#include <vector>

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/split_html_config.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"

namespace net_instaweb {

// At StartElement, if element is panel instance push a new element on the
// element stack. All elements until a new panel instance is found or the
// current panel ends are treated as belonging to below-the-fold html and no
// transformations (related to img tags) are done.
SplitHtmlHelperFilter::SplitHtmlHelperFilter(RewriteDriver* rewrite_driver)
    : CommonFilter(rewrite_driver),
      current_panel_element_(NULL) {
}

SplitHtmlHelperFilter::~SplitHtmlHelperFilter() {
}

void SplitHtmlHelperFilter::DetermineEnabled() {
  bool disable_filter = !driver()->request_properties()->SupportsSplitHtml(
      driver()->options()->enable_aggressive_rewriters_for_mobile());
  if (disable_filter) {
    driver()->log_record()->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kSplitHtmlHelper),
        RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED);
    set_is_enabled(false);
    return;
  }
  // If the critical line config is not present, this filter cannot do anything
  // useful.
  config_ = driver()->split_html_config();
  disable_filter = config_->critical_line_info() == NULL;
  if (disable_filter) {
    driver()->log_record()->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kSplitHtmlHelper),
        RewriterHtmlApplication::DISABLED);
    set_is_enabled(false);
    return;
  }
  driver()->log_record()->LogRewriterHtmlStatus(
      RewriteOptions::FilterId(RewriteOptions::kSplitHtmlHelper),
      RewriterHtmlApplication::ACTIVE);
  set_is_enabled(true);
}

void SplitHtmlHelperFilter::StartDocumentImpl() {
  set_current_panel_element(NULL);
  state_.reset(new SplitHtmlState(config_));

  // Clear out all the critical images obtained from pcache since we override
  // it. If above-the-fold html is requested (or split_html is being used in a
  // single request mode), we will populate the critical images when we see an
  // img tag which is in an above-the-fold panel. This allows inline-preview to
  // operate on the above-the-fold images.
  CriticalImagesInfo* critical_images_info =
      driver()->critical_images_info();
  if (critical_images_info != NULL) {
    critical_images_info->html_critical_images.clear();
    critical_images_info->css_critical_images.clear();
  } else {
    driver()->set_critical_images_info(new CriticalImagesInfo);
  }
  driver()->critical_images_info()->is_critical_image_info_present = true;
}

void SplitHtmlHelperFilter::EndDocument() {
  set_current_panel_element(NULL);
}

void SplitHtmlHelperFilter::StartElementImpl(HtmlElement* element) {
  state_->UpdateNumChildrenStack(element);
  if (state_->IsEndMarkerForCurrentPanel(element)) {
    EndPanelInstance();
  }

  if (state_->current_panel_id().empty()) {
    GoogleString panel_id = state_->MatchPanelIdForElement(element);
    // if panel_id is empty, then element didn't match with any start xpath of
    // panel specs
    if (!panel_id.empty()) {
      StartPanelInstance(element, panel_id);
    }
  }
  resource_tag_scanner::UrlCategoryVector attributes;
  resource_tag_scanner::ScanElement(element, driver_->options(), &attributes);
  for (int i = 0, n = attributes.size(); i < n; ++i) {
    if (attributes[i].category == semantic_type::kImage &&
        attributes[i].url->DecodedValueOrNull() != NULL &&
        (driver()->request_context()->split_request_type() !=
         RequestContext::SPLIT_BELOW_THE_FOLD)) {
      if (!state_->current_panel_id().empty()) {
        // For a below-the-fold image, insert a pagespeed_no_transform attribute
        // to prevent inline-preview-images filter from doing any rewriting.
        element->AddAttribute(
            driver()->MakeName(HtmlName::kPagespeedNoTransform),
            "", HtmlElement::NO_QUOTE);
      } else {
        // For an above-the-fold image, insert the url as a critical image.
        GoogleUrl image_gurl(driver()->base_url(),
                             attributes[i].url->DecodedValueOrNull());
        if (image_gurl.IsWebValid()) {
          GoogleString url(image_gurl.spec_c_str());
          CriticalImagesFinder* finder =
              driver()->server_context()->critical_images_finder();
          finder->AddHtmlCriticalImage(url, driver());
        }
      }
    }
  }
}

void SplitHtmlHelperFilter::EndElementImpl(HtmlElement* element) {
  if (!state_->num_children_stack()->empty()) {
    state_->num_children_stack()->pop_back();
  }
  if (state_->IsElementParentOfCurrentPanel(element) ||
      (element->parent() == NULL && current_panel_element() == element)) {
    EndPanelInstance();
  }
}

void SplitHtmlHelperFilter::StartPanelInstance(HtmlElement* element,
                                               const GoogleString& panel_id) {
  set_current_panel_element(element);
  if (element != NULL) {
    state_->set_current_panel_parent_element(element->parent());
    state_->set_current_panel_id(panel_id);
  }
}

void SplitHtmlHelperFilter::EndPanelInstance() {
  set_current_panel_element(NULL);
  state_->set_current_panel_parent_element(NULL);
  state_->set_current_panel_id("");
}

}  // namespace net_instaweb
