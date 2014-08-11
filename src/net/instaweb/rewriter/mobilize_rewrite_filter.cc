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

// Author: stevensr@google.com (Ryan Stevens)

#include "net/instaweb/rewriter/public/mobilize_rewrite_filter.h"

#include <algorithm>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parse.h"

namespace net_instaweb {

extern const char* CSS_mobilize_css;

const char MobilizeRewriteFilter::kImportanceAttributeName[] = "importance";
const MobilizeRewriteFilter::Importance
MobilizeRewriteFilter::kImportances[] = {
  // This is the order that the HTML content will be rearranged.
  Importance(kKeeper, "keeper"),
  Importance(kHeader, "header"),
  Importance(kNavigational, "navigational"),
  Importance(kContent, "content"),
  Importance(kMarginal, "marginal")
};
const int MobilizeRewriteFilter::kImportancesSize =
    arraysize(MobilizeRewriteFilter::kImportances);

const char MobilizeRewriteFilter::kPagesMobilized[] =
    "mobilization_pages_rewritten";
const char MobilizeRewriteFilter::kKeeperBlocks[] =
    "mobilization_keeper_blocks_moved";
const char MobilizeRewriteFilter::kHeaderBlocks[] =
    "mobilization_header_blocks_moved";
const char MobilizeRewriteFilter::kNavigationalBlocks[] =
    "mobilization_navigational_blocks_moved";
const char MobilizeRewriteFilter::kContentBlocks[] =
    "mobilization_content_blocks_moved";
const char MobilizeRewriteFilter::kMarginalBlocks[] =
    "mobilization_marginal_blocks_moved";
const char MobilizeRewriteFilter::kDeletedElements[] =
    "mobilization_elements_deleted";

namespace {
const char kViewportContent[] = "width=device-width,user-scalable=no";
const HtmlName::Keyword kKeeperTags[] = {
  HtmlName::kArea, HtmlName::kMap, HtmlName::kScript, HtmlName::kStyle};
const HtmlName::Keyword kPreserveNavTags[] = {HtmlName::kA};
const HtmlName::Keyword kTableTags[] = {
  HtmlName::kCaption, HtmlName::kCol, HtmlName::kColgroup, HtmlName::kTable,
  HtmlName::kTbody, HtmlName::kTd, HtmlName::kTfoot, HtmlName::kTh,
  HtmlName::kThead, HtmlName::kTr};
const HtmlName::Keyword kTableTagsToBr[] = {HtmlName::kTable, HtmlName::kTr};

#ifndef NDEBUG
void CheckKeywordsSorted(const HtmlName::Keyword* list, int len) {
  for (int i = 1; i < len; ++i) {
    DCHECK(list[i - 1] < list[i]);
  }
}
#endif  // #ifndef NDEBUG
}  // namespace

MobilizeRewriteFilter::MobilizeRewriteFilter(RewriteDriver* rewrite_driver)
    : driver_(rewrite_driver),
      important_element_depth_(0),
      body_element_depth_(0),
      nav_element_depth_(0),
      reached_reorder_containers_(false),
      added_style_(false),
      added_containers_(false),
      style_css_(CSS_mobilize_css) {
  Statistics* stats = rewrite_driver->statistics();
  num_pages_mobilized_ = stats->GetVariable(kPagesMobilized);
  num_keeper_blocks_ = stats->GetVariable(kKeeperBlocks);
  num_header_blocks_ = stats->GetVariable(kHeaderBlocks);
  num_navigational_blocks_ = stats->GetVariable(kNavigationalBlocks);
  num_content_blocks_ = stats->GetVariable(kContentBlocks);
  num_marginal_blocks_ = stats->GetVariable(kMarginalBlocks);
  num_elements_deleted_ = stats->GetVariable(kDeletedElements);
#ifndef NDEBUG
  CheckKeywordsSorted(kKeeperTags, arraysize(kKeeperTags));
  CheckKeywordsSorted(kPreserveNavTags, arraysize(kPreserveNavTags));
  CheckKeywordsSorted(kTableTags, arraysize(kTableTags));
  CheckKeywordsSorted(kTableTagsToBr, arraysize(kTableTagsToBr));
#endif  // #ifndef NDEBUG
}

MobilizeRewriteFilter::~MobilizeRewriteFilter() {}

void MobilizeRewriteFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kPagesMobilized);
  statistics->AddVariable(kKeeperBlocks);
  statistics->AddVariable(kHeaderBlocks);
  statistics->AddVariable(kNavigationalBlocks);
  statistics->AddVariable(kContentBlocks);
  statistics->AddVariable(kMarginalBlocks);
  statistics->AddVariable(kDeletedElements);
}

