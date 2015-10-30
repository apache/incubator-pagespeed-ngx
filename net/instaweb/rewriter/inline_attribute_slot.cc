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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/inline_attribute_slot.h"

namespace net_instaweb {

InlineAttributeSlot::InlineAttributeSlot(const ResourcePtr& resource,
                                         HtmlElement* element,
                                         HtmlElement::Attribute* attribute,
                                         StringPiece location)
    : ResourceSlot(resource),
      element_(element),
      attribute_(attribute),
      location_(location.data(), location.size()) {
}

InlineAttributeSlot::~InlineAttributeSlot() {
}

void InlineAttributeSlot::Render() {
  if (!disable_rendering()) {
    DCHECK(attribute_ != NULL);
    if (attribute_ != NULL) {
      attribute_->SetValue(resource()->ExtractUncompressedContents());
    }
  }
}

// Note: Copied from HtmlResourceSlotComparator.
bool InlineAttributeSlotComparator::operator()(
    const InlineAttributeSlotPtr& p, const InlineAttributeSlotPtr& q) const {
  // Note: The ordering depends on pointer comparison and so is arbitrary
  // and non-deterministic.
  if (p->element() < q->element()) {
    return true;
  } else if (p->element() > q->element()) {
    return false;
  }
  return (p->attribute() < q->attribute());
}

}  // namespace net_instaweb
