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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/resource_slot.h"

#include <cstddef>

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"

namespace net_instaweb {

ResourceSlot::~ResourceSlot() {
}

void ResourceSlot::SetResource(const ResourcePtr& resource) {
  resource_ = ResourcePtr(resource);
}

RewriteContext* ResourceSlot::LastContext() const {
  if (contexts_.empty()) {
    return NULL;
  }
  return contexts_.back();
}

void ResourceSlot::DetachContext(RewriteContext* context) {
  CHECK_EQ(contexts_.front(), context);
  contexts_.pop_front();
}

FetchResourceSlot::~FetchResourceSlot() {
}

void FetchResourceSlot::Render() {
  DCHECK(false) << "FetchResourceSlot::Render should never be called";
}

HtmlResourceSlot::~HtmlResourceSlot() {
}

void HtmlResourceSlot::Render() {
  if (attribute_ != NULL) {
    attribute_->SetValue(resource()->url());
    // Note, for inserting image-dimensions, we will likely have
    // to subclass or augment HtmlResourceSlot.
  }
}

// TODO(jmarantz): test sanity of set maintenance using this comparator.
bool HtmlResourceSlotComparator::operator()(const HtmlResourceSlotPtr& p,
                                            const HtmlResourceSlotPtr& q)
  const {
  if (p->element() < q->element()) {
    return true;
  } else if (p->element() > q->element()) {
    return false;
  }
  if (p->attribute() < q->attribute()) {
    return true;
  }
  return false;
}

}  // namespace net_instaweb