void MobilizeRewriteFilter::StartDocument() {
  important_element_depth_ = 0;
  body_element_depth_ = 0;
  nav_element_depth_ = 0;
  reached_reorder_containers_ = false;
  added_style_ = false;
  added_containers_ = false;
}

void MobilizeRewriteFilter::EndDocument() {
  num_pages_mobilized_->Add(1);
}

void MobilizeRewriteFilter::StartElement(HtmlElement* element) {
  HtmlName::Keyword keyword = element->keyword();

  // Remove any existing viewport tags.
  if (keyword == HtmlName::kMeta) {
    HtmlElement::Attribute* name_attribute =
        element->FindAttribute(HtmlName::kName);
    if (name_attribute != NULL &&
        (StringPiece(name_attribute->escaped_value()) == "viewport")) {
      driver_->DeleteNode(element);
      num_elements_deleted_->Add(1);
      return;
    }
  }

  if (keyword == HtmlName::kBody) {
    ++body_element_depth_;
    AddReorderContainers(element);
  } else if (body_element_depth_ > 0) {
    HandleStartTagInBody(element);
  }
}

void MobilizeRewriteFilter::EndElement(HtmlElement* element) {
  HtmlName::Keyword keyword = element->keyword();
  if (keyword == HtmlName::kBody) {
    --body_element_depth_;
    if (body_element_depth_ == 0) {
      RemoveReorderContainers();
      reached_reorder_containers_ = false;
    }
  } else if (body_element_depth_ == 0 && keyword == HtmlName::kHead) {
    AddStyleAndViewport(element);
  } else if (body_element_depth_ > 0) {
    HandleEndTagInBody(element);
  }
}

void MobilizeRewriteFilter::Characters(HtmlCharactersNode* characters) {
  if (body_element_depth_ == 0 || reached_reorder_containers_) {
    return;
  }

  bool del = false;
  GoogleString debug_msg;
  if (!InImportantElement()) {
    del = true;
    debug_msg = "Deleted characters which were not in an element which"
        " was tagged as important: ";
  } else if (nav_element_depth_ > 0 && nav_keyword_stack_.empty()) {
    del = true;
    debug_msg = "Deleted characters inside a navigational section"
        " which were not considered to be relevant to navigation: ";
  }

  if (del) {
    if (driver_->DebugMode() && !OnlyWhitespace(characters->contents())) {
      GoogleString msg = debug_msg + characters->contents();
      driver_->InsertDebugComment(msg, characters);
    }
    driver_->DeleteNode(characters);
    num_elements_deleted_->Add(1);
  }
}

