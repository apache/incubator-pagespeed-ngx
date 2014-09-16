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
#include "net/instaweb/rewriter/public/mobilize_rewrite_filter.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

class HtmlCharactersNode;
class HtmlElement;
class HtmlParse;
class RewriteDriver;
class Statistics;
class Variable;

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
  GoogleString ToString(bool symbolic, HtmlParse* parser);

  HtmlElement* element;          // NULL for global count
  ElementSample* parent;         // NULL for global count
  MobileRole::Level role;        // Mobile role (from parent where applicable)
  std::vector<double> features;  // feature vector, always of size kNumFeatures.
};

// Tags that are considered relevant and are counted in a sample.  Some tags are
// role tags or otherwise considered div-like.  These tag names are used to
// index the RelevantTagCount and RelevantTagPercent features below.
// Note that it's possible to add new tags to this list.
enum MobileRelevantTag {
  kATag = 0,
  kArticleTag,  // role tag
  kAsideTag,    // role tag
  kContentTag,  // role tag
  kDivTag,      // div-like tag
  kFooterTag,   // role tag
  kH1Tag,
  kH2Tag,
  kH3Tag,
  kH4Tag,
  kH5Tag,
  kH6Tag,
  kHeaderTag,   // role tag
  kImgTag,
  kMainTag,     // role tag
  kMenuTag,     // role tag
  kNavTag,      // role tag
  kPTag,
  kSectionTag,  // role tag
  kSpanTag,
  kNumRelevantTags
};

// Attribute substrings that are considered interesting if they occur in the id,
// class, or role of a div-like tag.
enum MobileAttrSubstring {
  kArticleAttr = 0,
  kAsideAttr,
  kBodyAttr,
  kBottomAttr,
  kColumnAttr,
  kCommentAttr,
  kContentAttr,
  kFootAttr,
  kHeadAttr,
  kHdrAttr,
  kLogoAttr,
  kMainAttr,
  kMarginAttr,
  kMenuAttr,
  kNavAttr,
  kSecAttr,
  kTitleAttr,
  kNumAttrStrings
};

// Every feature has a symbolic name given by Name or Name + Index.
// DEFINITIONS OF FEATURES:
// * "Previous" features do not include the tag being labeled.
// * "Contained" and "Relevant" features do include the tag being labeled.
// * "TagCount" features ignore clearly non-user-visible tags such as <script>,
//   <style>, and <link>, and include only tags inside <body>.
// * "TagDepth" features include only div-like tags such as <div>, <section>,
//   <header>, and <aside> (see kRoleTags and kDivLikeTags in
//   mobilize_label_filter.cc).  They are the nesting depth of the tag within
//   <body>.
// * ElementTagDepth is the depth of the tag being sampled itself.
// * ContainedTagDepth is the maximum depth of any div-like child of this tag.
// * ContainedTagRelativeDepth is the difference between these two depths.
// * ContentBytes Ignores tags and their attributes, and also ignores leading
//   and trailing whitespace between tags.  So "hi there" is 8 ContentBytes,
//   but "hi <i class='foo'>there</i>" is only 7 ContentBytes.
// * NonBlankBytes is like ContentBytes but ignores all whitespace.
// * HasAttrString is a family of 0/1 entries indicating whether the
//   corresponding string (see kRelevantAttrSubstrings in
//   mobilize_label_filter.cc) occurs in the class, id, or role attribute of the
//   sampled tag.
// * RelevantTagCount is a series of counters indicating the number of various
//   "interesting" HTML tags within the current tag.  This includes all div-like
//   tags along with tags such as <p>, <a>, <h1>, and <img> (see kRelevantTags
//   in mobilize_label_filter.cc).
enum FeatureName {
  kElementTagDepth = 0,
  kPreviousTagCount,
  kPreviousTagPercent,
  kPreviousContentBytes,
  kPreviousContentPercent,
  kPreviousNonBlankBytes,
  kPreviousNonBlankPercent,
  kContainedTagDepth,
  kContainedTagRelativeDepth,
  kContainedTagCount,
  kContainedTagPercent,
  kContainedContentBytes,
  kContainedContentPercent,
  kContainedNonBlankBytes,
  kContainedNonBlankPercent,
  kHasAttrString,
  kRelevantTagCount = kHasAttrString + kNumAttrStrings,
  kRelevantTagPercent = kRelevantTagCount + kNumRelevantTags,
  kNumFeatures = kRelevantTagPercent + kNumRelevantTags
};

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
  virtual void Characters(HtmlCharactersNode* characters);
  virtual void EndDocument();

 private:
  void Init();
  void HandleDivLikeElement(HtmlElement* element, MobileRole::Level role);
  void CheckAttributeStrings(HtmlElement* element);
  void SetMobileRole(HtmlElement* element, MobileRole::Level role);
  ElementSample* MakeNewSample(HtmlElement* element);
  void PopSampleStack();
  void ComputeContained(ElementSample* sample);
  void AggregateToTopOfStack(ElementSample* sample);
  void IncrementRelevantTagDepth();
  void SanityCheckEndOfDocumentState();
  void ComputeProportionalFeatures();
  void Label();
  void DebugLabel();

  HtmlElement* active_no_traverse_element_;
  int relevant_tag_depth_;
  int max_relevant_tag_depth_;
  int tag_count_;
  int content_bytes_;
  int content_non_blank_bytes_;
  bool were_roles_added_;

  std::vector<ElementSample*> samples_;  // in document order
  std::vector<ElementSample*> sample_stack_;

  Variable* pages_labeled_;
  Variable* pages_role_added_;
  Variable* role_variables_[MobileRole::kInvalid];

  DISALLOW_COPY_AND_ASSIGN(MobilizeLabelFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_LABEL_FILTER_H_
