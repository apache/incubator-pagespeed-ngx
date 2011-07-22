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

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/url_left_trim_filter.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/value.h"

namespace net_instaweb {

CssResourceSlot::~CssResourceSlot() {
}

void CssResourceSlot::Render() {
  if (disable_rendering()) {
    return;  // nothing done here.
  } else {
    GoogleString url = resource()->url();
    if (trim_base_.get() != NULL) {
      GoogleString trimmed_url;
      if (UrlLeftTrimFilter::Trim(
          *trim_base_, url, &trimmed_url,
          resource()->resource_manager()->message_handler())) {
        url = trimmed_url;
      }
    }

    delete (*values_)[value_index_];
    (*values_)[value_index_] =
        new Css::Value(Css::Value::URI, UTF8ToUnicodeText(url));
  }
}

void CssResourceSlot::EnableTrim(const GoogleUrl& base_url) {
  trim_base_.reset(new GoogleUrl);
  trim_base_->Reset(base_url);
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