void MobilizeRewriteFilter::HandleStartTagInBody(HtmlElement* element) {
  HtmlName::Keyword keyword = element->keyword();
  if (reached_reorder_containers_) {
    // Stop rewriting once we've reached the containers at the end of the body.
  } else if (IsReorderContainer(element)) {
    reached_reorder_containers_ = true;
  } else if (CheckForKeyword(kTableTags, arraysize(kTableTags), keyword)) {
    // Remove any table tags.
    if (CheckForKeyword(kTableTagsToBr, arraysize(kTableTagsToBr), keyword)) {
      HtmlElement* added_br_element = driver_->NewElement(
          element->parent(), HtmlName::kBr);
      added_br_element->set_close_style(HtmlElement::IMPLICIT_CLOSE);
      driver_->InsertElementAfterElement(element, added_br_element);
    }
    if (driver_->DebugMode()) {
      GoogleString msg(StrCat("Deleted table tag: ", element->name_str()));
      driver_->InsertDebugComment(msg, element);
    }
    driver_->DeleteSavingChildren(element);
    num_elements_deleted_->Add(1);
  } else if (GetImportance(element) != kInvalid) {
    // Record that we are starting an element with an importance attribute.
    ++important_element_depth_;
    if (GetImportance(element) == kNavigational) {
      ++nav_element_depth_;
      if (nav_element_depth_ == 1) {
        nav_keyword_stack_.clear();
      }
    }
  } else if (nav_element_depth_ > 0) {
    // Remove all navigational content not inside a desired tag.
    if (CheckForKeyword(
            kPreserveNavTags, arraysize(kPreserveNavTags), keyword)) {
      nav_keyword_stack_.push_back(keyword);
    }
    if (nav_keyword_stack_.empty()) {
      if (driver_->DebugMode()) {
        GoogleString msg(
            StrCat("Deleted non-nav element in navigational section: ",
                   element->name_str()));
        driver_->InsertDebugComment(msg, element);
      }
      driver_->DeleteSavingChildren(element);
      num_elements_deleted_->Add(1);
    }
  } else if (!InImportantElement()) {
    if (driver_->DebugMode()) {
      GoogleString msg(
          StrCat("Deleted element which did not have an importance: ",
                 element->name_str()));
      driver_->InsertDebugComment(msg, element);
    }
    driver_->DeleteSavingChildren(element);
    num_elements_deleted_->Add(1);
  }
}

void MobilizeRewriteFilter::HandleEndTagInBody(HtmlElement* element) {
  if (reached_reorder_containers_) {
    // Stop rewriting once we've reached the containers at the end of the body.
  } else if (GetImportance(element) != kInvalid) {
    --important_element_depth_;
    // Record that we've left an element with an importance attribute. If we are
    // no longer in one, we can move all the content of this element into its
    // appropriate container for reordering.
    HtmlElement* importance_container =
        ImportanceToContainer(GetImportance(element));
    DCHECK(importance_container != NULL)
        << "Reorder containers were never initialized.";
    if (!InImportantElement()) {
      // Move element and its children into its container.
      driver_->MoveCurrentInto(importance_container);
      LogMovedBlock(GetImportance(element));
    } else {
      // TODO(stevensr): Logging this may be too verbose, as having 'keepers'
      // inside <div>s is pretty common.
      driver_->InfoHere("We have nested elements with an importance"
                        " attribute. Assigning all children to the"
                        " importance level of the their parent.");
    }
    if (GetImportance(element) == kNavigational) {
      --nav_element_depth_;
    }
  } else if (nav_element_depth_ > 0) {
    HtmlName::Keyword keyword = element->keyword();
    if (!nav_keyword_stack_.empty() && (keyword == nav_keyword_stack_.back())) {
      nav_keyword_stack_.pop_back();
    }
  }
}

void MobilizeRewriteFilter::AddStyleAndViewport(HtmlElement* element) {
  if (!added_style_) {
    // <style>...</style>
    HtmlElement* added_style_element = driver_->NewElement(
        element, HtmlName::kStyle);
    driver_->AppendChild(element, added_style_element);
    HtmlCharactersNode* add_style_text = driver_->NewCharactersNode(
        added_style_element, style_css_);
    driver_->AppendChild(added_style_element, add_style_text);
    // <meta name="viewport"... />
    HtmlElement* added_viewport_element = driver_->NewElement(
        element, HtmlName::kMeta);
    added_viewport_element->set_close_style(HtmlElement::BRIEF_CLOSE);
    added_viewport_element->AddAttribute(
        driver_->MakeName(HtmlName::kName), "viewport",
        HtmlElement::SINGLE_QUOTE);
    added_viewport_element->AddAttribute(
        driver_->MakeName(HtmlName::kContent), kViewportContent,
        HtmlElement::SINGLE_QUOTE);
    driver_->AppendChild(element, added_viewport_element);
    added_style_ = true;
  }
}

