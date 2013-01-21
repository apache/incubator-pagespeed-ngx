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
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/vector_deque.h"

namespace net_instaweb {

class HtmlParse;
class HtmlResourceSlot;
class ResourceSlot;
class RewriteContext;

typedef RefCountedPtr<ResourceSlot> ResourceSlotPtr;
typedef RefCountedPtr<HtmlResourceSlot> HtmlResourceSlotPtr;
typedef std::vector<ResourceSlotPtr> ResourceSlotVector;

// A slot is a place in a web-site resource a URL is found, and may be
// rewritten.  Types of slots include HTML element attributes and CSS
// background URLs.  In principle they could also include JS ajax
// requests, although this is NYI.
//
// TODO(jmarantz): make this class thread-safe.
class ResourceSlot : public RefCounted<ResourceSlot> {
 public:
  explicit ResourceSlot(const ResourcePtr& resource)
      : resource_(resource),
        disable_rendering_(false),
        should_delete_element_(false),
        disable_further_processing_(false),
        was_optimized_(false) {
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

  // If disable_rendering is true, this slot will do nothing on rendering,
  // neither changing the URL or deleting any elements. This is intended for
  // use of filters which do the entire work in the Context.
  void set_disable_rendering(bool x) { disable_rendering_ = x; }
  bool disable_rendering() const { return disable_rendering_; }

  // Determines whether rendering the slot deletes the HTML Element.
  // For example, in the CSS combine filter we want the Render to
  // rewrite the first <link href>, but delete all the other <link>s.
  //
  // Calling RequestDeleteElement() also forces
  // set_disable_further_processing(true);
  void RequestDeleteElement() {
    should_delete_element_ = true;
    disable_further_processing_ = true;
  }
  bool should_delete_element() const { return should_delete_element_; }

  // Returns true if any of the contexts touching this slot optimized it
  // successfully. This in particular includes the case where a call to
  // RewriteContext::Rewrite() on a partition containing this slot returned
  // kRewriteOk.  Note in particular that was_optimized() does not tell you
  // whether *your* filter optimized the slot!  For this you should check
  // output_partition(n)->optimizable().
  bool was_optimized() const { return was_optimized_; }

  // Marks the slot as having been optimized.
  void set_was_optimized(bool x) { was_optimized_ = x; }

  // If disable_further_processing is true, no further filter taking this slot
  // as input will run. Note that this affects only HTML rewriting
  // (or nested rewrites) since fetch-style rewrites do not share slots
  // even when more than one filter was involved. For this to persist properly
  // on cache hits it should be set before RewriteDone is called.
  // (This also means you should not be using this when partitioning failed).
  // Only later filters are affected, not the currently running one.
  void set_disable_further_processing(bool x) {
    disable_further_processing_ = x;
  }

  bool disable_further_processing() const {
    return disable_further_processing_;
  }

  // Render is not thread-safe.  This must be called from the thread that
  // owns the DOM or CSS file. The RewriteContext state machine will only
  // call ResourceSlot::Render() on slots that were optimized successfully,
  // and whose partitions are safely url_relocatable(). (Note that this is
  // different from RewriteContext::Render).
  virtual void Render() = 0;

  // Called after all contexts have had a chance to Render.
  // This is especially useful for cases where Render was never called
  // but you want something to be done to all slots.
  virtual void Finished() {}

  // Update the URL in the slot target without touching the resource. This is
  // intended for when we're inlining things as data: URLs. Note that if you
  // call this you should also call set_disable_rendering(true), or otherwise
  // the result will be overwritten. Does not alter the URL in any way.  Not
  // supported on all slot types --- presently only slots representing things
  // within CSS and HTML have this operation (others will DCHECK-fail).  Must be
  // called from within a context's Render() method.
  virtual void DirectSetUrl(const StringPiece& url);

  // Returns true if DirectSetUrl is supported by this slot (html and css right
  // now).
  virtual bool CanDirectSetUrl() { return false; }

  // Return the last context to have been added to this slot.  Returns NULL
  // if no context has been added to the slot so far.
  RewriteContext* LastContext() const;

  // Adds a new context to this slot.
  void AddContext(RewriteContext* context) { contexts_.push_back(context); }

  // Detaches a context from the slot.  This must be the first or last context
  // that was added.
  void DetachContext(RewriteContext* context);

  // Returns a human-readable description of where this slot occurs, for use
  // in log messages.
  virtual GoogleString LocationString() = 0;

 protected:
  virtual ~ResourceSlot();
  REFCOUNT_FRIEND_DECLARATION(ResourceSlot);

 private:
  ResourcePtr resource_;
  bool disable_rendering_;
  bool should_delete_element_;
  bool disable_further_processing_;
  bool was_optimized_;

  // We track the RewriteContexts that are atempting to rewrite this
  // slot, to help us build a dependency graph between ResourceContexts.
  VectorDeque<RewriteContext*> contexts_;

  DISALLOW_COPY_AND_ASSIGN(ResourceSlot);
};

// A resource-slot created for a Fetch has an empty Render method -- Render
// should never be called.
class FetchResourceSlot : public ResourceSlot {
 public:
  explicit FetchResourceSlot(const ResourcePtr& resource)
      : ResourceSlot(resource) {
  }

  virtual void Render();
  virtual GoogleString LocationString();

 protected:
  REFCOUNT_FRIEND_DECLARATION(FetchResourceSlot);
  virtual ~FetchResourceSlot();

 private:
  DISALLOW_COPY_AND_ASSIGN(FetchResourceSlot);
};

class HtmlResourceSlot : public ResourceSlot {
 public:
  HtmlResourceSlot(const ResourcePtr& resource,
                   HtmlElement* element,
                   HtmlElement::Attribute* attribute,
                   HtmlParse* html_parse)
      : ResourceSlot(resource),
        element_(element),
        attribute_(attribute),
        html_parse_(html_parse),
        begin_line_number_(element->begin_line_number()),
        end_line_number_(element->end_line_number()) {
  }

  HtmlElement* element() { return element_; }
  HtmlElement::Attribute* attribute() { return attribute_; }

  virtual void Render();
  virtual GoogleString LocationString();
  virtual void DirectSetUrl(const StringPiece& url);
  virtual bool CanDirectSetUrl() { return true; }

 protected:
  REFCOUNT_FRIEND_DECLARATION(HtmlResourceSlot);
  virtual ~HtmlResourceSlot();

 private:
  HtmlElement* element_;
  HtmlElement::Attribute* attribute_;
  HtmlParse* html_parse_;

  int begin_line_number_;
  int end_line_number_;

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
