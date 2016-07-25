/*
 * Copyright 2016 Google Inc.
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
// Contains special slots that help rewrite images inside srcset attributes.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_SRCSET_SLOT_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_SRCSET_SLOT_H_

#include <memory>
#include <set>
#include <vector>

#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

class CommonFilter;
class RewriteDriver;
class SrcSetSlot;

// Since the various images inside a src attribute share the attribute, we
// hook them all up to a single SrcSetSlotCollection (which they own).
class SrcSetSlotCollection : public RefCounted<SrcSetSlotCollection> {
 public:
  struct ImageCandidate {
    ImageCandidate() : slot(nullptr) {}

    GoogleString url;
    GoogleString descriptor;

    // The slot owns us. Note that some of these may be nullptr if a resource
    // couldn't be created.
    SrcSetSlot* slot;
  };

  // Note: you need to separately call Initialize() to actually create all the
  // slots and the like. This sets up just enough to be able to compare
  // slots.
  SrcSetSlotCollection(RewriteDriver* driver,
                       HtmlElement* element,
                       HtmlElement::Attribute* attribute);

  // This will parse the passed in srcset attribute, and create all the slots,
  // and all their resources.
  void Initialize(CommonFilter* filter);

  int num_image_candidates() { return candidates_.size(); }

  // may be nullptr.
  SrcSetSlot* slot(int idx) { return candidates_[idx].slot; }
  const GoogleString& url(int idx) { return candidates_[idx].url; }
  void set_url(int idx, GoogleString new_url) {
    candidates_[idx].url = new_url;
  }
  const GoogleString& descriptor(int idx) {
    return candidates_[idx].descriptor;
  }

  HtmlElement* element() const { return element_; }
  HtmlElement::Attribute* attribute() const { return attribute_; }

  // The first filter that created this slot collection. There may be others.
  CommonFilter* filter() const { return filter_; }
  RewriteDriver* driver() { return driver_; }

  int begin_line_number() const { return begin_line_number_; }
  int end_line_number() const { return end_line_number_; }

  // This serializes everything back to the attribute.
  // (Which is sadly quadratic, but the size should be small enough that it's
  //  more practical than trying to coordinate).
  void Commit();

  // Parses the input srcset attribute into *out (replacing its contents),
  // filling in the url and descriptor fields (but not trying to create
  // resources or fill in slot).
  static void ParseSrcSet(StringPiece input, std::vector<ImageCandidate>* out);

  static GoogleString Serialize(const std::vector<ImageCandidate>& in);

 protected:
  virtual ~SrcSetSlotCollection() {}
  REFCOUNT_FRIEND_DECLARATION(SrcSetSlotCollection);

 private:
  std::vector<ImageCandidate> candidates_;
  RewriteDriver* driver_;
  HtmlElement* element_;
  HtmlElement::Attribute* attribute_;
  CommonFilter* filter_;
  int begin_line_number_;
  int end_line_number_;

  DISALLOW_COPY_AND_ASSIGN(SrcSetSlotCollection);
};

typedef RefCountedPtr<SrcSetSlotCollection> SrcSetSlotCollectionPtr;

// Note: this is non-deterministic between executions.
class SrcSetSlotCollectionComparator {
 public:
  bool operator()(const SrcSetSlotCollectionPtr& p,
                  const SrcSetSlotCollectionPtr& q) const;
};

typedef std::set<SrcSetSlotCollectionPtr,
                 SrcSetSlotCollectionComparator> SrcSetSlotCollectionSet;


class SrcSetSlot : public ResourceSlot {
 public:
  HtmlElement* element() const override { return parent_->element(); }
  void Render() override;
  GoogleString LocationString() const override;

 protected:
  friend class SrcSetSlotCollection;
  SrcSetSlot(const ResourcePtr& resource,
             SrcSetSlotCollection* parent,
             int index);

  REFCOUNT_FRIEND_DECLARATION(SrcSetSlot);
  ~SrcSetSlot() override;

 private:
  RefCountedPtr<SrcSetSlotCollection> parent_;
  int index_;
  UrlRelativity url_relativity_;

  DISALLOW_COPY_AND_ASSIGN(SrcSetSlot);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SRCSET_SLOT_H_
