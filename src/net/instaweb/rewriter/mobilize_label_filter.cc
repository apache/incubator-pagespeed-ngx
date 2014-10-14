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

#include <algorithm>
#include <cstddef>

#include "base/logging.h"
#include "net/instaweb/rewriter/public/decision_tree.h"
#include "net/instaweb/rewriter/public/mobilize_decision_trees.h"
#include "net/instaweb/rewriter/public/mobilize_rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/html/html_parse.h"

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
const char MobilizeLabelFilter::kDivsUnlabeled[] =
    "mobilization_divs_unlabeled";
const char MobilizeLabelFilter::kAmbiguousRoleLabels[] =
    "mobilization_divs_with_ambiguous_role_label";

namespace {

struct RelevantTagMetadata {
  HtmlName::Keyword html_name;
  MobileRelevantTag relevant_tag;
  bool is_div_like;
  MobileRole::Level mobile_role;

  // This opr< allows retrieval of metadata by html_name.
  bool operator<(const RelevantTagMetadata& other) const {
    return (this->html_name < other.html_name);
  }
};

// For div-like sectioning tags (those with roles), see also
// https://developers.whatwg.org/sections.html#sections
const RelevantTagMetadata kRelevantTags[] = {
  /* tag name            tag symbol    div_like?   role */
  { HtmlName::kA,        kATag,        false,    MobileRole::kInvalid },
  { HtmlName::kArticle,  kArticleTag,  true,     MobileRole::kContent },
  { HtmlName::kAside,    kAsideTag,    true,     MobileRole::kMarginal },
  { HtmlName::kButton,   kButtonTag,   false,    MobileRole::kInvalid },
  { HtmlName::kContent,  kContentTag,  true,     MobileRole::kContent },
  { HtmlName::kDatalist, kDatalistTag, false,    MobileRole::kInvalid },
  { HtmlName::kDiv,      kDivTag,      true,     MobileRole::kInvalid },
  { HtmlName::kFieldset, kFieldsetTag, false,    MobileRole::kInvalid },
  { HtmlName::kFooter,   kFooterTag,   true,     MobileRole::kMarginal },
  { HtmlName::kForm,     kFormTag,     true,     MobileRole::kInvalid },
  { HtmlName::kH1,       kH1Tag,       false,    MobileRole::kInvalid },
  { HtmlName::kH2,       kH2Tag,       false,    MobileRole::kInvalid },
  { HtmlName::kH3,       kH3Tag,       false,    MobileRole::kInvalid },
  { HtmlName::kH4,       kH4Tag,       false,    MobileRole::kInvalid },
  { HtmlName::kH5,       kH5Tag,       false,    MobileRole::kInvalid },
  { HtmlName::kH6,       kH6Tag,       false,    MobileRole::kInvalid },
  { HtmlName::kHeader,   kHeaderTag,   true,     MobileRole::kHeader },
  { HtmlName::kImg,      kImgTag,      false,    MobileRole::kInvalid },
  { HtmlName::kInput,    kInputTag,    false,    MobileRole::kInvalid },
  { HtmlName::kLegend,   kLegendTag,   false,    MobileRole::kInvalid },
  { HtmlName::kMain,     kMainTag,     true,     MobileRole::kContent },
  { HtmlName::kMenu,     kMenuTag,     true,     MobileRole::kNavigational },
  { HtmlName::kNav,      kNavTag,      true,     MobileRole::kNavigational },
  { HtmlName::kOptgroup, kOptgroupTag, false,    MobileRole::kInvalid },
  { HtmlName::kOption,   kOptionTag,   false,    MobileRole::kInvalid },
  { HtmlName::kP,        kPTag,        false,    MobileRole::kInvalid },
  { HtmlName::kSection,  kSectionTag,  true,     MobileRole::kContent },
  { HtmlName::kSelect,   kSelectTag,   false,    MobileRole::kInvalid },
  { HtmlName::kSpan,     kSpanTag,     false,    MobileRole::kInvalid },
  { HtmlName::kTextarea, kTextareaTag, false,    MobileRole::kInvalid },
};

// These tags are for the purposes of this filter just enclosing semantic noise
// that will mess up our ability to learn features on small pages where their
// presence or absence just swings the tag counts around wildly.
const HtmlName::Keyword kIgnoreTags[] = {
  HtmlName::kBody,
  HtmlName::kHtml,
};

struct RelevantAttrMetadata {
  MobileAttrSubstring id;
  const char* substring;
};

// Attribute substrings that are relevant for classification.  NOTE:
// kNumAttrStrings must be kept up to date when you change this (you should get
// a compile error if you add entries, but be very careful when removing them).
const RelevantAttrMetadata kRelevantAttrSubstrings[] = {
  {kArticleAttr, "article"},
  {kAsideAttr,   "aside"},
  {kBodyAttr,    "body"},
  {kBottomAttr,  "bottom"},
  {kCenterAttr,  "center"},
  {kColumnAttr,  "column"},
  {kCommentAttr, "comment"},
  {kContentAttr, "content"},
  {kFindAttr,    "find"},
  {kFootAttr,    "foot"},
  {kHdrAttr,     "hdr"},
  {kHeadAttr,    "head"},
  {kLeftAttr,    "left"},
  {kLogoAttr,    "logo"},
  {kMainAttr,    "main"},
  {kMarginAttr,  "margin"},
  {kMenuAttr,    "menu"},
  {kMiddleAttr,  "middle"},
  {kNavAttr,     "nav"},
  {kRightAttr,   "right"},
  {kSearchAttr,  "search"},
  {kSecAttr,     "sec"},
  {kTitleAttr,   "title"},
  {kTopAttr,     "top"},
};

// We search the following attributes on div-like tags, as these attributes tend
// to have names reflecting their intended semantics and we use the presence of
// those semantically-informative names as a signal.  "Role" in particular is
// *defined* to be a well-defined semantic description of intended use, and the
// HTML5 div-like tags are largely named for role attribute values.
// See http://www.w3.org/TR/wai-aria/roles#document_structure_roles
const HtmlName::Keyword kAttrsToSearch[] = {
  HtmlName::kId,
  HtmlName::kClass,
  HtmlName::kRole
};

const int kNoIndex = -1;

#ifndef NDEBUG
// For invariant-checking the static data above.
void CheckKeywordsSorted(const HtmlName::Keyword* list, int len) {
  for (int i = 1; i < len; ++i) {
    DCHECK_LT(list[i - 1], list[i]);
  }
}

void CheckTagMetadata() {
  for (int i = 0; i < kNumRelevantTags; ++i) {
    if (i > 0) {
      DCHECK_LT(kRelevantTags[i-1].html_name, kRelevantTags[i].html_name);
    }
    MobileRelevantTag tag = static_cast<MobileRelevantTag>(i);
    CHECK_EQ(tag, kRelevantTags[i].relevant_tag);
    if (!kRelevantTags[i].is_div_like) {
      CHECK_EQ(MobileRole::kInvalid, kRelevantTags[i].mobile_role);
    }
  }
}

void CheckAttrSubstrings() {
  for (int i = 0; i < kNumAttrStrings; ++i) {
    MobileAttrSubstring id = static_cast<MobileAttrSubstring>(i);
    CHECK_EQ(id, kRelevantAttrSubstrings[i].id);
    const char* string = kRelevantAttrSubstrings[i].substring;
    CHECK(string != NULL) << i;
    CHECK_LT(static_cast<typeof(strlen(string))>(1), strlen(string))
        << i << " '" << string << "'";
  }
}
#endif  // #ifndef NDEBUG

// tag metadata, or NULL if tag is not relevant.
const RelevantTagMetadata* FindTagMetadata(HtmlName::Keyword tag) {
  // To make std::lower_bound's comparison happy we need to hand it a
  // RelevantTagMetadata, so we cons one up containing only the tag.
  RelevantTagMetadata bogus_metadata;
  bogus_metadata.html_name = tag;
  const RelevantTagMetadata* p = std::lower_bound(
      kRelevantTags, kRelevantTags + kNumRelevantTags, bogus_metadata);
  if (kRelevantTags <= p &&
      p < kRelevantTags + kNumRelevantTags &&
      p->html_name == tag) {
    return p;
  } else {
    return NULL;
  }
}

bool IsKeeperTag(HtmlName::Keyword tag) {
  return std::binary_search(MobilizeRewriteFilter::kKeeperTags,
                            MobilizeRewriteFilter::kKeeperTags +
                            MobilizeRewriteFilter::kNumKeeperTags,
                            tag);
}

bool IsIgnoreTag(HtmlName::Keyword tag) {
  return std::binary_search(
      kIgnoreTags, kIgnoreTags + arraysize(kIgnoreTags), tag);
}

}  // namespace

