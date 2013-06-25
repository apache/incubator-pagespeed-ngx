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

#include <map>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/critical_line_info.pb.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
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
      disable_filter_(false),
      inside_pagespeed_no_defer_script_(false),
      current_panel_parent_element_(NULL),
      script_tag_scanner_(rewrite_driver) {
}

SplitHtmlHelperFilter::~SplitHtmlHelperFilter() {
}

void SplitHtmlHelperFilter::StartDocumentImpl() {
  element_json_stack_.clear();
  num_children_stack_.clear();
  current_panel_id_.clear();
  inside_pagespeed_no_defer_script_ = false;
  current_panel_parent_element_ = NULL;

  config_.reset(new SplitHtmlConfig(driver()));

  disable_filter_ = !driver()->request_properties()->SupportsSplitHtml(
      driver()->options()->enable_aggressive_rewriters_for_mobile());
  if (disable_filter_) {
    driver()->log_record()->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kSplitHtmlHelper),
        RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED);
    return;
  }
  // If the critical line config is not present, this filter cannot do anything
  // useful.
  disable_filter_ = config_->critical_line_info() == NULL;
  if (disable_filter_) {
    driver()->log_record()->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kSplitHtmlHelper),
        RewriterHtmlApplication::DISABLED);
    return;
  }
  driver()->log_record()->LogRewriterHtmlStatus(
      RewriteOptions::FilterId(RewriteOptions::kSplitHtmlHelper),
      RewriterHtmlApplication::ACTIVE);

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
    critical_images_info->is_set_from_pcache = false;
  }

  // Push the base panel.
  StartPanelInstance(static_cast<HtmlElement*>(NULL), "");
}

void SplitHtmlHelperFilter::EndDocument() {
  if (disable_filter_) {
    return;
  }

  // Remove critical html since it should already have been sent out by now.
  element_json_stack_[0].second->removeMember(BlinkUtil::kInstanceHtml);
  // Delete the root object pushed in StartDocument;
  delete element_json_stack_[0].second;
  element_json_stack_.pop_back();
}

bool SplitHtmlHelperFilter::IsElementParentOfCurrentPanel(
    HtmlElement* element) {
  return current_panel_parent_element_ != NULL &&
      current_panel_parent_element_ == element;
}

void SplitHtmlHelperFilter::EndPanelInstance() {
  ElementJsonPair element_json_pair = element_json_stack_.back();
  scoped_ptr<Json::Value> dictionary(element_json_pair.second);
  element_json_stack_.pop_back();
  Json::Value* parent_dictionary = element_json_stack_.back().second;
  AppendJsonData(&((*parent_dictionary)[current_panel_id_]), *dictionary);
  current_panel_parent_element_ = NULL;
  current_panel_id_ = "";
}

void SplitHtmlHelperFilter::StartPanelInstance(HtmlElement* element,
                                               const GoogleString& panel_id) {
  Json::Value* new_json = new Json::Value(Json::objectValue);
  // Push new Json
  element_json_stack_.push_back(std::make_pair(element, new_json));
  if (element != NULL) {
    current_panel_parent_element_ = element->parent();
    current_panel_id_ = panel_id;
  }
}

