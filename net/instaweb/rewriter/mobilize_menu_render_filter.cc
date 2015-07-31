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

// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/rewriter/public/mobilize_menu_render_filter.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/add_ids_filter.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/mobilize_label_filter.h"
#include "net/instaweb/rewriter/public/mobilize_menu_filter.h"
#include "net/instaweb/rewriter/public/mobilize_rewrite_filter.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/render_blocking_html_computation.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/opt/http/property_cache.h"

namespace net_instaweb {

const char MobilizeMenuRenderFilter::kMenusAdded[] =
    "mobilization_menus_added";

const char MobilizeMenuRenderFilter::kMobilizeMenuPropertyName[] =
    "mobilize_menu";

void MobilizeMenuRenderFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kMenusAdded);
}

class MobilizeMenuRenderFilter::MenuComputation
    : public RenderBlockingHtmlComputation {
 public:
  MenuComputation(MobilizeMenuRenderFilter* parent_filter,
                  RewriteDriver* parent_driver)
      : RenderBlockingHtmlComputation(parent_driver),
        parent_filter_(parent_filter) {}

 protected:
  virtual void SetupFilters(RewriteDriver* child_driver) {
    child_driver->AppendOwnedPreRenderFilter(new AddIdsFilter(child_driver));
    MobilizeLabelFilter* label_filter =
        new MobilizeLabelFilter(true /* is_menu_subfetch */, child_driver);
    child_driver->AppendOwnedPreRenderFilter(label_filter);
    menu_filter_ = new MobilizeMenuFilter(child_driver, label_filter);
    child_driver->AppendOwnedPreRenderFilter(menu_filter_);
  }

  virtual void Done(bool success) {
    if (success) {
      // Note that this will happen-before RenderDone.
      parent_filter_->menu_.reset(menu_filter_->release_menu());
      parent_filter_->menu_computed_ = true;
    }
  }

 private:
  MobilizeMenuRenderFilter* parent_filter_;
  MobilizeMenuFilter* menu_filter_;  // owned by child rewrite driver.
};

MobilizeMenuRenderFilter::MobilizeMenuRenderFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      use_readable_menus_(driver->DebugMode()),
      saw_end_document_(false),
      menu_computed_(false) {
  Statistics* stats = driver->statistics();
  num_menus_added_ = stats->GetVariable(kMenusAdded);
}

void MobilizeMenuRenderFilter::StartDocumentImpl() {
  saw_end_document_ = false;
  menu_computed_ = false;

  // This current reads per-URL, and doesn't do any aggregation.
  const PropertyCache::Cohort* cohort =
      driver()->server_context()->dom_cohort();
  if (cohort != NULL) {
    PropertyCacheDecodeResult result;
    menu_.reset(DecodeFromPropertyCache<MobilizeMenu>(
        driver(), cohort, kMobilizeMenuPropertyName,
        driver()->options()->finder_properties_cache_expiration_time_ms(),
        &result));
    if (result != kPropertyCacheDecodeOk) {
      menu_.reset(NULL);
    }
  }
  if (menu_.get() == NULL) {
    // We don't have a menu, so compute it.
    scoped_ptr<MenuComputation> compute_menu(
        new MenuComputation(this, driver()));
    compute_menu.release()->Compute(driver()->url());
  }
}

void MobilizeMenuRenderFilter::EndDocument() {
  saw_end_document_ = true;
}

void MobilizeMenuRenderFilter::RenderDone() {
  // Note that one can actually do this on the first RenderDone, not the last
  // one, but that make it harder to reason about where the output is getting
  // inserted.
  if (!saw_end_document_) {
    return;
  }

  // Note that despite the blocking background computation, the menu may still
  // be NULL, as it's possible that the fetch for the page has failed.
  if (menu_.get() != NULL) {
    ConstructMenu();

    if (menu_computed_) {
      // Write to in-memory property cache. Will be committed later because we
      // set driver()->set_write_property_cache_dom_cohor();
      const PropertyCache::Cohort* cohort =
          driver()->server_context()->dom_cohort();
      if (cohort != NULL) {
        UpdateInPropertyCache(*menu_, driver(), cohort,
                              kMobilizeMenuPropertyName,
                              false /* don't commit immediately */);
      }
    }
  } else {
    InsertNodeAtBodyEnd(driver()->NewCommentNode(NULL, "No computed menu"));
  }
}

