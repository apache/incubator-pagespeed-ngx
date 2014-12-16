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

#include <vector>

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/mobilize_decision_trees.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/html/html_parse.h"

namespace net_instaweb {

// Sample capturing the feature vector for a given DOM element.  We compute
// these up the DOM tree, aggregating into the parent when each child finishes.
// We also keep a global root sample so we can normalize statistics, and so that
// every actual DOM sample has a parent.
//
// Every feature is represented by a double entry in the feature vector f.
// Features ending in "Percent" have values between 0 and 100.0 and are computed
// at end of document by ComputeProportionalFeatures.  All other features are
// non-negative integers in practice.  We don't need the precision of doubles,
// but we do need the dynamic integer range or counters will peg.
struct ElementSample {
  ElementSample(int relevant_tag_depth, int tag_count,
                int content_bytes, int content_non_blank_bytes);

  // Here normalized represents 100 / global measurement, used
  // as a multiplier to compute percent features.
  void ComputeProportionalFeatures(ElementSample* normalized);
  GoogleString ToString(bool readable, HtmlParse* parser);

  HtmlElement* element;          // NULL for global count
  GoogleString id;               // id of *element, which might be flushed.
  ElementSample* parent;         // NULL for global count
  MobileRole::Level role;        // Mobile role (from parent where applicable)
  bool explicitly_labeled;       // Was this DOM element explicitly labeled?
  std::vector<double> features;  // feature vector, always of size kNumFeatures.
};

// Classify DOM elements by adding data-mobile-role= attributes so that the
// MoblizeRewriteFilter can rewrite them to be mobile-friendly.  The classes
// are:
//   Navigational: things like nav and menu bars, mostly in the header
//   Header: Page title, title image, logo associated with page, etc.
//   Content: The content we think the user wants to see.
//   Marginal: Other stuff on the page that typically resides in the margins,
//     header, or footer.
// We do this bottom-up, since we want to process children in a streaming
// fashion before their parent's close tag.  We take the presence of html5 tags
// as authoritative if UseTagNames is in LabelingMode; note that we've
// assumed that they're authoritative in training our classifiers.
class MobilizeLabelFilter : public CommonFilter {
 public:
  struct LabelingMode {
    bool use_tag_names : 1;
    bool use_classifier : 1;
    bool propagate_to_parent : 1;
  };

  static const LabelingMode kDoNotLabel;
  static const LabelingMode kUseTagNames;
  static const LabelingMode kDefaultLabelingMode;

  // Monitoring variable names
  static const char kPagesLabeled[];  // Pages run through labeler.
  static const char kPagesRoleAdded[];
  static const char kNavigationalRoles[];
  static const char kHeaderRoles[];
  static const char kContentRoles[];
  static const char kMarginalRoles[];
  static const char kDivsUnlabeled[];
  static const char kAmbiguousRoleLabels[];

  explicit MobilizeLabelFilter(RewriteDriver* driver);
  virtual ~MobilizeLabelFilter();

  static void InitStats(Statistics* statistics);

  virtual const char* Name() const { return "MobilizeLabel"; }
  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual void EndDocument();
  // Set labeling mode to use during traversal.
  // Intended for testing and debugging.
  LabelingMode& mutable_labeling_mode() {
    return labeling_mode_;
  }
  LabelingMode labeling_mode() const {
    return labeling_mode_;
  }

 private:
  void Init();
  void HandleDivLikeElement(HtmlElement* element, MobileRole::Level role);
  void CheckAttributeStrings(HtmlElement* element);
  ElementSample* MakeNewSample(HtmlElement* element);
  void PopSampleStack();
  void ComputeContained(ElementSample* sample);
  void AggregateToTopOfStack(ElementSample* sample);
  void IncrementRelevantTagDepth();
  void SanityCheckEndOfDocumentState();
  void ComputeProportionalFeatures();
  void PropagateChildrenToParent(MobileRole::Level level);
  void Label();
  void DebugLabel();
  void InjectLabelJavascript();
  void NonMobileUnlabel();

  HtmlElement* active_no_traverse_element_;
  int relevant_tag_depth_;
  int max_relevant_tag_depth_;
  int link_depth_;
  int tag_count_;
  int content_bytes_;
  int content_non_blank_bytes_;
  bool were_roles_added_;
  LabelingMode labeling_mode_;

  std::vector<ElementSample*> samples_;  // in document order
  std::vector<ElementSample*> sample_stack_;

  Variable* pages_labeled_;
  Variable* pages_role_added_;
  Variable* role_variables_[MobileRole::kInvalid];
  Variable* divs_unlabeled_;
  Variable* ambiguous_role_labels_;

  DISALLOW_COPY_AND_ASSIGN(MobilizeLabelFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_LABEL_FILTER_H_