ElementSample::ElementSample(int relevant_tag_depth, int tag_count,
                             int content_bytes, int content_non_blank_bytes)
    : element(NULL),
      parent(NULL),
      role(MobileRole::kInvalid),
      features(kNumFeatures, 0.0) {
  features[kElementTagDepth] = relevant_tag_depth;
  features[kPreviousTagCount] = tag_count;
  features[kPreviousContentBytes] = content_bytes;
  features[kPreviousNonBlankBytes] = content_non_blank_bytes;
  features[kContainedTagDepth] = relevant_tag_depth;
}

void ElementSample::ComputeProportionalFeatures(ElementSample* normalized) {
  features[kContainedTagRelativeDepth] =
      features[kContainedTagDepth] - features[kElementTagDepth];
  features[kPreviousTagPercent] =
      features[kPreviousTagCount] *
      normalized->features[kContainedTagCount];
  features[kContainedTagPercent] =
      features[kContainedTagCount] *
      normalized->features[kContainedTagCount];
  features[kPreviousContentPercent] =
      features[kPreviousContentBytes] *
      normalized->features[kContainedContentBytes];
  features[kContainedContentPercent] =
      features[kContainedContentBytes] *
      normalized->features[kContainedContentBytes];
  features[kPreviousNonBlankPercent] =
      features[kPreviousNonBlankBytes] *
      normalized->features[kContainedNonBlankBytes];
  features[kContainedNonBlankPercent] =
      features[kContainedNonBlankBytes] *
      normalized->features[kContainedNonBlankBytes];
  for (int j = 0; j < kNumRelevantTags; ++j) {
    features[kRelevantTagPercent + j] =
        features[kRelevantTagCount + j] *
        normalized->features[kRelevantTagCount + j];
  }
}

