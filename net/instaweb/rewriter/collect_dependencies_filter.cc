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

#include "net/instaweb/rewriter/public/collect_dependencies_filter.h"

#include <memory>

#include "base/logging.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/dependencies.pb.h"
#include "net/instaweb/rewriter/public/dependency_tracker.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/http/data_url.h"
#include "pagespeed/kernel/http/semantic_type.h"

namespace net_instaweb {


class CollectDependenciesFilter::Context : public RewriteContext {
 public:
  Context(DependencyType type, RewriteDriver* driver)
      : RewriteContext(driver, nullptr, nullptr),
        reported_(false),
        dep_type_(type),
        dep_id_(-1) {
  }

  void Initiated() {
    dep_id_ = Driver()->dependency_tracker()->RegisterDependencyCandidate();
  }

  ~Context() override {
    CHECK(reported_ || dep_id_ == -1);
  }

 protected:
  bool Partition(OutputPartitions* partitions,
                 OutputResourceVector* outputs) override {
    // We will never produce output, but always want to do stuff.
    outputs->push_back(OutputResourcePtr(nullptr));
    partitions->add_partition();
    return true;
  }

  void Rewrite(int partition_index,
               CachedResult* partition,
               const OutputResourcePtr& output_resource) override {
    Dependency* dep = partition->mutable_collected_dependency();
    dep->set_url(slot(0)->resource()->url());
    dep->set_content_type(dep_type_);

    // TODO(morlovich): Set validity_info.
    // This is surprisingly complicated, since essentially we have to get info
    // from all the steps along the RewriteContext, and the previous
    // RewriteContexts already got deleted. (This isn't needed in normal
    // operation since invalidation in the middle of a chain would change
    // the input URL in the middle of the chain, but we are trying to skip
    // to the end).
    // (is_pagespeed_resource is also not currently set, but I am not sure
    //  I actually want that: validity_info may be useful for non-optimized
    //  resources as well).

    CHECK(output_resource.get() == nullptr);
    CHECK_EQ(0, partition_index);
    RewriteDone(kRewriteFailed, 0);
  }

  OutputResourceKind kind() const override { return kOnTheFlyResource; }

  const char* id() const override {
    return "cdf";
  }

  void Render() override {
    Report();
  }

  void WillNotRender() override {
    Report();
  }

  void Cancel() override {
    Report();
  }

 private:
  void Report() {
    if (reported_) {
      // WillNotRender followed by Cancel can happen.
      return;
    }

    if (num_output_partitions() == 1) {
      CachedResult* result = output_partition(0);

      Driver()->dependency_tracker()->ReportDependencyCandidate(
          dep_id_,
          result->has_collected_dependency() ?
              &result->collected_dependency() : nullptr);
    } else {
      Driver()->dependency_tracker()->ReportDependencyCandidate(
          dep_id_, nullptr);
    }
    reported_ = true;
  }

  bool reported_;
  DependencyType dep_type_;
  int dep_id_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

CollectDependenciesFilter::CollectDependenciesFilter(RewriteDriver* driver)
    : CommonFilter(driver) {
}

void CollectDependenciesFilter::StartDocumentImpl() {
}

void CollectDependenciesFilter::StartElementImpl(HtmlElement* element) {
  // We generally don't want noscript path stuff, since it's not usually
  // used.
  if (noscript_element() != nullptr) {
    // Do nothing
    return;
  }

  resource_tag_scanner::UrlCategoryVector attributes;
  resource_tag_scanner::ScanElement(
      element, driver()->options(), &attributes);
  for (int i = 0, n = attributes.size(); i < n; ++i) {
    // We only collect scripts and CSS.
    if (attributes[i].category == semantic_type::kStylesheet ||
        attributes[i].category == semantic_type::kScript) {
      HtmlElement::Attribute* attr = attributes[i].url;
      StringPiece url(attr->DecodedValueOrNull());
      if (url.empty() || IsDataUrl(url)) {
        continue;
      }

      // TODO(morlovich): This should probably pay attention to things like
      // media= on CSS. We don't want to prioritize loading of the print
      // stylesheet.

      ResourcePtr resource(
          CreateInputResourceOrInsertDebugComment(url, element));
      if (resource.get() == nullptr) {
        // TODO(morlovich): This may mean a valid 3rd party resource;
        // we also probably don't want a warning in that case.
        continue;
      }
      ResourceSlotPtr slot(driver()->GetSlot(resource, element, attr));
      Context* context = new Context(
          attributes[i].category == semantic_type::kStylesheet ?
              DEP_CSS : DEP_JAVASCRIPT,
          driver());
      context->AddSlot(slot);
      if (driver()->InitiateRewrite(context)) {
        context->Initiated();
      }
    }
  }
}

void CollectDependenciesFilter::EndElementImpl(HtmlElement* element) {
}

}  // namespace net_instaweb