void SplitHtmlHelperFilter::StartElementImpl(HtmlElement* element) {
  if (disable_filter_) {
    return;
  }

  if (element->FindAttribute(HtmlName::kPagespeedNoDefer) &&
      element_json_stack_.size() > 1 ) {
    HtmlElement::Attribute* src = NULL;
    if (script_tag_scanner_.ParseScriptElement(element, &src) ==
        ScriptTagScanner::kJavaScript) {
      inside_pagespeed_no_defer_script_ = true;
      return;
    }
  }

  if (!num_children_stack_.empty()) {
    // Ignore some of the non-rendered tags for numbering the children. This
    // helps avoid mismatches due to combine_javascript combining differently
    // and creating different numbers of script nodes in different rewrites.
    // This also helps when combine_css combines link tags or styles differently
    // in different rewrites.
    if (element->keyword() != HtmlName::kScript &&
        element->keyword() != HtmlName::kNoscript &&
        element->keyword() != HtmlName::kStyle &&
        element->keyword() != HtmlName::kLink) {
      num_children_stack_.back()++;;
    }
    num_children_stack_.push_back(0);
  } else if (element->keyword() == HtmlName::kBody) {
    // Start the stack only once body is encountered.
    num_children_stack_.push_back(0);
  }

  if (IsEndMarkerForCurrentPanel(element)) {
    EndPanelInstance();
  }

  GoogleString panel_id = MatchPanelIdForElement(element);
  // if panel_id is empty, then element didn't match with any start xpath of
  // panel specs
  if (!panel_id.empty()) {
    StartPanelInstance(element, panel_id);
  }
  semantic_type::Category category;
  HtmlElement::Attribute* src = resource_tag_scanner::ScanElement(
      element, driver(), &category);
  if (category == semantic_type::kImage &&
      src != NULL && src->DecodedValueOrNull() != NULL &&
      !driver()->request_context()->is_split_btf_request()) {
    if (element_json_stack_.size() > 1) {
      // For a below-the-fold image, insert a pagespeed_no_transform attribute
      // to prevent inline-preview-images filter from doing any rewriting.
      element->AddAttribute(
          driver()->MakeName(HtmlName::kPagespeedNoTransform),
          "", HtmlElement::NO_QUOTE);
    } else if (driver()->critical_images_info() != NULL) {
      // For an above-the-fold image, insert the url as a critical image.
      GoogleUrl image_gurl(driver()->base_url(),
                           src->DecodedValueOrNull());
      if (image_gurl.is_valid()) {
        GoogleString url(image_gurl.spec_c_str());
        driver()->critical_images_info()->html_critical_images.insert(url);
      }
    }
  }
}

void SplitHtmlHelperFilter::EndElementImpl(HtmlElement* element) {
  if (disable_filter_) {
    return;
  }

  if (inside_pagespeed_no_defer_script_) {
    inside_pagespeed_no_defer_script_ = false;
    return;
  }

  if (!num_children_stack_.empty()) {
    num_children_stack_.pop_back();
  }
  if (IsElementParentOfCurrentPanel(element) ||
      (element->parent() == NULL &&
       element_json_stack_.back().first == element)) {
    EndPanelInstance();
  }
}

void SplitHtmlHelperFilter::AppendJsonData(Json::Value* dictionary,
                                           const Json::Value& dict) {
  if (!dictionary->isArray()) {
    *dictionary = Json::arrayValue;
  }
  dictionary->append(dict);
}

GoogleString SplitHtmlHelperFilter::MatchPanelIdForElement(
    HtmlElement* element) {
  for (int i = 0; i < config_->critical_line_info()->panels_size(); i++) {
    const Panel& panel = config_->critical_line_info()->panels(i);
    if (ElementMatchesXpath(
        element, *((*config_->xpath_map())[panel.start_xpath()]))) {
      return StrCat(BlinkUtil::kPanelId, ".", IntegerToString(i));
    }
  }
  return "";
}

bool SplitHtmlHelperFilter::IsEndMarkerForCurrentPanel(HtmlElement* element) {
  if (current_panel_parent_element_ == NULL) {
    return false;
  }

  PanelIdToSpecMap* panel_id_to_spec = config_->panel_id_to_spec();
  if (panel_id_to_spec->find(current_panel_id_) == panel_id_to_spec->end()) {
    LOG(DFATAL) << "Invalid Panelid: " << current_panel_id_ << " for url "
                << driver()->google_url().Spec();
    return false;
  }
  const Panel& panel = *((*panel_id_to_spec)[current_panel_id_]);
  return panel.has_end_marker_xpath() ?
      ElementMatchesXpath(
          element, *((*config_->xpath_map())[panel.end_marker_xpath()])) :
      false;
}

bool SplitHtmlHelperFilter::ElementMatchesXpath(
    const HtmlElement* element, const std::vector<XpathUnit>& xpath_units) {
  int j = xpath_units.size() - 1, k = num_children_stack_.size() - 2;
  for (; j >= 0 && k >= 0; j--, k--, element = element->parent()) {
    if (element->name_str() !=  xpath_units[j].tag_name) {
      return false;
    }
    if (!xpath_units[j].attribute_value.empty()) {
      return (element->AttributeValue(HtmlName::kId) != NULL &&
          element->AttributeValue(HtmlName::kId) ==
              xpath_units[j].attribute_value);
    } else if (xpath_units[j].child_number == num_children_stack_[k]) {
      continue;
    } else {
      return false;
    }
  }

  if (j < 0 && k < 0) {
    return true;
  }
  return false;
}

}  // namespace net_instaweb
