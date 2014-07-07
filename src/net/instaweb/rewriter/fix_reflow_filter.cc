/*
 * Copyright 2013 Google Inc.
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

// Author: sriharis@google.com (Srihari Sukumaran)

#include "net/instaweb/rewriter/public/fix_reflow_filter.h"

#include <map>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/rewriter/public/js_defer_disabled_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kReflowValueSeparators[] = ",:";
const char kReflowClassAttribute[] = "data-pagespeed-fix-reflow";

}  // namespace

namespace net_instaweb {

const char FixReflowFilter::kElementRenderedHeightPropertyName[] =
    "element_rendered_height";

FixReflowFilter::FixReflowFilter(RewriteDriver* driver)
    : rewrite_driver_(driver) {
}

FixReflowFilter::~FixReflowFilter() {
}

void FixReflowFilter::DetermineEnabled(GoogleString* disabled_reason) {
  set_is_enabled(JsDeferDisabledFilter::ShouldApply(rewrite_driver_) &&
                 // Can we also share the following conditions with
                 // JsDeferDisabledFilter.
                 !rewrite_driver_->flushing_cached_html() &&
                 !rewrite_driver_->flushed_cached_html());
  if (!is_enabled()) {
    rewrite_driver_->log_record()->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kFixReflows),
        RewriterHtmlApplication::DISABLED);
  }
}

void FixReflowFilter::StartDocument() {
  bool pcache_miss = true;
  PropertyPage* page = rewrite_driver_->property_page();
  const PropertyCache::Cohort* cohort =
      rewrite_driver_->server_context()->fix_reflow_cohort();
  if (page != NULL && cohort != NULL) {
    PropertyValue* property_value = page->GetProperty(
        cohort, kElementRenderedHeightPropertyName);
    VLOG(1) << "Property value: " << property_value << " has value? "
            << property_value->has_value();
    const int64 cache_ttl_ms = rewrite_driver_->options()->
        finder_properties_cache_expiration_time_ms();
    PropertyCache* property_cache =
        rewrite_driver_->server_context()->page_property_cache();
    if (property_value != NULL &&
        property_value->has_value() &&
        !property_cache->IsExpired(property_value, cache_ttl_ms)) {
      pcache_miss = false;
      VLOG(1) << "FixReflowFilter.  Valid value in pcache.";
      // Parse property_value->value() into "id:height" and keep these locally.
      StringPieceVector element_height_vector;
      SplitStringPieceToVector(
          property_value->value(), kReflowValueSeparators,
          &element_height_vector, true);
      for (int i = 0, n = element_height_vector.size(); i < n - 1; i += 2) {
        element_height_map_.insert(make_pair(
            element_height_vector[i].as_string(),
            element_height_vector[i+1].as_string()));
      }
    }
  }
  if (pcache_miss) {
    rewrite_driver_->log_record()->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kFixReflows),
        RewriterHtmlApplication::PROPERTY_CACHE_MISS);
  } else {
    rewrite_driver_->log_record()->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kFixReflows),
        RewriterHtmlApplication::ACTIVE);
  }
}

void FixReflowFilter::StartElement(HtmlElement* element) {
  // See if element has attribute id matching any one from "id:height" pairs.
  // If yes insert a style attribute with height.
  if (element->keyword() == HtmlName::kDiv) {
    const char* id = element->AttributeValue(HtmlName::kId);
    if (id != NULL) {
      ElementHeightMap::const_iterator i = element_height_map_.find(id);
      if (i != element_height_map_.end()) {
        rewrite_driver_->log_record()->SetRewriterLoggingStatus(
            RewriteOptions::FilterId(RewriteOptions::kFixReflows),
            RewriterApplication::APPLIED_OK);
        VLOG(1) << "div " << id << " has height " << i->second;
        element->AddAttribute(rewrite_driver_->MakeName(HtmlName::kStyle),
                              StrCat("min-height:", i->second),
                              HtmlElement::DOUBLE_QUOTE);
        element->AddAttribute(rewrite_driver_->MakeName(kReflowClassAttribute),
                              "", HtmlElement::DOUBLE_QUOTE);
        // TODO(sriharis):  Should we add js to delete the added style
        // attributes?  Maybe a function that is called from js_defer.js's
        // AfterDefer hook.
      }
    }
  }
}

}  // namespace net_instaweb
