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
#include "net/instaweb/rewriter/public/add_ids_filter.h"
#include "net/instaweb/rewriter/public/decision_tree.h"
#include "net/instaweb/rewriter/public/mobilize_decision_trees.h"
#include "net/instaweb/rewriter/public/mobilize_rewrite_filter.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/escaping.h"
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

const char kNbsp[] = "&nbsp;";

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
// Entries with trailing comment are potentially useless and
// being monitored for removal.
const RelevantTagMetadata kRelevantTags[] = {
  /* tag name            tag symbol    div_like?   role */
  { HtmlName::kA,        kATag,        false,    MobileRole::kUnassigned },
  { HtmlName::kArticle,  kArticleTag,  true,     MobileRole::kContent },
  { HtmlName::kAside,    kAsideTag,    true,     MobileRole::kMarginal },
  { HtmlName::kButton,   kButtonTag,   false,    MobileRole::kUnassigned },
  { HtmlName::kContent,  kContentTag,  true,     MobileRole::kContent },
  { HtmlName::kDatalist, kDatalistTag, false,    MobileRole::kUnassigned },  //
  { HtmlName::kDiv,      kDivTag,      true,     MobileRole::kUnassigned },
  { HtmlName::kFieldset, kFieldsetTag, false,    MobileRole::kUnassigned },
  { HtmlName::kFooter,   kFooterTag,   true,     MobileRole::kMarginal },
  { HtmlName::kForm,     kFormTag,     true,     MobileRole::kUnassigned },
  { HtmlName::kH1,       kH1Tag,       false,    MobileRole::kUnassigned },
  { HtmlName::kH2,       kH2Tag,       false,    MobileRole::kUnassigned },
  { HtmlName::kH3,       kH3Tag,       false,    MobileRole::kUnassigned },
  { HtmlName::kH4,       kH4Tag,       false,    MobileRole::kUnassigned },
  { HtmlName::kH5,       kH5Tag,       false,    MobileRole::kUnassigned },
  { HtmlName::kH6,       kH6Tag,       false,    MobileRole::kUnassigned },
  { HtmlName::kHeader,   kHeaderTag,   true,     MobileRole::kHeader },
  { HtmlName::kImg,      kImgTag,      false,    MobileRole::kUnassigned },
  { HtmlName::kInput,    kInputTag,    false,    MobileRole::kUnassigned },
  { HtmlName::kLegend,   kLegendTag,   false,    MobileRole::kUnassigned },  //
  { HtmlName::kLi,       kLiTag,       false,    MobileRole::kUnassigned },
  { HtmlName::kMain,     kMainTag,     true,     MobileRole::kContent },
  { HtmlName::kMenu,     kMenuTag,     true,     MobileRole::kNavigational },
  { HtmlName::kNav,      kNavTag,      true,     MobileRole::kNavigational },
  { HtmlName::kOptgroup, kOptgroupTag, false,    MobileRole::kUnassigned },  //
  { HtmlName::kOption,   kOptionTag,   false,    MobileRole::kUnassigned },
  { HtmlName::kP,        kPTag,        false,    MobileRole::kUnassigned },
  { HtmlName::kSection,  kSectionTag,  true,     MobileRole::kUnassigned },
  { HtmlName::kSelect,   kSelectTag,   false,    MobileRole::kUnassigned },  //
  { HtmlName::kSpan,     kSpanTag,     false,    MobileRole::kUnassigned },
  { HtmlName::kTextarea, kTextareaTag, false,    MobileRole::kUnassigned },
  { HtmlName::kUl,       kUlTag,       true,     MobileRole::kUnassigned },
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
  {kArticleAttr, "article"},   // Useless?
  {kAsideAttr,   "aside"},     // Useless?
  {kBannerAttr,  "banner"},
  {kBarAttr,     "bar"},
  {kBodyAttr,    "body"},      // Useless?
  {kBotAttr,     "bot"},
  {kCenterAttr,  "center"},    // Useless?
  {kColAttr,     "col"},
  {kCommentAttr, "comment"},
  {kContentAttr, "content"},
  {kFindAttr,    "find"},      // Useless?
  {kFootAttr,    "foot"},
  {kHdrAttr,     "hdr"},       // Useless?
  {kHeadAttr,    "head"},
  {kLeftAttr,    "left"},      // Useless?
  {kLogoAttr,    "logo"},
  {kMainAttr,    "main"},      // Useless?
  {kMarginAttr,  "margin"},    // Useless?
  {kMenuAttr,    "menu"},
  {kMidAttr,     "mid"},
  {kNavAttr,     "nav"},
  {kPostAttr,    "post"},
  {kRightAttr,   "right"},     // Useless?
  {kSearchAttr,  "search"},
  {kSecAttr,     "sec"},
  {kTitleAttr,   "title"},     // Useless?
  {kTopAttr,     "top"},
  {kWrapAttr,    "wrap"},
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
      CHECK_EQ(MobileRole::kUnassigned, kRelevantTags[i].mobile_role);
    }
  }
}

