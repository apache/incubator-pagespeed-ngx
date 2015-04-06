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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_FILTER_BASE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_FILTER_BASE_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/mobilize_decision_trees.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"

namespace net_instaweb {

// A mobile role and its associated HTML attribute value.
struct MobileRoleData {
  static const MobileRoleData kMobileRoles[MobileRole::kInvalid];

  MobileRoleData(MobileRole::Level level, const char* value)
      : level(level),
        value(value) {}

  static const MobileRoleData* FromString(StringPiece mobile_role);
  static MobileRole::Level LevelFromString(StringPiece mobile_role);
  static const char* StringFromLevel(MobileRole::Level level) {
    return (level < MobileRole::kInvalid) ? kMobileRoles[level].value : NULL;
  }

  const MobileRole::Level level;
  const char* const value;  // Set to a static string in cc.
};

class MobilizeFilterBase : public CommonFilter {
 public:
  explicit MobilizeFilterBase(RewriteDriver* driver);
  virtual ~MobilizeFilterBase();

  static bool IsKeeperTag(HtmlName::Keyword tag);

  // Don't override these.  Override the NonSkip virtual methods below.
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);
  virtual void EndDocument();

 protected:
  bool AreInSkip() {
    return active_skip_element_ != NULL;
  }
  // Only called when !AreInSkip (not including skipped elements themselves).
  virtual void StartNonSkipElement(
      MobileRole::Level role_attribute, HtmlElement* element) = 0;
  // Called exactly when the matching Start method is called.
  virtual void EndNonSkipElement(HtmlElement* element) = 0;
  virtual void EndDocumentImpl() = 0;

 private:
  HtmlElement* active_skip_element_;

  DISALLOW_COPY_AND_ASSIGN(MobilizeFilterBase);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_FILTER_BASE_H_