GoogleString ElementSample::ToString(bool readable, HtmlParse* parser) {
  GoogleString sample_string;
  const char* k = readable ? "" : "'k";
  const char* q = readable ? "" : "'";
  if (role != MobileRole::kInvalid && (!readable || parent->role != role)) {
    StrAppend(
        &sample_string,
        StringPrintf("%srole%s: %s%s%s, ",
                     q, q, q, MobileRole::kMobileRoles[role].value, q));
  }
  StrAppend(
      &sample_string,
      StringPrintf("%sElementTagDepth%s: %.f",
                   k, q, features[kElementTagDepth]));
  if (features[kPreviousTagCount] > 0) {
    StrAppend(
        &sample_string, StringPrintf(
            ", %sPreviousTagCount%s: %.f, %sPreviousTagPercent%s: %.2f",
            k, q, features[kPreviousTagCount],
            k, q, features[kPreviousTagPercent]));
  }
  if (features[kPreviousContentBytes] > 0) {
    StrAppend(
        &sample_string,
        StringPrintf(
            ", %sPreviousContentBytes%s: %.f, %sPreviousContentPercent%s: %.2f"
            ", %sPreviousNonBlankBytes%s: %.f"
            ", %sPreviousNonBlankPercent%s: %.2f",
            k, q, features[kPreviousContentBytes],
            k, q, features[kPreviousContentPercent],
            k, q, features[kPreviousNonBlankBytes],
            k, q, features[kPreviousNonBlankPercent]));
  }
  if (features[kContainedTagCount] > 0) {
    StrAppend(
        &sample_string,
        StringPrintf(
            ", %sContainedTagDepth%s: %.f, %sContainedTagRelativeDepth%s: %.f"
            ", %sContainedTagCount%s: %.f, %sContainedTagPercent%s: %.2f",
            k, q, features[kContainedTagDepth],
            k, q, features[kContainedTagRelativeDepth],
            k, q, features[kContainedTagCount],
            k, q, features[kContainedTagPercent]));
  }
  if (features[kContainedContentBytes] > 0) {
    StrAppend(
        &sample_string,
        StringPrintf(
        ", %sContainedContentBytes%s: %.f, %sContainedContentPercent%s: %.2f"
        ", %sContainedNonBlankBytes%s: %.f"
        ", %sContainedNonBlankPercent%s: %.2f",
        k, q, features[kContainedContentBytes],
        k, q, features[kContainedContentPercent],
        k, q, features[kContainedNonBlankBytes],
        k, q, features[kContainedNonBlankPercent]));
  }
  for (int i = 0; i < kNumAttrStrings; ++i) {
    if (features[kHasAttrString + i] == 1.0) {
      const char* substring = kRelevantAttrSubstrings[i].substring;
      if (readable) {
        StrAppend(&sample_string, ", ", substring, ": 1");
      } else {
        StrAppend(&sample_string,
                  StringPrintf(", 'kHasAttrString + k%c%sAttr': 1",
                               UpperChar(substring[0]), substring+1));
      }
    }
  }
  for (int i = 0; i < kNumRelevantTags; ++i) {
    if (features[kRelevantTagCount + i] > 0) {
      GoogleString tag;
      parser->MakeName(kRelevantTags[i].html_name).value().CopyToString(&tag);
      if (readable) {
        const char* tag_c_str = tag.c_str();
        StrAppend(&sample_string,
                  StringPrintf(", %s count: %.f, %s percent: %.2f",
                               tag_c_str, features[kRelevantTagCount + i],
                               tag_c_str, features[kRelevantTagPercent + i]));
      } else {
        tag[0] = UpperChar(tag[0]);
        const char* tag_c_str = tag.c_str();
        StrAppend(&sample_string,
                  StringPrintf(", 'kRelevantTagCount + k%sTag': %.f"
                               ", 'kRelevantTagPercent + k%sTag': %.f",
                               tag_c_str, features[kRelevantTagCount + i],
                               tag_c_str, features[kRelevantTagPercent + i]));
      }
    }
  }
  return sample_string;
}