void MobilizeMenuRenderFilter::DetermineEnabled(GoogleString* disabled_reason) {
  bool enabled = MobilizeRewriteFilter::IsApplicableFor(driver());
  set_is_enabled(enabled);
  if (enabled) {
    driver()->set_write_property_cache_dom_cohort(true);
  }
}

// Actually construct the menu as nested <ul> and <li> elements at the end of
// the DOM.
void MobilizeMenuRenderFilter::ConstructMenu() {
  if (menu_ != NULL && menu_->entries_size() > 0) {
    DCHECK(MobilizeMenuFilter::IsMenuOk(*menu_));
    HtmlElement* nav = driver()->NewElement(NULL, HtmlName::kNav);
    driver()->AddAttribute(nav, HtmlName::kId, "psmob-nav-panel");
    InsertNodeAtBodyEnd(nav);
    HtmlElement* ul = driver()->NewElement(nav, HtmlName::kUl);
    driver()->AddAttribute(ul, HtmlName::kClass, "psmob-open");
    driver()->AppendChild(nav, ul);
    ConstructMenuWithin(1, "psmob-nav-panel", *menu_, ul);
    num_menus_added_->Add(1);
  } else {
    driver()->message_handler()->Message(
        kWarning, "No navigation found for %s", driver()->url());
    if (use_readable_menus_) {
      HtmlNode* comment_node =
          driver()->NewCommentNode(NULL, "No navigation?!");
      InsertNodeAtBodyEnd(comment_node);
    }
  }
}

// Construct a single level of menu structure and its submenus within the DOM
// element ul.  Labels each <li> element with an id based on parent_id.
void MobilizeMenuRenderFilter::ConstructMenuWithin(
    int level, StringPiece parent_id, const MobilizeMenu& menu,
    HtmlElement* ul) {
  int n = menu.entries_size();
  for (int i = 0; i < n; ++i) {
    const MobilizeMenuItem& item = menu.entries(i);
    if (use_readable_menus_) {
      // Make the debug output readable by adding a newline and indent.
      GoogleString indent = "\n";
      indent.append(2 * level, ' ');
      HtmlCharactersNode* indent_node =
          driver()->NewCharactersNode(ul, indent);
      driver()->AppendChild(ul, indent_node);
    }
    HtmlElement* li = driver()->NewElement(ul, HtmlName::kLi);
    driver()->AppendChild(ul, li);
    GoogleString id = StrCat(parent_id, "-", IntegerToString(i));
    driver()->AddAttribute(li, HtmlName::kId, id);
    if (item.has_submenu()) {
      // TODO(jmaessen): Add arrow icon?  It's currently being added by JS,
      // which can account for the theme data.  This also means we don't
      // duplicate the data: url in the html.
      HtmlElement* title_div = driver()->NewElement(li, HtmlName::kDiv);
      driver()->AppendChild(li, title_div);
      // Add an A tag so that the mouse pointer on desktop will indicate that
      // the submenus can be clicked on.
      HtmlElement* title_a = driver()->NewElement(title_div, HtmlName::kA);
      driver()->AddAttribute(title_a, HtmlName::kHref, "#");
      driver()->AppendChild(title_div, title_a);
      HtmlCharactersNode* submenu_title =
          driver()->NewCharactersNode(title_a, item.name());
      driver()->AppendChild(title_a, submenu_title);
      HtmlElement* ul = driver()->NewElement(li, HtmlName::kUl);
      driver()->AppendChild(li, ul);
      ConstructMenuWithin(level + 1, id, item.submenu(), ul);
    } else {
      HtmlElement* a = driver()->NewElement(li, HtmlName::kA);
      driver()->AddAttribute(a, HtmlName::kHref, item.url());
      driver()->AppendChild(li, a);
      HtmlCharactersNode* item_name =
          driver()->NewCharactersNode(a, item.name());
      driver()->AppendChild(a, item_name);
    }
  }
}

}  // namespace net_instaweb
