/*
 * Copyright 2012 Google Inc.
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
// Author: mmohabey@google.com (Megha Mohabey)

#include "net/instaweb/rewriter/public/collect_subresources_filter.h"

#include <map>
#include <set>
#include <utility>

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

// CollectSubresourcesFilter::Context maintains the context of all the
// subresource urls seen in the head. By being at the end of the rewriting chain
// it ensures that we get the url at after all the rewriting is done.
class CollectSubresourcesFilter::Context : public SingleRewriteContext {
 public:
  Context(RewriteDriver* driver, int resource_id, ResourceMap* subresources,
          AbstractMutex* mutex)
      : SingleRewriteContext(driver, NULL, NULL),
        resource_id_(resource_id),
        populated_resource_(false),
        subresources_(subresources),
        mutex_(mutex) {}

 protected:
  virtual void RewriteSingle(const ResourcePtr& input,
                             const OutputResourcePtr& output) {
    GetSubresource();
    RewriteDone(kRewriteFailed, 0);
  }

  virtual void Render() {
    GetSubresource();
  }

  virtual OutputResourceKind kind() const { return kOnTheFlyResource; }

  virtual const char* id() const {
    return "fs";
  }

 private:
  // Gets the rewritten subresource URL.
  void GetSubresource() {
    if (populated_resource_) {
      return;
    }
    populated_resource_ = true;
    // Do not add resources which are inlined or combined.
    if (num_slots() == 0 || slot(0)->disable_rendering() ||
        slot(0)->should_delete_element()) {
      return;
    }
    ResourceSlot* resource_slot = slot(0).get();
    if (resource_slot->was_optimized()) {
      ResourcePtr resource = resource_slot->resource();
      GoogleString url = resource->url();
      if (!url.empty()) {
        const ContentType* content_type = resource->type();
        if (content_type == NULL) {
          content_type = resource->response_headers()->DetermineContentType();
          if (content_type == NULL) {
            content_type = NameExtensionToContentType(url);
          }
        }
        FlushEarlyContentType type = GetFlushEarlyContentType(
            content_type->type());
        if (type != OTHER) {
          FlushEarlyResource* flush_early_resource = new FlushEarlyResource;
          flush_early_resource->set_rewritten_url(url);
          flush_early_resource->set_content_type(type);
          {
            ScopedMutex lock(mutex_);
            (*subresources_)[resource_id_] = flush_early_resource;
          }
        }
      }
    }
  }

  FlushEarlyContentType GetFlushEarlyContentType(ContentType::Type type) {
    switch (type) {
      case ContentType::kJavascript:
        return JAVASCRIPT;
      case ContentType::kCss:
        return CSS;
      default:
        return OTHER;
    }
  }

  int resource_id_;  // The seq_no of the resource in the head.
  bool populated_resource_;  // If the FlushEarlyResource is populated.
  ResourceMap* subresources_;
  AbstractMutex* mutex_;
};

CollectSubresourcesFilter::CollectSubresourcesFilter(RewriteDriver* driver)
    : RewriteFilter(driver),
      num_resources_(0),
      mutex_(driver->resource_manager()->thread_system()->NewMutex()),
      property_cache_(driver->resource_manager()->page_property_cache()) {
}

void CollectSubresourcesFilter::StartDocumentImpl() {
  in_first_head_ = false;
  seen_first_head_ = false;
  num_resources_ = 0;
  STLDeleteValues(&subresources_);
}

CollectSubresourcesFilter::~CollectSubresourcesFilter() {
  STLDeleteValues(&subresources_);
}

void CollectSubresourcesFilter::StartElementImpl(HtmlElement* element) {
  if (!driver()->UserAgentSupportsFlushEarly()) {
    return;
  }
  if (element->keyword() == HtmlName::kHead && !seen_first_head_) {
    seen_first_head_ = true;
    in_first_head_ = true;
    return;
  }
  if (in_first_head_) {
    semantic_type::Category category;
    HtmlElement::Attribute* src = resource_tag_scanner::ScanElement(
        element, driver(), &category);
    if (src == NULL) {
      return;
    }
    if (category != semantic_type::kStylesheet &&
        category != semantic_type::kScript) {
      return;
    }
    // Skip data URIs.
    StringPiece src_value(src->DecodedValueOrNull());
    if (src_value.empty() || src_value.starts_with("data:")) {
      return;
    }
    CreateSubresourceContext(src_value, element, src);
  }
}

void CollectSubresourcesFilter::EndElementImpl(HtmlElement* element) {
  if (!driver()->UserAgentSupportsFlushEarly()) {
    return;
  }
  if (element->keyword() == HtmlName::kHead && in_first_head_) {
    in_first_head_ = false;
  }
}

void CollectSubresourcesFilter::CreateSubresourceContext(
    StringPiece url,
    HtmlElement* element,
    HtmlElement::Attribute* attr) {
  ++num_resources_;
  ResourcePtr resource = CreateInputResource(url);
  if (resource.get() != NULL) {
    ResourceSlotPtr slot(driver()->GetSlot(resource, element, attr));
    Context* context = new Context(driver(), num_resources_, &subresources_,
                                   mutex_.get());
    context->AddSlot(slot);
    driver()->InitiateRewrite(context);
  }
}

// TODO(mmohabey): Add the scripts added by other filters in this list.
void CollectSubresourcesFilter::AddSubresourcesToFlushEarlyInfo(
    FlushEarlyInfo* info) {
  ResourceMap::const_iterator it;
  StringSet subresources_set;
  std::pair<StringSet::iterator, bool> ret;

  info->clear_subresource();
  // Add the subresources to the property cache in the order they are seen in
  // head.
  for (it = subresources_.begin(); it != subresources_.end(); ++it) {
    // Adding to the set to figure out if we have this subresource link added
    // to the property cache already. If so do not add it again.
    ret = subresources_set.insert(it->second->rewritten_url());
    if (ret.second == true) {
      info->add_subresource()->CopyFrom(*(it->second));
    }
  }
}

}  // namespace net_instaweb