MobilizeLabelFilter::MobilizeLabelFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      labeling_mode_(kUseTagNamesAndClassifier) {
  Init();
  Statistics* stats = driver->statistics();
  pages_labeled_ = stats->GetVariable(kPagesLabeled);
  pages_role_added_ = stats->GetVariable(kPagesRoleAdded);
  role_variables_[MobileRole::kKeeper] = NULL;
  role_variables_[MobileRole::kHeader] = stats->GetVariable(kHeaderRoles);
  role_variables_[MobileRole::kNavigational] =
      stats->GetVariable(kNavigationalRoles);
  role_variables_[MobileRole::kContent] = stats->GetVariable(kContentRoles);
  role_variables_[MobileRole::kMarginal] = stats->GetVariable(kMarginalRoles);
  divs_unlabeled_ = stats->GetVariable(kDivsUnlabeled);
  ambiguous_role_labels_ = stats->GetVariable(kAmbiguousRoleLabels);
#ifndef NDEBUG
  CHECK_EQ(kNumRelevantTags, arraysize(kRelevantTags));
  CHECK_EQ(kNumAttrStrings, arraysize(kRelevantAttrSubstrings));
  CheckKeywordsSorted(kIgnoreTags, arraysize(kIgnoreTags));
  CheckTagMetadata();
  CheckAttrSubstrings();
#endif  // #ifndef NDEBUG
}

MobilizeLabelFilter::~MobilizeLabelFilter() {
  DCHECK(samples_.empty());
  DCHECK(sample_stack_.empty());
}

void MobilizeLabelFilter::Init() {
  active_no_traverse_element_ = NULL;
  relevant_tag_depth_ = 0;
  max_relevant_tag_depth_ = 0;
  tag_count_ = 0;
  content_bytes_ = 0;
  content_non_blank_bytes_ = 0;
  were_roles_added_ = false;
}

void MobilizeLabelFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kPagesLabeled);
  statistics->AddVariable(kPagesRoleAdded);
  statistics->AddVariable(kNavigationalRoles);
  statistics->AddVariable(kHeaderRoles);
  statistics->AddVariable(kContentRoles);
  statistics->AddVariable(kMarginalRoles);
  statistics->AddVariable(kDivsUnlabeled);
  statistics->AddVariable(kAmbiguousRoleLabels);
}

void MobilizeLabelFilter::StartDocumentImpl() {
  Init();
  // Set up global sample so that upward aggregation of samples has a base case.
  // It's at virtual tag depth 0 (tags start at tag depth 1).
  MakeNewSample(NULL);
}

void MobilizeLabelFilter::StartElementImpl(HtmlElement* element) {
  if (active_no_traverse_element_ != NULL ||
      IsIgnoreTag(element->keyword())) {
    return;
  }
  if (element->keyword() == HtmlName::kHead) {
    // Ignore all content in document head.  Note: this is potentially unsafe,
    // as browsers will sometimes display content included in HEAD if it looks
    // like the page author included it there by mistake.  But we make the same
    // assumption in the rewrite filter.
    active_no_traverse_element_ = element;
    return;
  }
  if (IsKeeperTag(element->keyword())) {
    // Ignore content in things like <script> and <style> blocks that
    // don't contain user-accessible content.
    active_no_traverse_element_ = element;
    return;
  }
  // We've dropped all the tags we don't even want to look inside.
  // Now decide how interesting the tag might be.
  const RelevantTagMetadata* tag_metadata = FindTagMetadata(element->keyword());
  if (tag_metadata != NULL) {
    // Tag that we want to count (includes all the div-like tags).
    IncrementRelevantTagDepth();
    MobileRole::Level mobile_role =
        (labeling_mode_ == kUseTagNames ||
         labeling_mode_ == kUseTagNamesAndClassifier) ?
        tag_metadata->mobile_role :
        MobileRole::kInvalid;
    if (tag_metadata->is_div_like) {
      HandleDivLikeElement(element, mobile_role);
    }
    if (mobile_role == MobileRole::kInvalid) {
      sample_stack_.back()->
          features[kRelevantTagCount + tag_metadata->relevant_tag]++;
    } else {
      // Note that we do not count role tags (at the moment) because we're using
      // their presence to select training data -- as a result we end up with
      // classifiers that classify first based on the role tags and then fall
      // back to the other criteria we'd like to use.  So instead we count all
      // of these tags as <div>s.
      sample_stack_.back()->
          features[kRelevantTagCount + kDivTag]++;
    }
  }
  ++tag_count_;
}

void MobilizeLabelFilter::HandleDivLikeElement(HtmlElement* element,
                                               MobileRole::Level role) {
  ElementSample* sample = MakeNewSample(element);
  // Handle hand-annotated element.
  HtmlElement::Attribute* mobile_role_attribute =
      element->FindAttribute(HtmlName::kDataMobileRole);
  if (mobile_role_attribute != NULL) {
    sample->role =
        MobileRole::LevelFromString(mobile_role_attribute->escaped_value());
  } else {
    sample->role = role;
  }
  // Now search the attributes for any indicative strings.
  for (int i = 0; i < static_cast<int>(arraysize(kAttrsToSearch)); ++i) {
    StringPiece value(element->AttributeValue(kAttrsToSearch[i]));
    if (!value.empty()) {
      for (int j = 0; j < kNumAttrStrings; ++j) {
        if (FindIgnoreCase(value, kRelevantAttrSubstrings[j].substring) !=
            StringPiece::npos) {
          sample->features[kHasAttrString + j] = 1.0;
        }
      }
    }
  }
}

void MobilizeLabelFilter::EndElementImpl(HtmlElement* element) {
  if (active_no_traverse_element_ != NULL) {
    if (active_no_traverse_element_ == element) {
      active_no_traverse_element_ = NULL;
    }
    return;
  }
  if (IsIgnoreTag(element->keyword())) {
    return;
  }
  if (element == sample_stack_.back()->element) {
    PopSampleStack();
  }
  if (FindTagMetadata(element->keyword()) != NULL) {
    --relevant_tag_depth_;
  }
}

