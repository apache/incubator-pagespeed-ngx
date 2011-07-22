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
//
// Contains CssResourceSlot (for representing locations in CSS AST during async
// rewrites) and CssResourceSlotFactory (for getting the same slot object for
// the same location).

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_RESOURCE_SLOT_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_RESOURCE_SLOT_H_

#include <cstddef>
#include <set>

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"

namespace Css { class Values; }

namespace net_instaweb {

class GoogleUrl;

// A place storing a rewritable URL inside a CSS AST.
class CssResourceSlot : public ResourceSlot {
 public:
  virtual void Render();

  Css::Values* values() const { return values_; }
  size_t value_index() const { return value_index_; }

  // Enables trimming of URLs versus a given base.
  void EnableTrim(const GoogleUrl& base_url);

 protected:
  CssResourceSlot(const ResourcePtr& resource, Css::Values* values,
                  size_t value_index)
      : ResourceSlot(resource),
        values_(values),
        value_index_(value_index) {
  }

  REFCOUNT_FRIEND_DECLARATION(CssResourceSlot);
  virtual ~CssResourceSlot();

 private:
  friend class CssResourceSlotFactory;

  Css::Values* values_;
  size_t value_index_;
  scoped_ptr<GoogleUrl> trim_base_;  // NULL if not trimming.

  DISALLOW_COPY_AND_ASSIGN(CssResourceSlot);
};

typedef RefCountedPtr<CssResourceSlot> CssResourceSlotPtr;

// Helper factory that makes sure we get a single CSS object for given value
// slot in the CSS AST.
class CssResourceSlotFactory {
 public:
  CssResourceSlotFactory() {}
  ~CssResourceSlotFactory();

  // Warning: this is only safe if the declaration containing this property is
  // not modified while this exists.
  CssResourceSlotPtr GetSlot(const ResourcePtr& resource,
                             Css::Values* values, size_t value_index);
  CssResourceSlotPtr UniquifySlot(CssResourceSlotPtr slot);

 private:
  class SlotComparator {
   public:
    bool operator()(const CssResourceSlotPtr& p,
                    const CssResourceSlotPtr& q) const;
  };
  typedef std::set<CssResourceSlotPtr, SlotComparator> SlotSet;

  SlotSet slots_;
  DISALLOW_COPY_AND_ASSIGN(CssResourceSlotFactory);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_RESOURCE_SLOT_H_
