/*
 * Copyright 2015 Google Inc.
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

#include "net/instaweb/rewriter/public/mobilize_filter_base.h"

#include <algorithm>

#include "base/logging.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/mobilize_decision_trees.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"

namespace net_instaweb {

namespace {

const HtmlName::Keyword kKeeperTags[] = {
  HtmlName::kArea, HtmlName::kLink, HtmlName::kMap, HtmlName::kMeta,
  HtmlName::kScript, HtmlName::kStyle, HtmlName::kTitle};
const int kNumKeeperTags = arraysize(kKeeperTags);

#ifndef NDEBUG
// For invariant-checking the static data above.
void CheckKeepersSorted() {
  for (int i = 1; i < kNumKeeperTags; ++i) {
    DCHECK_LT(kKeeperTags[i - 1], kKeeperTags[i]);
  }
}
#endif  // #ifndef NDEBUG

}  // namespace

const MobileRoleData MobileRoleData::kMobileRoles[MobileRole::kInvalid] = {
  MobileRoleData(MobileRole::kKeeper, "keeper"),
  MobileRoleData(MobileRole::kHeader, "header"),
  MobileRoleData(MobileRole::kNavigational, "navigational"),
  MobileRoleData(MobileRole::kContent, "content"),
  MobileRoleData(MobileRole::kMarginal, "marginal")
};

const MobileRoleData* MobileRoleData::FromString(StringPiece mobile_role) {
  for (int i = 0, n = arraysize(kMobileRoles); i < n; ++i) {
    if (mobile_role == kMobileRoles[i].value) {
      return &kMobileRoles[i];
    }
  }
  return NULL;
}

MobileRole::Level MobileRoleData::LevelFromString(StringPiece mobile_role) {
  const MobileRoleData* role = FromString(mobile_role);
  if (role == NULL) {
    return MobileRole::kInvalid;
  } else {
    return role->level;
  }
}

bool MobilizeFilterBase::IsKeeperTag(HtmlName::Keyword tag) {
  return std::binary_search(kKeeperTags, kKeeperTags + kNumKeeperTags, tag);
}

MobilizeFilterBase::MobilizeFilterBase(RewriteDriver* driver)
    : CommonFilter(driver),
      active_skip_element_(NULL) {
#ifndef NDEBUG
  CheckKeepersSorted();
#endif  // #ifndef NDEBUG
}

MobilizeFilterBase::~MobilizeFilterBase() {
  DCHECK(!AreInSkip());
}

void MobilizeFilterBase::StartElementImpl(HtmlElement* element) {
  if (AreInSkip()) {
    return;
  }
  if (IsKeeperTag(element->keyword())) {
    // Ignore content in things like <script> and <style> blocks that
    // don't contain user-accessible content.
    active_skip_element_ = element;
    return;
  }
  HtmlElement::Attribute* mobile_role_attribute =
      element->FindAttribute(HtmlName::kDataMobileRole);
  MobileRole::Level role =
      mobile_role_attribute == NULL ? MobileRole::kUnassigned :
      MobileRoleData::LevelFromString(mobile_role_attribute->escaped_value());
  StartNonSkipElement(role, element);
}

void MobilizeFilterBase::EndElementImpl(HtmlElement* element) {
  if (AreInSkip()) {
    if (active_skip_element_ == element) {
      active_skip_element_ = NULL;
    }
    return;
  }
  EndNonSkipElement(element);
}

void MobilizeFilterBase::EndDocument() {
  EndDocumentImpl();
  DCHECK(active_skip_element_ == NULL);
  active_skip_element_ = NULL;
}

}  // namespace net_instaweb
