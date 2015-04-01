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

#include "net/instaweb/rewriter/public/mobilize_menu_filter.h"

#include <algorithm>

#include "base/logging.h"
#include "net/instaweb/rewriter/mobilize_menu.pb.h"
#include "net/instaweb/rewriter/public/mobilize_rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"

namespace net_instaweb {

// TODO(jmaessen): Store into pcache if we're not in a nested context.
MobilizeMenuFilter::MobilizeMenuFilter(RewriteDriver* rewrite_driver)
    : MobilizeFilterBase(rewrite_driver),
      outer_nav_element_(NULL),
      menu_item_trailing_whitespace_(false),
      cleanup_menu_(true) {}

MobilizeMenuFilter::~MobilizeMenuFilter() {
  DCHECK(outer_nav_element_ == NULL);
  DCHECK(menu_item_text_.empty());
}

void MobilizeMenuFilter::InitStats(Statistics* statistics) {
  // TODO(jmaessen): Stats?
}

void MobilizeMenuFilter::DetermineEnabled(GoogleString* disabled_reason) {
  if (!MobilizeRewriteFilter::IsApplicableFor(driver())) {
    // Log redundantly with the rewrite filter in case we're
    // currently in an iframe request where no rewriting happens.
    disabled_reason->assign("Not a mobile User Agent.");
    set_is_enabled(false);
  }
}

void MobilizeMenuFilter::StartDocumentImpl() {
  menu_.reset(new MobilizeMenu);
}

void MobilizeMenuFilter::EndDocumentImpl() {
  if (cleanup_menu_) {
    CleanupMenu(menu_.get());
  }
  DCHECK(outer_nav_element_ == NULL);
  DCHECK(menu_item_text_.empty());
  DCHECK(menu_stack_.empty());
  outer_nav_element_ = NULL;
  menu_item_text_.clear();
  menu_item_trailing_whitespace_ = false;
  menu_stack_.clear();
}

void MobilizeMenuFilter::StartNonSkipElement(
    MobileRole::Level role_attribute, HtmlElement* element) {
  if (outer_nav_element_ == NULL) {
    if (role_attribute != MobileRole::kNavigational) {
      return;
    }
    outer_nav_element_ = element;
    StartTopMenu();
  }
  switch (element->keyword()) {
    case HtmlName::kUl:
      StartDeepMenu();
      break;
    case HtmlName::kLi:
      StartMenuItem(NULL);
      break;
    case HtmlName::kA: {
      StartMenuItem(element->EscapedAttributeValue(HtmlName::kHref));
      break;
    }
    default:
      break;
  }
}

void MobilizeMenuFilter::EndNonSkipElement(HtmlElement* element) {
  if (outer_nav_element_ == NULL) {
    return;
  }
  switch (element->keyword()) {
    case HtmlName::kLi:
    case HtmlName::kA: {
      EndMenuItem();
      break;
    }
    case HtmlName::kUl:
      EndDeepMenu();
      break;
    default:
      break;
  }
  if (element == outer_nav_element_) {
    outer_nav_element_ = NULL;
    EndTopMenu();
  }
}

void MobilizeMenuFilter::Characters(HtmlCharactersNode* characters) {
  if (outer_nav_element_ == NULL) {
    return;
  }
  StringPiece contents(characters->contents());
  // Insert a space before trimmed contents if:
  //   1) There is already menu_item_text_.
  //   2) That text ended with whitespace or this text starts with whitespace.
  //   3) If this is only whitespace, just set the flag and move on.
  if (TrimLeadingWhitespace(&contents) && !menu_item_text_.empty()) {
    menu_item_trailing_whitespace_ = true;
  }
  if (!contents.empty()) {
    StringPiece lead(menu_item_trailing_whitespace_ ? " " : "");
    menu_item_trailing_whitespace_ = TrimTrailingWhitespace(&contents);
    StrAppend(&menu_item_text_, lead, contents);
  }
}

void MobilizeMenuFilter::StartTopMenu() {
  DCHECK(menu_item_text_.empty());
  DCHECK(!menu_item_trailing_whitespace_);
  DCHECK(menu_stack_.empty());
  menu_stack_.push_back(menu_.get());
}

void MobilizeMenuFilter::StartDeepMenu() {
  MobilizeMenuItem* entry = EnsureMenuItem();
  if (!menu_item_text_.empty()) {
    entry->set_name(menu_item_text_);
  }
  menu_stack_.push_back(entry->mutable_submenu());
  menu_item_text_.clear();
  menu_item_trailing_whitespace_ = false;
}

// Clear and discard the collected text in menu_item_text_, complaining if there
// actually was any.
void MobilizeMenuFilter::ClearMenuText() {
  if (!menu_item_text_.empty()) {
    driver()->InfoHere("Discarding unrooted nav text: %s",
                       menu_item_text_.c_str());
  }
  menu_item_text_.clear();
  menu_item_trailing_whitespace_ = false;
}

// Don't call this except from End[Top,Deep]Menu
void MobilizeMenuFilter::EndMenuCommon() {
  CHECK(!menu_stack_.empty());
  menu_stack_.pop_back();
  ClearMenuText();
}

void MobilizeMenuFilter::EndTopMenu() {
  EndMenuCommon();
  CHECK(menu_stack_.empty());
}

void MobilizeMenuFilter::EndDeepMenu() {
  EndMenuCommon();
  CHECK(!menu_stack_.empty());
}

MobilizeMenuItem* MobilizeMenuFilter::EnsureMenuItem() {
  CHECK(!menu_stack_.empty());
  MobilizeMenu* current_menu = menu_stack_.back();
  int sz = current_menu->entries_size();
  MobilizeMenuItem* entry;
  if (sz == 0) {
    entry = current_menu->add_entries();
  } else {
    entry = current_menu->mutable_entries(sz - 1);
    if (entry->has_url() || entry->has_submenu() || entry->has_name()) {
      entry = current_menu->add_entries();
    }
  }
  return entry;
}

void MobilizeMenuFilter::StartMenuItem(const char* href_or_null) {
  ClearMenuText();
  MobilizeMenuItem* entry = EnsureMenuItem();
  if (href_or_null != NULL && *href_or_null != '\0') {
    entry->set_url(href_or_null);
  }
}

void MobilizeMenuFilter::EndMenuItem() {
  CHECK(!menu_stack_.empty());
  MobilizeMenu* current_menu = menu_stack_.back();
  CHECK_LT(0, current_menu->entries_size());
  MobilizeMenuItem* entry =
      current_menu->mutable_entries(current_menu->entries_size() - 1);
  if (menu_item_text_.empty()) {
    // Do nothing.  This happens often when we have <li><a> </a></li>.
  } else if (entry->has_name()) {
    driver()->InfoHere("Menu item %s with trailing text %s",
                       entry->name().c_str(), menu_item_text_.c_str());
  } else {
    entry->set_name(menu_item_text_);
  }
  menu_item_text_.clear();
  menu_item_trailing_whitespace_ = false;
}

namespace {

// Choose the best level for a url occurring at menu levels a and b.
//   * 0 is used for absent values.  Choose the other in that case.
//   * Level 2 is preferred (one level nested).
//   * Otherwise prefer the minimum level.
// Only value a can be absent.
int BestLevel(int a, int b) {
  if (a == 0) {
    return b;
  } else if (a == 2 || b == 2) {
    return 2;
  } else {
    return std::min(a, b);
  }
}

}  // namespace

// Clean up the constructed menu by removing duplicate elements, empty submenus,
// etc.  We try to keep a url as close to level 2 as possible (inside a single
// nested menu).  If it's deeper, we favor a shallower occurrence.  If it's
// shallower, we favor the nested one.
void MobilizeMenuFilter::CleanupMenu(MobilizeMenu* menu) {
  if (menu->entries_size() == 0) {
    return;
  }
  UrlLevelMap url_level;
  MobilizeMenu swept_menu;
  SweepMenu(*menu, &swept_menu);
  DCHECK(IsMenuOk(swept_menu));
  CollectMenuUrls(1, swept_menu, &url_level);
  ClearDuplicateEntries(1, &swept_menu, &url_level);
  menu->Clear();
  SweepMenu(swept_menu, menu);
  DCHECK(IsMenuOk(*menu));
}

// Sweep valid entries from menu into new_menu, throwing out garbage and
// flattening useless nesting.
void MobilizeMenuFilter::SweepNestedMenu(
    const MobilizeMenu& menu, MobilizeMenu* new_menu) {
  int n = menu.entries_size();
  for (int i = 0; i < n; ++i) {
    const MobilizeMenuItem& item = menu.entries(i);
    if (item.has_name()) {
      if (item.has_submenu()) {
        MobilizeMenu new_submenu;
        SweepNestedMenu(item.submenu(), &new_submenu);
        if (new_submenu.entries_size() > 0) {
          if (item.has_url()) {
            LOG(INFO) << "Dropping link " << item.url() << " on submenu "
                      << item.name();
          }
          MobilizeMenuItem* new_item = new_menu->add_entries();
          if (new_submenu.entries_size() == 1) {
            const MobilizeMenuItem& single_entry = new_submenu.entries(0);
            if (single_entry.has_name()) {
              LOG(INFO) << "Flattening away 1-entry submenu "
                        << single_entry.name();
            }
            // Pull the data out of the single submenu entry.  Warning:
            // assignment here is O(n) if the nested entry is itself a submenu.
            // We could work around this if it came up often and we expected
            // large menus, but we don't expect either.
            *new_item = single_entry;
          } else {
            new_item->set_name(item.name());
            new_item->mutable_submenu()->mutable_entries()->Swap(
                new_submenu.mutable_entries());
          }
          continue;
        }
        DCHECK_EQ(0, new_submenu.entries_size());
        // Fall through in case empty submenu had url attached.
      }
      if (!item.has_url()) {
        LOG(INFO) << "Dropping item " << item.name() << " without link.";
        continue;
      }
      MobilizeMenuItem* new_item = new_menu->add_entries();
      new_item->set_url(item.url());
      new_item->set_name(item.name());
    } else {
      if (item.has_url()) {
        LOG(INFO) << "Dropping link " << item.url()
                  << " without name (image only?)";
      }
      if (item.has_submenu()) {
        // submenu without title.  Flatten into new_menu.
        SweepNestedMenu(item.submenu(), new_menu);
      }
    }
  }
}

// Sweep top-level menu, flattening a singleton outer submenu.
void MobilizeMenuFilter::SweepMenu(
    const MobilizeMenu& menu, MobilizeMenu* new_menu) {
  SweepNestedMenu(menu, new_menu);
  if (new_menu->entries_size() == 1 && new_menu->entries(0).has_submenu()) {
    // We move the nested menu into temp, swapping empty entries into the place
    // where the nested menu used to be.
    MobilizeMenu temp;
    temp.mutable_entries()->Swap(
        new_menu->mutable_entries(0)->mutable_submenu()->mutable_entries());
    // Now we replace new_menu's entries with the nested entries.
    new_menu->mutable_entries()->Swap(temp.mutable_entries());
    // At this point temp contains the original top-level new_menu entry, and
    // will be deallocated on block exit.
  }
}

// Find canonical occurrences of menu urls.
void MobilizeMenuFilter::CollectMenuUrls(
    int level, const MobilizeMenu& menu, UrlLevelMap* url_level) {
  int n = menu.entries_size();
  for (int i = 0; i < n; ++i) {
    const MobilizeMenuItem& item = menu.entries(i);
    DCHECK(item.has_name());
    if (item.has_submenu()) {
      DCHECK(!item.has_url());
      CollectMenuUrls(level + 1, item.submenu(), url_level);
    }
    if (item.has_url()) {
      DCHECK(!item.has_submenu());
      int& preferred_level = (*url_level)[item.url()];
      preferred_level = BestLevel(preferred_level, level);
    }
  }
}

// Take duplicate url entries and clear them from *menu, based on the
// data previously collected in *url_level by CollectNestedMenu.
void MobilizeMenuFilter::ClearDuplicateEntries(
    int level, MobilizeMenu* menu, UrlLevelMap* url_level) {
  int n = menu->entries_size();
  for (int i = 0; i < n; ++i) {
    MobilizeMenuItem* item = menu->mutable_entries(i);
    if (item->has_submenu()) {
      ClearDuplicateEntries(level + 1, item->mutable_submenu(), url_level);
    } else if (item->has_url()) {
      int& preferred_level = (*url_level)[item->url()];
      if (level == preferred_level) {
        // First occurrence at preferred_level.  Clear preferred_level so
        // subsequent occurrences at the same level have their menu entries
        // cleared.
        preferred_level = 0;
      } else {
        // Duplicated.  Clear it.
        LOG(INFO) << "Dropping duplicate entry " << item->name()
                  << " for " << item->url() << " at level " << level;
        item->clear_url();
        item->clear_name();
      }
    }
  }
}

// Rules for a well-formed menu:
// * Every entry has a name.
// * Every entry has either a submenu or a url, not both.
// * Every submenu has at least two entries.
// These conditions are enforced by SweepNestedMenu.
// For debug purposes.  Usage: DCHECK(IsMenuOK(menu))
bool MobilizeMenuFilter::IsMenuOk(const MobilizeMenu& menu) {
  bool ok = true;
  int n = menu.entries_size();
  for (int i = 0; i < n; ++i) {
    const MobilizeMenuItem& item = menu.entries(i);
    if (!item.has_name()) {
      ok = false;
      LOG(ERROR) << "Menu item without name.";
    }
    if (item.has_submenu()) {
      const MobilizeMenu& submenu = item.submenu();
      if (item.has_url()) {
        ok = false;
        LOG(ERROR) << "Submenu " << item.name() << " with url " << item.url();
      }
      if (submenu.entries_size() <= 1) {
        ok = false;
        LOG(ERROR) << "Submenu " << item.name() << " has <= 1 entry.";
      }
      ok = IsMenuOk(submenu) && ok;
    } else if (!item.has_url()) {
      ok = false;
      LOG(ERROR) << "Item " << item.name() << " without link.";
    }
  }
  return ok;
}

}  // namespace net_instaweb
