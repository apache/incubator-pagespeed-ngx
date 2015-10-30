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

#include "net/instaweb/rewriter/public/inline_resource_slot.h"

#include "base/logging.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

InlineResourceSlot::InlineResourceSlot(const ResourcePtr& resource,
                                       HtmlCharactersNode* char_node,
                                       StringPiece location)
    : ResourceSlot(resource),
      char_node_(char_node),
      location_(location.data(), location.size()) {
}

InlineResourceSlot::~InlineResourceSlot() {
}

void InlineResourceSlot::Render() {
  if (!disable_rendering()) {
    DCHECK(char_node_ != NULL);
    // Note: This should be an InlineOutputResource so it will be loaded by
    // default.
    DCHECK(resource()->loaded());
    DCHECK(!resource()->response_headers()->cache_fields_dirty());
    if (char_node_ != NULL && resource()->loaded()) {
      resource()->ExtractUncompressedContents().CopyToString(
          char_node_->mutable_contents());
    }
  }
}

// TODO(sligocki): Use code from HtmlResourceSlot or pass in the RewriteDriver
// and call driver->UrlLine().
GoogleString InlineResourceSlot::LocationString() const {
  return location_;
}

bool InlineResourceSlotComparator::operator()(
    const InlineResourceSlotPtr& p, const InlineResourceSlotPtr& q) const {
  return (p->element() < q->element());
}

}  // namespace net_instaweb
