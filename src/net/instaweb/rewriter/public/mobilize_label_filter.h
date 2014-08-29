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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_LABEL_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_LABEL_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/mobilize_rewrite_filter.h"
#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;
class Statistics;
class Variable;

// Classify DOM elements by adding importance= attributes so that the
// MoblizeRewriteFilter can rewrite them to be mobile-friendly.  The classes
// are:
//   Navigational: things like nav and menu bars, mostly in the header
//   Header: Page title, title image, logo associated with page, etc.
//   Content: The content we think the user wants to see.
//   Marginal: Other stuff on the page that typically resides in the margins,
//     header, or footer.
// Initially we just attempt Navigational and Header classification.
// TODO(jmaessen): do the rest of the classification.
// We do this bottom-up, since we want to process children in a streaming
// fashion before their parent's close tag.  We take the presence of html5 tags
// as authoritative (though in practice this might not be the case), but we've
// assumed that they're authoritative in training our classifiers.
// TODO(jmaessen): use actual classifier output.
class MobilizeLabelFilter : public CommonFilter {
 public:
  // Monitoring variable names
  static const char kPagesLabeled[];  // Pages run through labeler.
  static const char kPagesRoleAdded[];
  static const char kNavigationalRoles[];
  static const char kHeaderRoles[];
  static const char kContentRoles[];
  static const char kMarginalRoles[];

  explicit MobilizeLabelFilter(RewriteDriver* driver);
  virtual ~MobilizeLabelFilter();

  static void InitStats(Statistics* statistics);

  virtual const char* Name() const { return "MobilizeLabel"; }
  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);
  virtual void EndDocument();

 private:
  void SetMobileRole(HtmlElement* element, MobileRole::Level level);

  HtmlElement* active_no_traverse_element_;
  HtmlElement* active_no_label_element_;
  bool were_roles_added_;

  Variable* pages_labeled_;
  Variable* pages_role_added_;
  Variable* role_variables_[MobileRole::kInvalid];

  DISALLOW_COPY_AND_ASSIGN(MobilizeLabelFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_LABEL_FILTER_H_
