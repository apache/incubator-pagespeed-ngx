/*
 * Copyright 2014 Google Inc.
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

// Author: jmaessen@google.com (Jan-Willem Maessen)

#include "net/instaweb/rewriter/public/mobilize_label_filter.h"

#include "net/instaweb/rewriter/public/mobilize_rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"

namespace net_instaweb {

const char MobilizeLabelFilter::kPagesLabeled[] =
    "mobilization_pages_labeled";
const char MobilizeLabelFilter::kPagesRoleAdded[] =
    "mobilization_pages_role_added";
const char MobilizeLabelFilter::kNavigationalRoles[] =
    "mobilization_navigational_roles";
const char MobilizeLabelFilter::kHeaderRoles[] =
    "mobilization_header_roles";
const char MobilizeLabelFilter::kContentRoles[] =
    "mobilization_content_roles";
const char MobilizeLabelFilter::kMarginalRoles[] =
    "mobilization_marginal_roles";

MobilizeLabelFilter::MobilizeLabelFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      active_no_traverse_element_(NULL),
      active_no_label_element_(NULL),
      were_roles_added_(false) {
  Statistics* stats = driver->statistics();
  pages_labeled_ = stats->GetVariable(kPagesLabeled);
  pages_role_added_ = stats->GetVariable(kPagesRoleAdded);
  role_variables_[MobileRole::kKeeper] = NULL;
  role_variables_[MobileRole::kHeader] = stats->GetVariable(kHeaderRoles);
  role_variables_[MobileRole::kNavigational] =
      stats->GetVariable(kNavigationalRoles);
  role_variables_[MobileRole::kContent] = stats->GetVariable(kContentRoles);
  role_variables_[MobileRole::kMarginal] = stats->GetVariable(kMarginalRoles);
}

MobilizeLabelFilter::~MobilizeLabelFilter() { }

void MobilizeLabelFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(MobilizeLabelFilter::kPagesLabeled);
  statistics->AddVariable(MobilizeLabelFilter::kPagesRoleAdded);
  statistics->AddVariable(MobilizeLabelFilter::kNavigationalRoles);
  statistics->AddVariable(MobilizeLabelFilter::kHeaderRoles);
  statistics->AddVariable(MobilizeLabelFilter::kContentRoles);
  statistics->AddVariable(MobilizeLabelFilter::kMarginalRoles);
}

void MobilizeLabelFilter::StartDocumentImpl() {
  active_no_traverse_element_ = NULL;
  active_no_label_element_ = NULL;
  were_roles_added_ = false;
}

void MobilizeLabelFilter::StartElementImpl(HtmlElement* element) {
  if (active_no_traverse_element_ != NULL) {
    return;
  }
  if (element->keyword() == HtmlName::kHead) {
    // Ignore all content in document head.  Note: this is potentially unsafe,
    // as browsers will sometimes display content included in HEAD if it looks
    // like the page author included it there by mistake.
    active_no_traverse_element_ = element;
    return;
  }
  HtmlElement::Attribute* existing_importance =
      element->FindAttribute(HtmlName::kDataMobileRole);
  if (existing_importance != NULL) {
    // If element already has a defined importance, ignore it and all its
    // children.
    // TODO(jmaessen): still compute metrics over child nodes to permit parent
    // roles to override children?  Or disable all labeling entirely?
    active_no_label_element_ = element;
    return;
  }
  switch (element->keyword()) {
    case HtmlName::kNav:
    case HtmlName::kMenu:
      SetMobileRole(element, MobileRole::kNavigational);
      active_no_traverse_element_ = element;
      return;
    case HtmlName::kHeader:
      SetMobileRole(element, MobileRole::kHeader);
      active_no_label_element_ = element;
      return;
    case HtmlName::kMain:
    case HtmlName::kArticle:
    case HtmlName::kSection:
      SetMobileRole(element, MobileRole::kContent);
      active_no_label_element_ = element;
      return;
    case HtmlName::kAside:
    case HtmlName::kFooter:
      SetMobileRole(element, MobileRole::kMarginal);
      active_no_label_element_ = element;
      return;
    default:
      // Ignore most HTML tags.
      return;
  }
}

void MobilizeLabelFilter::EndElementImpl(HtmlElement* element) {
  if (active_no_traverse_element_ != NULL) {
    if (active_no_traverse_element_ == element) {
      active_no_traverse_element_ = NULL;
    }
    return;
  }
  if (active_no_label_element_ != NULL) {
    if (active_no_label_element_ == element) {
      active_no_label_element_ = NULL;
    }
  }
}

void MobilizeLabelFilter::EndDocument() {
  pages_labeled_->Add(1);
  if (were_roles_added_) {
    pages_role_added_->Add(1);
  }
}

void MobilizeLabelFilter::SetMobileRole(HtmlElement* element,
                                        MobileRole::Level level) {
  if (active_no_label_element_ != NULL) {
    return;
  }
  driver()->AddEscapedAttribute(element, HtmlName::kDataMobileRole,
                                MobileRole::StringFromLevel(level));
  were_roles_added_ = true;
  role_variables_[level]->Add(1);
}

}  // namespace net_instaweb
