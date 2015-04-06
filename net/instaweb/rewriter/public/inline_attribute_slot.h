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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_INLINE_ATTRIBUTE_SLOT_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_INLINE_ATTRIBUTE_SLOT_H_

#include "net/instaweb/rewriter/public/resource_slot.h"

namespace net_instaweb {

class InlineAttributeSlot : public ResourceSlot {
 public:
  InlineAttributeSlot(const ResourcePtr& resource,
                      HtmlElement* element,
                      HtmlElement::Attribute* attribute,
                      StringPiece location);
  virtual ~InlineAttributeSlot();
  virtual HtmlElement* element() const { return element_; }
  virtual GoogleString LocationString() const { return location_; }

  virtual void Render();

  const HtmlElement::Attribute* attribute() const { return attribute_; }

 private:
  HtmlElement* element_;
  HtmlElement::Attribute* attribute_;
  GoogleString location_;

  DISALLOW_COPY_AND_ASSIGN(InlineAttributeSlot);
};

typedef RefCountedPtr<InlineAttributeSlot> InlineAttributeSlotPtr;

class InlineAttributeSlotComparator {
 public:
  bool operator()(const InlineAttributeSlotPtr& p,
                  const InlineAttributeSlotPtr& q) const;
};

typedef std::set<InlineAttributeSlotPtr,
                 InlineAttributeSlotComparator> InlineAttributeSlotSet;

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_INLINE_ATTRIBUTE_SLOT_H_