void MobilizeLabelFilter::Characters(HtmlCharactersNode* characters) {
  if (active_no_traverse_element_ != NULL) {
    return;
  }
  // We ignore leading and trailing whitespace when accounting for characters,
  // since long strings of HTML markup often include whitespace for readability,
  // and it generally (though not universally) lacks semantic content.
  StringPiece contents(characters->contents());
  TrimWhitespace(&contents);
  content_bytes_ += contents.size();
  // Now trim characters from the StringPiece, counting only non-whitespace.
  while (!contents.empty()) {
    ++content_non_blank_bytes_;
    contents.remove_prefix(1);
    TrimLeadingWhitespace(&contents);
  }
}

void MobilizeLabelFilter::EndDocument() {
  // The horrifying static cast here avoids warnings under gcc, while allowing a
  // system-dependent return type for .size().
  DCHECK_EQ(static_cast<size_t>(1), sample_stack_.size());
  ElementSample* global = sample_stack_.back();
  sample_stack_.pop_back();
  ComputeContained(global);
  pages_labeled_->Add(1);
  // Now that we have global information, compute sample that require
  // normalization (eg percent of links in page, percent of text, etc.).
  // Use this to label the DOM elements.
  ComputeProportionalFeatures();
  Label();
  if (were_roles_added_) {
    pages_role_added_->Add(1);
  }
  if (driver()->DebugMode()) {
    DebugLabel();
  }
  SanityCheckEndOfDocumentState();
  STLDeleteElements(&samples_);
}

void MobilizeLabelFilter::SetMobileRole(HtmlElement* element,
                                        MobileRole::Level role) {
  if (!driver()->IsRewritable(element)) {
    LOG(WARNING) << "Dropped mobile role annotation due to flush.";
    return;
  }
  if (element->FindAttribute(HtmlName::kDataMobileRole) != NULL) {
    return;
  }
  driver()->AddEscapedAttribute(element, HtmlName::kDataMobileRole,
                                MobileRole::StringFromLevel(role));
  were_roles_added_ = true;
  role_variables_[role]->Add(1);
}

ElementSample* MobilizeLabelFilter::MakeNewSample(HtmlElement* element) {
  // Effectively a factory for MobileFeature, uses a bunch of filter state.
  ElementSample* result =
      new ElementSample(relevant_tag_depth_, tag_count_,
                        content_bytes_, content_non_blank_bytes_);
  if (element != NULL) {
    // Non-global sample.  This relies on the existence of a global sample with
    // element==NULL at the front of sample_stack to ensure there's a parent
    // element available.
    result->element = element;
    result->parent = sample_stack_.back();
    result->role = result->parent->role;
  }
  samples_.push_back(result);
  sample_stack_.push_back(result);
  return result;
}

void MobilizeLabelFilter::PopSampleStack() {
  ElementSample* popped(sample_stack_.back());
  sample_stack_.pop_back();
  ComputeContained(popped);
  // Aggregate statistics of popped child into parent.
  AggregateToTopOfStack(popped);
}

void MobilizeLabelFilter::ComputeContained(ElementSample* sample) {
  // Update contained counts now that element is complete.
  sample->features[kContainedTagCount] =
      tag_count_ - sample->features[kPreviousTagCount];
  sample->features[kContainedContentBytes] =
      content_bytes_ - sample->features[kPreviousContentBytes];
  sample->features[kContainedNonBlankBytes] =
      content_non_blank_bytes_ - sample->features[kPreviousNonBlankBytes];
}

void MobilizeLabelFilter::AggregateToTopOfStack(ElementSample* sample) {
  // Assumes sample was just popped, and aggregates its data to the sample
  // at the top of the stack.
  ElementSample* parent = sample_stack_.back();
  parent->features[kContainedTagDepth] =
      std::max(parent->features[kContainedTagDepth],
               sample->features[kContainedTagDepth]);
  for (int i = 0; i < kNumRelevantTags; ++i) {
    parent->features[kRelevantTagCount + i] +=
        sample->features[kRelevantTagCount + i];
  }
}

