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
#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/rewriter/public/dependency_tracker.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/http/data_url.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/semantic_type.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/parser.h"

namespace net_instaweb {

class CollectDependenciesFilter::Context : public RewriteContext {
 public:
  Context(DependencyType type, RewriteDriver* driver)
      : RewriteContext(driver, nullptr, nullptr),
        mutex_(driver->server_context()->thread_system()->NewMutex()),
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

  bool Partition(OutputPartitions* partitions,
                 OutputResourceVector* outputs) override {
    // We will never produce output, but always want to do stuff.
    outputs->push_back(OutputResourcePtr(nullptr));
    partitions->add_partition();

    ResourcePtr resource(slot(0)->resource());
    if (resource->loaded()) {
      resource->AddInputInfoToPartition(
          Resource::kIncludeInputHash, 0, partitions->mutable_partition(0));
    }
    return true;
  }

  static bool DefinitelyNeededToRender(
      const std::unique_ptr<Css::Import>& import) {
    StringVector media_types;
    if (!css_util::ConvertMediaQueriesToStringVector(
            import->media_queries(), &media_types)) {
      // Something we don't understand. This includes things specifying
      // media queries, which we can't evaluate, and therefore conservatively
      // assume to be potentially unneeded.
      return false;
    }
    return DefinitelyNeededToRender(media_types);
  }

  static bool DefinitelyNeededToRender(const StringVector& media_types) {
    if (media_types.empty()) {
      return true;  // @import "foo", without media specified.
    }

    for (auto& medium : media_types) {
      if (StringCaseEqual(medium, "all") || StringCaseEqual(medium, "screen")) {
        return true;
      }
    }
    return false;
  }

 protected:
  void ExtractNestedCssDependencies(const Dependency* parent_dep,
                                    const ResourcePtr& resource,
                                    CachedResult* partition) {
    // TODO(morlovich): We should probably look inside <style> blocks like this,
    // too?

    // Don't crash out on resources without anything loaded, and don't try to
    // parse error pages for CSS imports.
    if (!resource->HttpStatusOk()) {
      return;
    }
    Css::Parser parser(resource->ExtractUncompressedContents());
    parser.set_preservation_mode(true);
    // We avoid quirks-mode so that we do not "fix" something we shouldn't have.
    parser.set_quirks_mode(false);

    while (true) {
      std::unique_ptr<Css::Import> import(parser.ParseNextImport());
      if (import == nullptr ||
          parser.errors_seen_mask() != Css::Parser::kNoError) {
        break;
      }

      if (DefinitelyNeededToRender(import)) {
        GoogleString rel_url(
            import->link().utf8_data(), import->link().utf8_length());
        GoogleUrl full_url(GoogleUrl(resource->url()), rel_url);
        if (full_url.IsWebValid()) {
          Dependency* dep = partition->add_collected_dependency();
          dep->set_url(full_url.Spec().as_string());
          dep->set_content_type(DEP_CSS);
          *dep->mutable_validity_info() = parent_dep->validity_info();
        }
      }
    }
  }

  void Rewrite(int partition_index,
               CachedResult* partition,
               const OutputResourcePtr& output_resource) override {
    Dependency* dep = partition->add_collected_dependency();
    dep->set_url(slot(0)->resource()->url());
    dep->set_content_type(dep_type_);

    // The framework collected input info from any filter that ran before
    // us, but not us (since it will do it after we finish work) --- which
    // matters if our input is an unoptimized result, so add in our input info.
    for (int i = 0; i < partition->input_size(); ++i) {
      slot(0)->ReportInput(partition->input(i));
    }

    if (slot(0)->inputs() != nullptr) {
      for (const InputInfo& input : *slot(0)->inputs()) {
        InputInfo* stored_copy = dep->add_validity_info();
        *stored_copy = input;

        // Drop the parts of the info we can't use for checking validity
        // of push.
        stored_copy->clear_input_content_hash();
        stored_copy->clear_disable_further_processing();
        stored_copy->clear_index();
      }
    }

    // Note: this needs to happen after the above since we need to propagate
    // validity_info.
    if (dep_type_ == DEP_CSS) {
      ExtractNestedCssDependencies(dep, slot(0)->resource(), partition);
    }

    // TODO(morlovich): is_pagespeed_resource is not currently set, but I am not
    // sure I actually want that: validity_info may be useful for non-optimized
    // resources as well, and we set that already.

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
    {
      ScopedMutex hold(mutex_.get());
      if (reported_) {
        return;
      }
      reported_ = true;
    }

    // We don't have results in time (and if we did, we wouldn't be able to
    // access them from this thread), so give up on propagating to pcache for
    // this time. This is somewhat conservative: if this is actually an early
    // flush window we could deliver the result to depedency_tracker safely,
    // but then if it's after document end it would have us miss the cache
    // commit entirely...
    Driver()->dependency_tracker()->ReportDependencyCandidate(dep_id_, nullptr);
  }

