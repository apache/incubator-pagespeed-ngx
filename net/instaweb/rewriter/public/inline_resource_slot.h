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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_INLINE_RESOURCE_SLOT_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_INLINE_RESOURCE_SLOT_H_

#include "net/instaweb/rewriter/public/resource_slot.h"

namespace net_instaweb {

// ResourceSlot for inline CSS or JS (which are the contents of a Characters
// block instead of being the result of an HTTP fetch).
// Note: Inline JS does not currently use this class. Instead it is rewritten
// in the parsing thread.
// TODO(sligocki): This is currently being used for CSS attribute rewriting
// too. Use a separate Slot for that.
class InlineResourceSlot : public ResourceSlot {
 public:
  // TODO(sligocki): Construct resource in this function??
  InlineResourceSlot(const ResourcePtr& resource,
                     HtmlCharactersNode* char_node,
                     StringPiece location);

  // Debug information should be placed next to <style> or <script> block
  // surrounding the Characters node.
  virtual HtmlElement* element() const { return char_node_->parent(); }

  virtual void Render();
  virtual GoogleString LocationString() const;

 protected:
  REFCOUNT_FRIEND_DECLARATION(InlineResourceSlot);
  virtual ~InlineResourceSlot();

 private:
  HtmlCharactersNode* char_node_;
  const GoogleString location_;

  DISALLOW_COPY_AND_ASSIGN(InlineResourceSlot);
};

typedef RefCountedPtr<InlineResourceSlot> InlineResourceSlotPtr;

class InlineResourceSlotComparator {
 public:
  bool operator()(const InlineResourceSlotPtr& p,
                  const InlineResourceSlotPtr& q) const;
};

typedef std::set<InlineResourceSlotPtr,
                 InlineResourceSlotComparator> InlineResourceSlotSet;

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_INLINE_RESOURCE_SLOT_H_