void MobilizeLabelFilter::IncrementRelevantTagDepth() {
  if (++relevant_tag_depth_ > max_relevant_tag_depth_) {
    max_relevant_tag_depth_ = relevant_tag_depth_;
  }
  if (relevant_tag_depth_ >
      sample_stack_.back()->features[kContainedTagDepth]) {
    sample_stack_.back()->features[kContainedTagDepth] = relevant_tag_depth_;
  }
}

void MobilizeLabelFilter::SanityCheckEndOfDocumentState() {
#ifndef NDEBUG
  CHECK(sample_stack_.empty());
  CHECK_EQ(0, relevant_tag_depth_);
  // The horrifying static cast here avoids warnings under gcc, while allowing a
  // system-dependent return type for .size().
  CHECK_LE(static_cast<typeof(samples_.size())>(1), samples_.size());
  ElementSample* global = samples_[0];
  CHECK_EQ(0, global->features[kElementTagDepth]);
  CHECK_EQ(0, global->features[kPreviousTagCount]);
  CHECK_EQ(0, global->features[kPreviousContentBytes]);
  CHECK_EQ(0, global->features[kPreviousNonBlankBytes]);
  CHECK_EQ(max_relevant_tag_depth_, global->features[kContainedTagDepth]);
  CHECK_EQ(tag_count_, global->features[kContainedTagCount]);
  CHECK_EQ(content_bytes_, global->features[kContainedContentBytes]);
  CHECK_EQ(content_non_blank_bytes_, global->features[kContainedNonBlankBytes]);
  // Just for consistency, we muck with the global tag counts so the counts are
  // monotonic below (but we do it here rather than at the beginning so it
  // doesn't disrupt the global count of contained tags).  This allows us to
  // deal with documents with a single enclosing global <div> (say) that
  // encloses all the actual content.
  global->features[kPreviousTagCount] = -1;
  global->features[kContainedTagCount]++;
  for (int i = 1, n = samples_.size(); i < n; ++i) {
    ElementSample* sample = samples_[i];
    CHECK(sample != NULL);
    ElementSample* parent = sample->parent;
    CHECK(sample->element != NULL);
    CHECK(parent != NULL);
    CHECK_LT(parent->features[kElementTagDepth],
             sample->features[kElementTagDepth]);
    CHECK_LT(parent->features[kPreviousTagCount],
             sample->features[kPreviousTagCount]);
    CHECK_LE(parent->features[kPreviousContentBytes],
             sample->features[kPreviousContentBytes]);
    CHECK_LE(parent->features[kPreviousNonBlankBytes],
             sample->features[kPreviousNonBlankBytes]);
    CHECK_GE(parent->features[kContainedTagDepth],
             sample->features[kContainedTagDepth]);
    CHECK_GT(parent->features[kContainedTagCount],
             sample->features[kContainedTagCount]);
    CHECK_GE(parent->features[kContainedContentBytes],
             sample->features[kContainedContentBytes]);
    CHECK_GE(parent->features[kContainedNonBlankBytes],
             sample->features[kContainedNonBlankBytes]);
  }
  global->features[kPreviousTagCount] = 0;
  global->features[kContainedTagCount]--;
#endif  // #ifndef NDEBUG
}

void MobilizeLabelFilter::ComputeProportionalFeatures() {
  ElementSample* global = samples_[0];
  ElementSample normalized(0, 0, 0, 0);
  for (int i = 1; i < kNumFeatures; ++i) {
    if (global->features[i] > 0.0) {
      normalized.features[i] = 100.00 / global->features[i];
    } else {
      normalized.features[i] = 0.0;
    }
  }
  for (int i = 1, n = samples_.size(); i < n; ++i) {
    CHECK_LT(0, global->features[kContainedTagCount]);
    ElementSample* sample = samples_[i];
    sample->ComputeProportionalFeatures(&normalized);
  }
}