  void Cancel() override {
    Report();
  }

 private:
  void Report() {
    {
      ScopedMutex hold(mutex_.get());
      if (reported_) {
        return;
      }
      reported_ = true;
    }

    DependencyTracker* dep_tracker = Driver()->dependency_tracker();

    // We already allocated dep_id_, so we should report on it, with either
    // the first dependency we collected, or nullptr.
    if (num_output_partitions() == 1 &&
        output_partition(0)->collected_dependency_size() > 0) {
      CachedResult* result = output_partition(0);

      // Top-level stuff just gets its dep_id_ as the sorting key.
      result->mutable_collected_dependency(0)->add_order_key(dep_id_);

      dep_tracker->ReportDependencyCandidate(dep_id_,
                                             &result->collected_dependency(0));

      // Any other dependencies stored in result->collected_dependency >= 1
      // are things we discovered *inside* whatever is described by
      // result->collected_dependency(0)
      //
      // We grab a brand new ID for each one's storage inside
      // dependency_tracker, and give them sorting keys based on the parent's
      // dep_id_: (dep_id_, 1), (dep_id_, 2), etc., and so on, to make them get
      // sorted after their parent (whose sorting key will be (dep_id_)) and
      // before the next top-level resource, which will be something like
      // (dep_id_ + 1) or some larger number. Note that we produce order keys
      // at most 2 deep because we (for now?) only collect dependencies that
      // deep.
      for (int c = 1; c < result->collected_dependency_size(); ++c) {
        int additional_dep_id = dep_tracker->RegisterDependencyCandidate();
        Dependency* child_dep = result->mutable_collected_dependency(c);
        child_dep->add_order_key(dep_id_);
        child_dep->add_order_key(c);
        dep_tracker->ReportDependencyCandidate(additional_dep_id, child_dep);
      }
    } else {
      dep_tracker->ReportDependencyCandidate(dep_id_, nullptr);
    }
  }

  std::unique_ptr<AbstractMutex> mutex_;
  bool reported_ GUARDED_BY(mutex_);
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

      // Check media on standard stylesheets.
      if (attributes[i].category == semantic_type::kStylesheet &&
          element->keyword() == HtmlName::kLink &&
          attr->keyword() == HtmlName::kHref) {
        HtmlElement::Attribute* media =
            element->FindAttribute(HtmlName::kMedia);
        if (media != nullptr) {
          if (media->DecodedValueOrNull() == nullptr) {
            // Encoding weirdness with media attribute -> don't push
            continue;
          }
          StringVector media_vector;
          css_util::VectorizeMediaAttribute(media->DecodedValueOrNull(),
                                            &media_vector);
          if (!Context::DefinitelyNeededToRender(media_vector)) {
            continue;
          }
        }
      }

      ResourcePtr resource(
          CreateInputResourceOrInsertDebugComment(url, element));
      if (resource.get() == nullptr) {
        // TODO(morlovich): This may mean a valid 3rd party resource;
        // we also probably don't want a warning in that case.
        continue;
      }
      ResourceSlotPtr slot(driver()->GetSlot(resource, element, attr));
      slot->set_need_aggregate_input_info(true);
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