// Adds containers at the end of the element (preferrably the body), which we
// use to reorganize elements in the DOM by moving elements into the correct
// container. Later, we will delete these elements once the HTML has been
// restructured.
void MobilizeRewriteFilter::AddReorderContainers(HtmlElement* element) {
  if (!added_containers_) {
    importance_containers_.clear();
    for (int i = 0, n = arraysize(kImportances); i < n; ++i) {
      HtmlElement* added_container = driver_->NewElement(
          element, HtmlName::kDiv);
      added_container->AddAttribute(
          driver_->MakeName(HtmlName::kName), kImportances[i].value,
          HtmlElement::SINGLE_QUOTE);
      driver_->AppendChild(element, added_container);
      importance_containers_.push_back(added_container);
    }
    added_containers_ = true;
  }
}

void MobilizeRewriteFilter::RemoveReorderContainers() {
  if (added_containers_) {
    for (int i = 0, n = importance_containers_.size(); i < n; ++i) {
      if (driver_->DebugMode()) {
        GoogleString msg(StrCat("End section: ", kImportances[i].value));
        driver_->InsertDebugComment(msg, importance_containers_[i]);
      }
      driver_->DeleteSavingChildren(importance_containers_[i]);
    }
    importance_containers_.clear();
    added_containers_ = false;
  }
}

bool MobilizeRewriteFilter::IsReorderContainer(HtmlElement* element) {
  for (int i = 0, n = importance_containers_.size(); i < n; ++i) {
    if (element == importance_containers_[i]) {
      return true;
    }
  }
  return false;
}

// Maps each importance type to the container we created for it, or NULL for
// unrecognized importance types.
HtmlElement* MobilizeRewriteFilter::ImportanceToContainer(
    ImportanceLevel importance) {
  for (int i = 0, n = arraysize(kImportances); i < n; ++i) {
    if (importance == kImportances[i].level) {
      return importance_containers_[i];
    }
  }
  return NULL;
}

MobilizeRewriteFilter::ImportanceLevel
MobilizeRewriteFilter::StringToImportance(const StringPiece& importance) {
  for (int i = 0, n = arraysize(kImportances); i < n; ++i) {
    if (importance == kImportances[i].value) {
      return kImportances[i].level;
    }
  }
  return kInvalid;
}

MobilizeRewriteFilter::ImportanceLevel MobilizeRewriteFilter::GetImportance(
    HtmlElement* element) {
  HtmlElement::Attribute* importance_attribute =
      element->FindAttribute(kImportanceAttributeName);
  if (importance_attribute) {
    return StringToImportance(importance_attribute->escaped_value());
  } else {
    if (CheckForKeyword(kKeeperTags, arraysize(kKeeperTags),
                        element->keyword())) {
      return kKeeper;
    }
    return kInvalid;
  }
}

bool MobilizeRewriteFilter::CheckForKeyword(
    const HtmlName::Keyword* sorted_list, int len, HtmlName::Keyword keyword) {
  return std::binary_search(sorted_list, sorted_list+len, keyword);
}

void MobilizeRewriteFilter::LogMovedBlock(ImportanceLevel level) {
  switch (level) {
    case kKeeper:
      num_keeper_blocks_->Add(1);
      break;
    case kHeader:
      num_header_blocks_->Add(1);
      break;
    case kNavigational:
      num_navigational_blocks_->Add(1);
      break;
    case kContent:
      num_content_blocks_->Add(1);
      break;
    case kMarginal:
      num_marginal_blocks_->Add(1);
      break;
    case kInvalid:
      // Should not happen.
      LOG(DFATAL) << "Attepted to move kInvalid element";
      break;
  }
}

}  // namespace net_instaweb