void MobilizeLabelFilter::Label() {
  DecisionTree navigational(kNavigationalTree, kNavigationalTreeSize);
  DecisionTree header(kHeaderTree, kHeaderTreeSize);
  DecisionTree content(kContentTree, kContentTreeSize);
  int n = samples_.size();

  // Default classification to carry down tree.
  samples_[0]->role =
      (labeling_mode_ == kUseClassifier ||
       labeling_mode_ == kUseTagNamesAndClassifier) ?
      MobileRole::kKeeper : MobileRole::kInvalid;
  // Now classify in opening tag order (parents before children).
  for (int i = 1; i < n; ++i) {
    ElementSample* sample = samples_[i];
    if (sample->role != MobileRole::kInvalid) {
      // Hand-labeled or HTML5.
      continue;
    }
    if (labeling_mode_ == kUseClassifier ||
        labeling_mode_ == kUseTagNamesAndClassifier) {
      double navigational_confidence = navigational.Predict(sample->features);
      bool is_navigational =
          navigational_confidence >= kNavigationalTreeThreshold;
      double header_confidence = header.Predict(sample->features);
      bool is_header = header_confidence >= kHeaderTreeThreshold;
      double content_confidence = content.Predict(sample->features);
      bool is_content = content_confidence >= kContentTreeThreshold;
      // If exactly one classification is chosen, use that.
      if (is_navigational) {
        if (!is_header && !is_content) {
          sample->role = MobileRole::kNavigational;
        } else {
          ambiguous_role_labels_->Add(1);
        }
      } else if (is_header) {
        if (!is_content) {
          sample->role = MobileRole::kHeader;
        } else {
          ambiguous_role_labels_->Add(1);
        }
      } else if (is_content) {
        sample->role = MobileRole::kContent;
      }
    }
    if (sample->role == MobileRole::kInvalid) {
      // No or ambiguous classification.  Carry over from parent.
      sample->role = sample->parent->role;
    }
  }
  // All unclassified nodes have been labeled with kKeeper using parent
  // propagation.  We want to mark as kMarginal all the kKeeper nodes whose
  // children are also Marginal.  If a node is labeled kKeeper and any child is
  // non-Marginal we want to mark it kInvalid.  We work in reverse DOM order,
  // basically invalidating parents of non-marginal content.
  for (int i = n - 1; i > 0; --i) {
    ElementSample* sample = samples_[i];
    if (sample->role == MobileRole::kKeeper) {
        sample->role = MobileRole::kMarginal;
    } else if (sample->role != MobileRole::kMarginal &&
               sample->parent->role == MobileRole::kKeeper) {
      // Non-marginal child with keeper parent; parent is invalid.
      sample->parent->role = MobileRole::kInvalid;
    }
  }
  // Finally, go through the nodes in DOM order and actually add labels at
  // mobile role transition points.
  samples_[0]->role = MobileRole::kInvalid;
  for (int i = 1; i < n; ++i) {
    ElementSample* sample = samples_[i];
    DCHECK_NE(MobileRole::kKeeper, sample->role);
    if (sample->role != MobileRole::kInvalid &&
        sample->role != sample->parent->role) {
      SetMobileRole(sample->element, sample->role);
    } else {
      divs_unlabeled_->Add(1);
    }
  }
}

void MobilizeLabelFilter::DebugLabel() {
  // Map relevant attr keywords back to the corresponding string.  Sadly
  // html_name doesn't give us an easy mechanism for accomplishing this
  // transformation.
  for (int i = 1, n = samples_.size(); i < n; ++i) {
    ElementSample* sample = samples_[i];
    // TODO(jmaessen): Have a better format for the sample string,
    // and control sample string dumping independently of debug.
    if (driver()->DebugMode()) {
      driver()->InsertDebugComment(
          sample->ToString(true /* readable */, driver()), sample->element);
      GoogleString sample_string =
          sample->ToString(false /* numeric */, driver());
      driver()->message_handler()->Message(
          kInfo, "%s: %s { %s }",
          driver()->url(),
          sample->element->name_str().as_string().c_str(),
          sample_string.c_str());
    }
  }
}

}  // namespace net_instaweb