void CheckAttrSubstrings() {
  for (int i = 0; i < kNumAttrStrings; ++i) {
    MobileAttrSubstring id = static_cast<MobileAttrSubstring>(i);
    CHECK_EQ(id, kRelevantAttrSubstrings[i].id);
    const char* string = kRelevantAttrSubstrings[i].substring;
    CHECK(string != NULL) << i;
    CHECK_LT(1, static_cast<uint64>(strlen(string)))
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

int CountNonWhitespaceChars(const StringPiece contents) {
  int result = 0;
  for (stringpiece_ssize_type i = 0; i < contents.size(); ++i) {
    if (!IsHtmlSpace(contents[i])) {
      result++;
    }
  }
  return result;
}

bool TrimLeadingWhitespaceAndNbsp(StringPiece* str) {
  bool trimmed = false;
  while (!str->empty()) {
    if (IsHtmlSpace(str->data()[0])) {
      trimmed = true;
      str->remove_prefix(1);
    } else if (str->starts_with(kNbsp)) {
      trimmed = true;
      str->remove_prefix(STATIC_STRLEN(kNbsp));
    } else {
      break;
    }
  }
  return trimmed;
}

bool TrimTrailingWhitespaceAndNbsp(StringPiece* str) {
  bool trimmed = false;
  while (!str->empty()) {
    if (IsHtmlSpace(str->data()[str->size() - 1])) {
      trimmed = true;
      str->remove_suffix(1);
    } else if (str->ends_with(kNbsp)) {
      trimmed = true;
      str->remove_suffix(STATIC_STRLEN(kNbsp));
    } else {
      break;
    }
  }
  return trimmed;
}

bool TrimWhitespaceAndNbsp(StringPiece* str) {
  return (TrimLeadingWhitespaceAndNbsp(str) |
          TrimTrailingWhitespaceAndNbsp(str));
}

inline bool IsRoleValid(MobileRole::Level role) {
  // Equivalent to role != kInvalid && role != kUnassigned.
  return role < MobileRole::kInvalid;
}

// A *simple* ASCII-only capitalization function for known lower-case strings.
// Used for output, or this would be the slow way to accomplish this task.
GoogleString Capitalize(StringPiece s) {
  GoogleString result;
  s.CopyToString(&result);
  if (!result.empty()) {
    result[0] = UpperChar(result[0]);
  }
  return result;
}

}  // namespace

ElementSample::ElementSample(int relevant_tag_depth, int tag_count,
                             int content_bytes, int content_non_blank_bytes)
    : element(NULL),
      parent(NULL),
      role(MobileRole::kUnassigned),
      propagated_role(MobileRole::kUnassigned),
      explicitly_labeled(false),
      explicitly_non_nav(false),
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
  if (IsRoleValid(role) && (!readable || parent->role != role)) {
    StrAppend(
        &sample_string,
        StringPrintf("%srole%s: %s%s%s, ",
                     q, q, q, MobileRoleData::StringFromLevel(role), q));
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
  if (features[kContainedAContentBytes] > 0) {
    StrAppend(
        &sample_string,
        StringPrintf(
            ", %sContainedAContentBytes%s: %.f"
            ", %sContainedAContentLocalPercent%s: %.2f",
            k, q, features[kContainedAContentBytes],
            k, q, features[kContainedAContentLocalPercent]));
  }
  if (features[kContainedNonAContentBytes] > 0) {
    StrAppend(
        &sample_string,
        StringPrintf(
            ", %sContainedNonAContentBytes%s: %.f",
            k, q, features[kContainedNonAContentBytes]));
  }
  if (features[kContainedAImgTag] > 0) {
    StrAppend(
        &sample_string,
        StringPrintf(
            ", %sContainedAImgTag%s: %.f, %sContainedAImgLocalPercent%s: %.2f",
            k, q, features[kContainedAImgTag],
            k, q, features[kContainedAImgLocalPercent]));
  }
  if (features[kContainedNonAImgTag] > 0) {
    StrAppend(
        &sample_string,
        StringPrintf(
            ", %sContainedNonAImgTag%s: %.f",
            k, q, features[kContainedNonAImgTag]));
  }
  for (int i = 0; i < kNumAttrStrings; ++i) {
    if (features[kHasAttrString + i] == 1.0) {
      const char* substring = kRelevantAttrSubstrings[i].substring;
      if (readable) {
        StrAppend(&sample_string, ", ", substring, ": 1");
      } else {
        StrAppend(&sample_string,
                  ", 'kHasAttrString + k", Capitalize(substring), "Attr': 1");
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
        GoogleString capitalized = Capitalize(tag);
        const char* tag_c_str = capitalized.c_str();
        StrAppend(&sample_string,
                  StringPrintf(", 'kRelevantTagCount + k%sTag': %.f"
                               ", 'kRelevantTagPercent + k%sTag': %.f",
                               tag_c_str, features[kRelevantTagCount + i],
                               tag_c_str, features[kRelevantTagPercent + i]));
      }
    }
  }
  for (int i = 0; i < MobileRole::kMarginal; ++i) {
    MobileRole::Level role = static_cast<MobileRole::Level>(i);
    if (features[kParentRoleIs + role] > 0) {
      const char* role_name = MobileRoleData::StringFromLevel(role);
      if (readable) {
        StrAppend(
            &sample_string,
            StringPrintf(", parent role is %s", role_name));
      } else {
        StrAppend(
            &sample_string,
            ", 'kParentRoleIs + MobileRole::k", Capitalize(role_name), "': 1");
      }
    }
  }
  return sample_string;
}

MobilizeLabelFilter::MobilizeLabelFilter(RewriteDriver* driver)
    : CommonFilter(driver) {
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
  link_depth_ = 0;
  tag_count_ = 0;
  content_bytes_ = 0;
  content_non_blank_bytes_ = 0;
  were_roles_added_ = false;
  nav_classes_.clear();
  non_nav_classes_.clear();
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

void MobilizeLabelFilter::GetClassesFromOptions(const RewriteOptions* options) {
  const GoogleString& nav_classes = options->mob_nav_classes();
  if (nav_classes.empty()) {
    return;
  }
  StringPieceVector classes;
  SplitStringPieceToVector(nav_classes, ",", &classes, true /* Skip empty */);
  for (int i = 0, n = classes.size(); i < n; ++i) {
    StringPiece& element = classes[i];
    TrimWhitespace(&element);
    if (element.empty()) {
      continue;
    }
    if (element[0] == '-') {
      element.remove_prefix(1);
      non_nav_classes_.insert(element);
    } else {
      if (element[0] == '+') {
        element.remove_prefix(1);
      }
      nav_classes_.insert(element);
    }
  }
}

void MobilizeLabelFilter::StartDocumentImpl() {
  Init();
  GetClassesFromOptions(driver()->options());
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
  HandleElementWithMetadata(element);
  if (!nav_classes_.empty() || !non_nav_classes_.empty()) {
    HandleExplicitlyConfiguredElement(element);
  }
  ++tag_count_;
}

void MobilizeLabelFilter::HandleElementWithMetadata(HtmlElement* element) {
  const RelevantTagMetadata* tag_metadata = FindTagMetadata(element->keyword());
  if (tag_metadata == NULL) {
    return;
  }
  if (element->keyword() == HtmlName::kA) {
    ++link_depth_;
  } else if (element->keyword() == HtmlName::kImg) {
    // Track whether this img is inside or outside an <a> tag.
    FeatureName contained_a_img_feature =
        link_depth_ > 0 ? kContainedAImgTag : kContainedNonAImgTag;
    sample_stack_.back()->features[contained_a_img_feature]++;
  }
  // Tag that we want to count (includes all the div-like tags).
  IncrementRelevantTagDepth();
  MobileRole::Level mobile_role = tag_metadata->mobile_role;
  if (tag_metadata->is_div_like) {
    HandleDivLikeElement(element, mobile_role);
  }
  if (!IsRoleValid(mobile_role)) {
    sample_stack_.back()->
        features[kRelevantTagCount + tag_metadata->relevant_tag]++;
  } else {
    // Note that we do not count role tags (at the moment) because we're using
    // their presence to select training data -- as a result we end up with
    // classifiers that classify first based on the role tags and then fall back
    // to the other criteria we'd like to use.  So instead we count all of these
    // tags as <div>s.
    sample_stack_.back()->
        features[kRelevantTagCount + kDivTag]++;
  }
}

void MobilizeLabelFilter::HandleDivLikeElement(HtmlElement* element,
                                               MobileRole::Level role) {
  ElementSample* sample = MakeNewSample(element);
  // Handle hand-annotated element.
  HtmlElement::Attribute* mobile_role_attribute =
      element->FindAttribute(HtmlName::kDataMobileRole);
  if (mobile_role_attribute != NULL) {
    sample->role =
        MobileRoleData::LevelFromString(mobile_role_attribute->escaped_value());
    sample->explicitly_labeled = true;
  } else {
    sample->role = role;
    if (role != MobileRole::kUnassigned) {
      // DOM element determined the label already.
      sample->explicitly_labeled = true;
    }
  }
  // Now search the attributes for any indicative strings.
  for (int i = 0; i < static_cast<int>(arraysize(kAttrsToSearch)); ++i) {
    HtmlName::Keyword attr = kAttrsToSearch[i];
    StringPiece value(element->EscapedAttributeValue(attr));
    if (value.empty()) {
      continue;
    }
    if (attr == HtmlName::kId &&
        value.starts_with(AddIdsFilter::kClassPrefix)) {
      // Ignore PageSpeed-inserted ids.
      continue;
    }
    for (int j = 0; j < kNumAttrStrings; ++j) {
      if (FindIgnoreCase(value, kRelevantAttrSubstrings[j].substring) !=
          StringPiece::npos) {
        sample->features[kHasAttrString + j] = 1.0;
      }
    }
  }
}

void MobilizeLabelFilter::HandleExplicitlyConfiguredElement(
    HtmlElement* element) {
  // User configuration can force us to label an element as navigational (or
  // non-navigational) based on its id or class.
  // id matches take precedence, and exclusions take precedence over inclusions.
  StringPiece id(element->EscapedAttributeValue(HtmlName::kId));
  if (!id.empty()) {
    if (non_nav_classes_.count(id) != 0) {
      ExplicitlyConfigureRole(MobileRole::kUnassigned, element);
      return;
    } else if (nav_classes_.count(id) != 0) {
      ExplicitlyConfigureRole(MobileRole::kNavigational, element);
      return;
    }
  }
  StringPiece class_attr(element->EscapedAttributeValue(HtmlName::kClass));
  if (!class_attr.empty()) {
    StringPieceVector classes;
    SplitStringPieceToVector(class_attr, " ", &classes, true /* Skip empty */);
    for (int i = 0, n = classes.size(); i < n; ++i) {
      if (non_nav_classes_.count(classes[i]) != 0) {
        ExplicitlyConfigureRole(MobileRole::kUnassigned, element);
        return;
      } else if (nav_classes_.count(classes[i]) != 0) {
        ExplicitlyConfigureRole(MobileRole::kNavigational, element);
        // Keep checking for exclusions.
      }
    }
  }
}

// Return a sample for *element, which will be the most recently created sample
// or (if that's not a sample for *element) a fresh sample.
void MobilizeLabelFilter::ExplicitlyConfigureRole(
    MobileRole::Level role, HtmlElement* element) {
  ElementSample* sample = (sample_stack_.back()->element == element) ?
      sample_stack_.back() : MakeNewSample(element);
  sample->explicitly_labeled = true;
  sample->explicitly_non_nav = role != MobileRole::kNavigational;
  sample->role = role;
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
    ElementSample* sample_to_delete = NULL;
    if (link_depth_ > 0 &&
        samples_.back() == sample_stack_.back() &&
        samples_.back()->features[kRelevantTagCount + kATag] == 0.0) {
      // Throw away samples that occur within a link (unless nested links
      // strongly suggest this is a link in error).  Avoids nav-classifying text
      // fragments or individual images.  We still need to aggregate the nested
      // statistics to the parent node, though, which is done by PopSampleStack.
      sample_to_delete = samples_.back();
      UnlabelledDiv(sample_to_delete);
      samples_.pop_back();
    }
    PopSampleStack();
    delete sample_to_delete;
  }
  if (FindTagMetadata(element->keyword()) != NULL) {
    --relevant_tag_depth_;
  }
  if (element->keyword() == HtmlName::kA) {
    --link_depth_;
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
  TrimWhitespaceAndNbsp(&contents);
  int content_nbsp_count = CountSubstring(contents, kNbsp);
  int content_size_adjustment = content_nbsp_count * (STATIC_STRLEN(kNbsp) - 1);
  content_bytes_ += contents.size() - content_size_adjustment;
  FeatureName contained_a_content_bytes_feature =
      link_depth_ > 0 ? kContainedAContentBytes : kContainedNonAContentBytes;
  sample_stack_.back()->features[contained_a_content_bytes_feature] +=
      contents.size() - content_size_adjustment;
  content_non_blank_bytes_ +=
      CountNonWhitespaceChars(contents) -
      (content_nbsp_count + content_size_adjustment);
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
  if (driver()->DebugMode() ||
      driver()->options()->log_mobilization_samples()) {
    DebugLabel();
  }
  SanityCheckEndOfDocumentState();
  if (driver()->options()->mob_always() ||
      driver()->request_properties()->IsMobile() ||
      driver()->DebugMode()) {
    InjectLabelJavascript();
  } else {
    // TODO(jmaessen): Consider disabling this filter *and* add_ids if we don't
    // need them.  But note that we are likely to want to instrument desktop
    // page views once we start to beacon back information for mobilizing pages.
    NonMobileUnlabel();
  }
  if (were_roles_added_) {
    pages_role_added_->Add(1);
  }
  STLDeleteElements(&samples_);
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
    const HtmlElement::Attribute* id = element->FindAttribute(HtmlName::kId);
    if (id == NULL) {
      driver()->InfoAt(NULL, "%s element lacks an id!",
                       element->name_str().as_string().c_str());
      LOG(DFATAL) << "Element lacks an id!";
    } else {
      const char* value = id->escaped_value();
      if (value != NULL) {
        result->id.assign(value);
      }
    }
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
  double a_content_bytes = sample->features[kContainedAContentBytes];
  if (a_content_bytes > 0.0) {
    sample->features[kContainedAContentLocalPercent] =
        100.0 * (a_content_bytes /
                 (a_content_bytes +
                  sample->features[kContainedNonAContentBytes]));
  }
  double a_img_tag = sample->features[kContainedAImgTag];
  if (a_img_tag > 0.0) {
    sample->features[kContainedAImgLocalPercent] =
        100.0 * (a_img_tag /
                 (a_img_tag + sample->features[kContainedNonAImgTag]));
  }
}

void MobilizeLabelFilter::AggregateToTopOfStack(ElementSample* sample) {
  // Assumes sample was just popped, and aggregates its data to the sample
  // at the top of the stack.
  ElementSample* parent = sample_stack_.back();
  parent->features[kContainedTagDepth] =
      std::max(parent->features[kContainedTagDepth],
               sample->features[kContainedTagDepth]);
  parent->features[kContainedAContentBytes] +=
      sample->features[kContainedAContentBytes];
  parent->features[kContainedNonAContentBytes] +=
      sample->features[kContainedNonAContentBytes];
  parent->features[kContainedAImgTag] +=
      sample->features[kContainedAImgTag];
  parent->features[kContainedNonAImgTag] +=
      sample->features[kContainedNonAImgTag];
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
  CHECK_LE(1, static_cast<uint64>(samples_.size()));
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
    CHECK_NE(MobileRole::kKeeper, sample->role);
    CHECK_NE(MobileRole::kUnassigned, sample->role);
    CHECK_LE(parent->features[kElementTagDepth],
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
    CHECK_GE(parent->features[kContainedAContentBytes],
             sample->features[kContainedAContentBytes]);
    CHECK_GE(parent->features[kContainedNonAContentBytes],
             sample->features[kContainedNonAContentBytes]);
    CHECK_GE(parent->features[kContainedAImgTag],
             sample->features[kContainedAImgTag]);
    CHECK_GE(parent->features[kContainedNonAImgTag],
             sample->features[kContainedNonAImgTag]);
    for (int i = 0; i < kNumRelevantTags; ++i) {
      CHECK_GE(parent->features[kRelevantTagCount + i],
               sample->features[kRelevantTagCount + i]);
    }
    for (int i = 0; i < MobileRole::kMarginal; ++i) {
      MobileRole::Level role = static_cast<MobileRole::Level>(i);
      if (sample->features[kParentRoleIs + role] != 0) {
        // Must have been propagated from parent.
        CHECK_EQ(role, parent->role)
            << MobileRoleData::StringFromLevel(role);
      } else if (parent->role == role) {
        // parent->role must have been set by parent propagation,
        // so our role must match.
        CHECK_EQ(role, sample->role)
            << MobileRoleData::StringFromLevel(role);
      }
    }
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
  bool log_samples = driver()->options()->log_mobilization_samples();
  DecisionTree navigational(kNavigationalTree, kNavigationalTreeSize);
  DecisionTree header(kHeaderTree, kHeaderTreeSize);
  DecisionTree content(kContentTree, kContentTreeSize);
  int n = samples_.size();

  // Default classification to carry down tree.
  samples_[0]->role = MobileRole::kUnassigned;
  // Now classify in opening tag order (parents before children)
  for (int i = 1; i < n; ++i) {
    ElementSample* sample = samples_[i];
    if (sample->parent->role < MobileRole::kMarginal) {
      // Set appropriate kParentRoleIs feature.  This must be done for all
      // samples or we can't use the feature for training.  We can't do it until
      // we get here because the parent node must have been classified.
      sample->features[kParentRoleIs + sample->parent->role] = 1.0;
    }
    if (IsRoleValid(sample->role)) {
      // Hand-labeled or HTML5.
      continue;
    }
    if (!sample->explicitly_non_nav) {
      sample->explicitly_non_nav = sample->parent->explicitly_non_nav;
    }
    // The way navigation extraction currently works, we take the entire DOM
    // rooted at the point marked navigational.  To reflect that fact, once our
    // parent sample is navigational we fall through to parent->child
    // propagation.  Similarly, when log_samples is on we are collecting
    // training data and don't classify, we just propagate the information
    // obtained from HTML5 tags in the DOM.
    if (sample->parent->role != MobileRole::kNavigational && !log_samples) {
      double navigational_confidence = navigational.Predict(sample->features);
      bool is_navigational =
          navigational_confidence >= kNavigationalTreeThreshold;
      double header_confidence = header.Predict(sample->features);
      bool is_header = header_confidence >= kHeaderTreeThreshold;
      double content_confidence = content.Predict(sample->features);
      bool is_content = content_confidence >= kContentTreeThreshold;
      // If exactly one classification is chosen, use that.
      if (is_navigational && !sample->explicitly_non_nav) {
        if (!is_header && !is_content) {
          sample->role = MobileRole::kNavigational;
        } else {
          ambiguous_role_labels_->Add(1);
          if (is_header) {
            driver()->message_handler()->Message(
                kInfo, "Both navigational and header");
          }
          if (is_content) {
            driver()->message_handler()->Message(
                kInfo, "Both navigational and content");
          }
        }
      } else if (is_header) {
        if (!is_content) {
          sample->role = MobileRole::kHeader;
        } else {
          ambiguous_role_labels_->Add(1);
          driver()->message_handler()->Message(
              kInfo, "Both header and content");
        }
      } else if (is_content) {
        sample->role = MobileRole::kContent;
      }
    }
    if (!IsRoleValid(sample->role)) {
      // No or ambiguous classification.  Carry over from parent.
      sample->role = sample->parent->role;
    }
  }
  // All unclassified nodes have been labeled with kUnassigned using parent
  // propagation.  Now do upward propagation from labeled nodes to their parent:
  // if all the children of a node are unlabeled or share the same label, the
  // parent gets that label.  If a leaf is unlabeled, it's marginal.
  for (int i = n - 1; i > 0; --i) {
    // Reverse tag order, from leaves to root.
    ElementSample* sample = samples_[i];
    ElementSample* parent = sample->parent;
    // Meaning of sample->propagated_label at this point:
    //   kInvalid if children have multiple labels from kHeader..kMarginal.
    //   kHeader..kContent if at least one child had that label.
    //   kMarginal if at least one child was *explicitly* labeled that.
    //   kUnassigned if all children unassigned.
    // Meaning of parent->propagated_label at this point is the same, but only
    //   accounts for the children we've previously seen.
    // At end of loop body, sample->label should reflect
    //   sample->propagated_label if it started as kUnassigned
    //   and parent->propagated_label should account for sample->label.
    MobileRole::Level role_to_parent = sample->role;
    // First decide label of sample based on what children
    // have propagated to propagated_role.
    if (sample->role == MobileRole::kUnassigned) {
      role_to_parent = sample->propagated_role;
      if (role_to_parent == MobileRole::kUnassigned ||
          (sample->explicitly_non_nav &&
           role_to_parent == MobileRole::kNavigational)) {
        sample->role = MobileRole::kMarginal;
      } else {
        sample->role = role_to_parent;
      }
    }
    if (role_to_parent != MobileRole::kUnassigned) {
      if (parent->propagated_role == role_to_parent) {
        // no change.
      } else if (parent->propagated_role == MobileRole::kUnassigned) {
        parent->propagated_role = role_to_parent;
      } else {
        // Conflict.
        parent->propagated_role = MobileRole::kInvalid;
      }
    }
  }
  // For consistency, label the root as invalid so that no unassigned samples
  // remain.
  samples_[0]->role = MobileRole::kInvalid;
}

void MobilizeLabelFilter::DebugLabel() {
  bool debug_mode = driver()->DebugMode();
  bool log_samples = driver()->options()->log_mobilization_samples();
  if (!debug_mode && !log_samples) {
    return;
  }
  // Map relevant attr keywords back to the corresponding string.  Sadly
  // html_name doesn't give us an easy mechanism for accomplishing this
  // transformation.
  for (int i = 1, n = samples_.size(); i < n; ++i) {
    ElementSample* sample = samples_[i];
    HtmlElement* element = sample->element;
    if (debug_mode) {
      if (sample->role != sample->parent->role &&
          driver()->IsRewritable(element) &&
          element->FindAttribute(HtmlName::kDataMobileRole) == NULL) {
        // Add mobile role annotation in place where possible.
        driver()->AddEscapedAttribute(
            element, HtmlName::kDataMobileRole,
            MobileRoleData::StringFromLevel(sample->role));
      }
      driver()->InsertDebugComment(
          sample->ToString(true /* readable */, driver()), element);
    }
    if (log_samples) {
      // TODO(jmaessen): This should really send samples to a separate file,
      // rather than the error log, but that requires solving some simple
      // concurrency problems that aren't currently worth it for this use case
      // alone.
      GoogleString sample_string =
          sample->ToString(false /* numeric */, driver());
      driver()->message_handler()->Message(
          kError, "%s: %s { %s }",
          driver()->url(),
          element->live() ? element->name_str().as_string().c_str()
                          : "(flushed element)",
          sample_string.c_str());
    }
  }
}

// The div corresponding to *sample will be unlabelled.  Bump stats and remove
// its id if it was PageSpeed-inserted.
void MobilizeLabelFilter::UnlabelledDiv(ElementSample* sample) {
  divs_unlabeled_->Add(1);
  if (!driver()->DebugMode() && driver()->IsRewritable(sample->element) &&
      StringPiece(sample->id).starts_with(AddIdsFilter::kClassPrefix)) {
    // Strip out id if it was inserted by PageSpeed.
    sample->element->DeleteAttribute(HtmlName::kId);
  }
}

void MobilizeLabelFilter::InjectLabelJavascript() {
  // Go through the nodes in DOM order and collect role transition points.
  GoogleString role_id_list_js[MobileRole::kInvalid];
  int n = samples_.size();
  bool any_roles_listed = false;
  for (int i = 1; i < n; ++i) {
    ElementSample* sample = samples_[i];
    MobileRole::Level role = sample->role;
    if (role != sample->parent->role) {
      if (!IsRoleValid(role)) {
        LOG(DFATAL) << "Invalid role " << role <<
            " below valid one " << sample->parent->role;
      } else {
        if (!sample->explicitly_labeled) {
          were_roles_added_ = true;
        }
        role_variables_[role]->Add(1);
        EscapeToJsStringLiteral(
            sample->id, false /* no quotes */, &role_id_list_js[role]);
        StrAppend(&role_id_list_js[role], "','");
        any_roles_listed = true;
      }
    } else {
      UnlabelledDiv(sample);
    }
  }
  if (!any_roles_listed) {
    // Don't inject any code if there's nothing to do.
    return;
  }
  // Now turn the resulting JS fragments into code.
  GoogleString js;
  for (int i = 0; i < MobileRole::kInvalid; ++i) {
    MobileRole::Level level = static_cast<MobileRole::Level>(i);
    if (!role_id_list_js[level].empty()) {
      StringPiece s(role_id_list_js[level]);
      // Remove trailing ",'"
      s.remove_suffix(2);
      // Create identifier and bind it. Example:
      // pageSpeedNavigationalIds=['id1','id2'];
      StrAppend(&js, "pagespeed",
                Capitalize(MobileRoleData::StringFromLevel(level)),
                "Ids=['", s, "];\n");
    }
  }
  HtmlElement* script = driver()->NewElement(NULL, HtmlName::kScript);
  InsertNodeAtBodyEnd(script);
  AddJsToElement(js, script);
}

void MobilizeLabelFilter::NonMobileUnlabel() {
  // Computed labeling is not actually wanted in DOM, though we may still have
  // needed to log the labeled elements.  Strip the added ids and don't inject
  // JS.
  int n = samples_.size();
  for (int i = 1; i < n; ++i) {
    ElementSample* sample = samples_[i];
    if (driver()->IsRewritable(sample->element) &&
        StringPiece(sample->id).starts_with(AddIdsFilter::kClassPrefix)) {
      // Strip out id inserted by pagespeed.
      sample->element->DeleteAttribute(HtmlName::kId);
    }
  }
}

}  // namespace net_instaweb
