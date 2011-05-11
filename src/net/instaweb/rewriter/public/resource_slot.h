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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_SLOT_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_SLOT_H_

#include <set>
#include <vector>

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"

namespace net_instaweb {

class HtmlResourceSlot;
class ResourceSlot;

typedef RefCountedPtr<ResourceSlot> ResourceSlotPtr;
typedef RefCountedPtr<HtmlResourceSlot> HtmlResourceSlotPtr;
typedef std::vector<ResourceSlotPtr> ResourceSlotVector;

// A slot is a place in a web-site resource a URL is found, and may be
// rewritten.  Types of slots include HTML element attributes and CSS
// background URLs.  In principle they could also include JS ajax
// requests, although this is NYI.
class ResourceSlot : public RefCounted<ResourceSlot> {
 public:
  explicit ResourceSlot(const ResourcePtr& resource) : resource_(resource) {
  }

  ResourcePtr resource() const { return resource_; }

  // Note that while slots can be mutated by multiple threads; they are
  // implemented with thread-safety in mind -- only mainline render their
  // results back into the DOM.
  //
  // For example, SetResource may be run from a helper-thread, but we
  // would not want that threaded mutation to propagate instantly back
  // into the HTML or CSS DOM.  We buffer the changes in the ResoureSlot
  // and then render them in the request thread, synchronous to the
  // HTML filter execution.
  //
  // TODO(jmarantz): Add a lock or that we or an overall protocol
  // preventing unwanted interference between renderer's reads and
  // worker writes.
  void SetResource(const ResourcePtr& resource);

  // Render is not thread-safe.  This must be called from the thread that
  // owns the DOM or CSS file.
  virtual void Render() = 0;

 protected:
  virtual ~ResourceSlot();
  REFCOUNT_FRIEND_DECLARATION(ResourceSlot);

 private:
  ResourcePtr resource_;

  DISALLOW_COPY_AND_ASSIGN(ResourceSlot);
};

// A resource-slot created for a Fetch has an empty Render method -- Render
// should never be called.
class FetchResourceSlot : public ResourceSlot {
 public:
  explicit FetchResourceSlot(const ResourcePtr& resource)
      : ResourceSlot(resource) {
  }

 protected:
  REFCOUNT_FRIEND_DECLARATION(FetchResourceSlot);
  virtual ~FetchResourceSlot();
  virtual void Render();

 private:
  DISALLOW_COPY_AND_ASSIGN(FetchResourceSlot);
};

class HtmlResourceSlot : public ResourceSlot {
 public:
  HtmlResourceSlot(const ResourcePtr& resource,
                   HtmlElement* element,
                   HtmlElement::Attribute* attribute)
      : ResourceSlot(resource),
        element_(element),
        attribute_(attribute) {
  }

  HtmlElement* element() { return element_; }
  HtmlElement::Attribute* attribute() { return attribute_; }

 protected:
  REFCOUNT_FRIEND_DECLARATION(HtmlResourceSlot);
  virtual ~HtmlResourceSlot();
  virtual void Render();

 private:
  HtmlElement* element_;
  HtmlElement::Attribute* attribute_;

  DISALLOW_COPY_AND_ASSIGN(HtmlResourceSlot);
};

class HtmlResourceSlotComparator {
 public:
  bool operator()(const HtmlResourceSlotPtr& p,
                  const HtmlResourceSlotPtr& q) const;
};

typedef std::set<HtmlResourceSlotPtr,
                 HtmlResourceSlotComparator> HtmlResourceSlotSet;

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_SLOT_H_
