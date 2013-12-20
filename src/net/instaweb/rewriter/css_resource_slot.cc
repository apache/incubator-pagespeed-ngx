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
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/url_left_trim_filter.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/http/google_url.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/value.h"

namespace net_instaweb {

CssResourceSlot::CssResourceSlot(const ResourcePtr& resource,
                                 const GoogleUrl& trim_url,
                                 const RewriteOptions* options,
                                 Css::Values* values,
                                 size_t value_index)
    : ResourceSlot(resource),
      values_(values),
      value_index_(value_index),
      url_relativity_(GoogleUrl::FindRelativity(UnicodeTextToUTF8(
          values->at(value_index)->GetStringValue()))),
      options_(options) {
  trim_url_.Reset(trim_url);
}

CssResourceSlot::~CssResourceSlot() {
}

void CssResourceSlot::Render() {
  if (disable_rendering()) {
    return;  // nothing done here.
  } else {
    GoogleString url = resource()->url();

#ifndef NDEBUG
    // Check that it's an absolute URL.
    GoogleUrl gurl(url);
    DCHECK(gurl.IsWebValid());
#endif  // NDEBUG

    // TODO(sligocki): Remove URL trimming code from CSS path?
    GoogleString trimmed_url;
    if (options_->trim_urls_in_css() &&
        options_->Enabled(RewriteOptions::kLeftTrimUrls) &&
        UrlLeftTrimFilter::Trim(
            trim_url_, url, &trimmed_url,
            resource()->server_context()->message_handler())) {
      // TODO(sligocki): Make sure this is the correct (final) URL of the CSS.
      DirectSetUrl(trimmed_url);
    } else {
      DirectSetUrl(RelativizeOrPassthrough(options_, url, url_relativity_,
                                           trim_url_));
    }
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

bool CssResourceSlot::DirectSetUrl(const StringPiece& url) {
  // We should never try to render unauthorized resource URLs as is.
  if (!resource()->is_authorized_domain()) {
    return false;
  }
  delete (*values_)[value_index_];
  (*values_)[value_index_] =
      new Css::Value(Css::Value::URI,
                     UTF8ToUnicodeText(url.data(), url.size()));
  return true;
}

bool CssResourceSlotFactory::SlotComparator::operator()(
    const CssResourceSlotPtr& p, const CssResourceSlotPtr& q) const {
  return (std::make_pair(p->values(), p->value_index()) <
              std::make_pair(q->values(), q->value_index()));
}

CssResourceSlotFactory::~CssResourceSlotFactory() {
}

CssResourceSlotPtr CssResourceSlotFactory::GetSlot(
    const ResourcePtr& resource, const GoogleUrl& trim_url,
    const RewriteOptions* options, Css::Values* values, size_t value_index) {
  CssResourceSlot* slot_obj =
      new CssResourceSlot(resource, trim_url, options, values, value_index);
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
