/*
 * Copyright 2011 Google Inc.
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

#include "net/instaweb/rewriter/public/css_resource_slot.h"

#include <cstddef>
#include <set>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/url_left_trim_filter.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/value.h"

namespace net_instaweb {

CssResourceSlot::~CssResourceSlot() {
}

void CssResourceSlot::Render() {
  if (disable_rendering()) {
    return;  // nothing done here.
  } else {
    GoogleString rel_url = resource()->url();
#ifndef NDEBUG
    // Check that it's an absolute URL.
    GoogleUrl rel_gurl(rel_url);
    DCHECK(rel_gurl.is_valid());
#endif  // NDEBUG

    // Trim URL.
    StringPiece url(rel_url);
    GoogleString trimmed_url;
    if (trim_base_.get() != NULL) {
      if (UrlLeftTrimFilter::Trim(
              *trim_base_, url, &trimmed_url,
              resource()->resource_manager()->message_handler())) {
        url = trimmed_url;
      }
    }
    DirectSetUrl(url);
  }
}

void CssResourceSlot::Finished() {
  // We always want to Render CssResourceSlots (even if the sub-resource was
  // not optimizable), because the URLs need to be absolutified.
  Render();
}

GoogleString CssResourceSlot::LocationString() {
  // TODO(morlovich): Improve quality of this diagnostic.
  return "Inside CSS";
}

void CssResourceSlot::EnableTrim(const GoogleUrl& base_url) {
  trim_base_.reset(new GoogleUrl);
  trim_base_->Reset(base_url);
}

void CssResourceSlot::DirectSetUrl(const StringPiece& url) {
  delete (*values_)[value_index_];
  (*values_)[value_index_] =
      new Css::Value(Css::Value::URI,
                     UTF8ToUnicodeText(url.data(), url.size()));
}

bool CssResourceSlotFactory::SlotComparator::operator()(
    const CssResourceSlotPtr& p, const CssResourceSlotPtr& q) const {
  return (std::make_pair(p->values(), p->value_index()) <
              std::make_pair(q->values(), q->value_index()));
}

CssResourceSlotFactory::~CssResourceSlotFactory() {
}

CssResourceSlotPtr CssResourceSlotFactory::GetSlot(
    const ResourcePtr& resource, Css::Values* values, size_t value_index) {
  CssResourceSlot* slot_obj =
      new CssResourceSlot(resource, values, value_index);
  CssResourceSlotPtr slot(slot_obj);
  return UniquifySlot(slot);
}

CssResourceSlotPtr CssResourceSlotFactory::UniquifySlot(
    CssResourceSlotPtr slot) {
  std::pair<SlotSet::iterator, bool> iter_found = slots_.insert(slot);
  if (!iter_found.second) {
    // The slot was already in the set.  Release the one we just
    // allocated and use the one already in.
    SlotSet::iterator iter = iter_found.first;
    slot.reset(*iter);
  }
  return slot;
}

}  // namespace net_instaweb
