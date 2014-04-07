/*
 * Copyright 2010 Google Inc.
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
//
// Contains implementation of CssCombineFilter, which concatenates multiple
// CSS files into one. Implemented in part via delegating to
// CssCombineFilter::CssCombiner, a ResourceCombiner subclass.

#include "net/instaweb/rewriter/public/css_combine_filter.h"

#include <vector>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_combiner.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/charset_util.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"
#include "webutil/css/parser.h"

namespace net_instaweb {

class MessageHandler;
class HtmlIEDirectiveNode;
class UrlSegmentEncoder;

// Names for Statistics variables.
const char CssCombineFilter::kCssCombineOpportunities[] =
    "css_combine_opportunities";
const char CssCombineFilter::kCssFileCountReduction[] =
    "css_file_count_reduction";

// Combining helper. Takes care of checking that media matches, that we do not
// produce @import's in the middle and of URL absolutification.
class CssCombineFilter::CssCombiner : public ResourceCombiner {
 public:
  CssCombiner(RewriteDriver* driver,
              CssCombineFilter* filter)
      : ResourceCombiner(driver, kContentTypeCss.file_extension() + 1, filter),
        combined_css_size_(0) {
    Statistics* stats = server_context_->statistics();
    css_file_count_reduction_ = stats->GetVariable(kCssFileCountReduction);
  }

  bool CleanParse(const StringPiece& contents) {
    Css::Parser parser(contents);
    parser.set_preservation_mode(true);
    // Among other issues, quirks-mode allows unbalanced {}s in some cases.
    parser.set_quirks_mode(false);
    // TODO(sligocki): Do parsing on low-priority worker thread.
    scoped_ptr<Css::Stylesheet> stylesheet(parser.ParseRawStylesheet());
    return (parser.errors_seen_mask() == Css::Parser::kNoError);
  }

  virtual bool ResourceCombinable(Resource* resource,
                                  GoogleString* failure_reason,
                                  MessageHandler* handler) {
    // If this CSS file is not parseable it may have errors that will break
    // the rest of the files combined with this one. So we should not include
    // it in the combination.
    // TODO(sligocki): Just do the CSS parsing and rewriting here.
    if (!CleanParse(resource->contents())) {
      *failure_reason = "CSS parse error";
      // TODO(sligocki): All parse failures are repeated twice because we will
      // try to combine them in the normal combination, then we'll try again
      // with this as the first of a new combination.
      return false;
    }

    // styles containing @import cannot be appended to others, as any
    // @import in the middle will be ignored.
    // TODO(sligocki): Do CSS parsing and rewriting here so that we can
    // git rid of this restriction.
    if ((num_urls() != 0) &&
        CssTagScanner::HasImport(resource->contents(), handler)) {
      *failure_reason = "Can't have @import in middle of CSS";
      return false;
    }

    return true;
  }

  OutputResourcePtr MakeOutput() {
    return Combine(rewrite_driver_->message_handler());
  }

  bool Write(const ResourceVector& in, const OutputResourcePtr& out) {
    return WriteCombination(in, out, rewrite_driver_->message_handler());
  }

  void set_media(const char* media) { media_ = media; }
  const GoogleString& media() const { return media_; }

  void AddFileCountReduction(int num_files) {
    css_file_count_reduction_->Add(num_files);
    if (num_files >= 1) {
      rewrite_driver_->log_record()->SetRewriterLoggingStatus(
          RewriteOptions::FilterId(RewriteOptions::kCombineCss),
          RewriterApplication::APPLIED_OK);
    }
  }

  virtual bool ContentSizeTooBig() const {
    int64 combined_css_max_size =
      rewrite_driver_->options()->max_combined_css_bytes();
    return (combined_css_max_size >= 0 &&
            combined_css_max_size < combined_css_size_);
  }

  virtual void AccumulateCombinedSize(const ResourcePtr& resource) {
    combined_css_size_ += resource->contents().size();
  }

  virtual void Clear() {
    ResourceCombiner::Clear();
    combined_css_size_ = 0;
  }

 private:
  virtual const ContentType* CombinationContentType() {
    return &kContentTypeCss;
  }

  virtual bool WritePiece(int index, const Resource* input,
                          OutputResource* combination, Writer* writer,
                          MessageHandler* handler);

  GoogleString media_;
  Variable* css_file_count_reduction_;
  int64 combined_css_size_;
};

class CssCombineFilter::Context : public RewriteContext {
 public:
  Context(RewriteDriver* driver, CssCombineFilter* filter)
      : RewriteContext(driver, NULL, NULL),
        filter_(filter),
        combiner_(driver, filter),
        new_combination_(true) {
  }

  CssCombiner* combiner() { return &combiner_; }

  bool AddElement(HtmlElement* element, HtmlElement::Attribute* href) {
    bool ret = false;
    ResourcePtr resource(filter_->CreateInputResource(
        href->DecodedValueOrNull()));
    if (resource.get() != NULL) {
      ResourceSlotPtr slot(Driver()->GetSlot(resource, element, href));
      AddSlot(slot);
      elements_.push_back(element);
      ret = true;
    }
    return ret;
  }

  bool empty() const { return elements_.empty(); }
  bool new_combination() const { return new_combination_; }

  void Reset() {
    combiner_.Reset();
    combiner_.set_media("");
    new_combination_ = true;
  }

  void SetMedia(const char* media) {
    combiner_.set_media(media);
    new_combination_ = false;
  }

 protected:
  virtual bool Partition(OutputPartitions* partitions,
                         OutputResourceVector* outputs) {
    MessageHandler* handler = Driver()->message_handler();
    CachedResult* partition = NULL;
    CHECK_EQ(static_cast<int>(elements_.size()), num_slots());
    for (int i = 0, n = num_slots(); i < n; ++i) {
      bool add_input = false;
      ResourcePtr resource(slot(i)->resource());

      if (resource->IsSafeToRewrite(rewrite_uncacheable())) {
        if (combiner_.AddResourceNoFetch(resource, handler).value) {
          // This new element works in the existing partition.
          add_input = true;
        } else {
          // This new element does not work in the existing partition,
          // so close out that partition if it's non-empty.
          if (partition != NULL) {
            FinalizePartition(partitions, partition, outputs);
            partition = NULL;
            if (combiner_.AddResourceNoFetch(resource, handler).value) {
              add_input = true;
            }
          }
        }
      } else {
        // A failed resource-fetch tells us to finalize any partition that
        // we've already started.  We don't want to combine across a CSS file
        // that our server sees as a 404 because the browser might successfully
        // fetch that file, and thus we'd mangle the ordering if we combined
        // across it.
        FinalizePartition(partitions, partition, outputs);
        partition = NULL;
      }
      if (add_input) {
        if (partition == NULL) {
          partition = partitions->add_partition();
        }
        resource->AddInputInfoToPartition(
            Resource::kIncludeInputHash, i, partition);
      }
    }
    FinalizePartition(partitions, partition, outputs);
    return (partitions->partition_size() != 0);
  }

  virtual void Rewrite(int partition_index,
                       CachedResult* partition,
                       const OutputResourcePtr& output) {
    // resource_combiner.cc calls WriteCombination as part
    // of Combine.  But if we are being called on behalf of a
    // fetch then the resource still needs to be written.
    RewriteResult result = kRewriteOk;
    // OutputResource CHECK-fails if you try to Write twice, which
    // would happen in the html-rewrite phase without this check.
    if (!output->IsWritten()) {
      ResourceVector resources;
      for (int i = 0, n = num_slots(); i < n; ++i) {
        ResourcePtr resource(slot(i)->resource());
        resources.push_back(resource);
      }
      if (!combiner_.Write(resources, output)) {
        result = kRewriteFailed;
      }
    }
    RewriteDone(result, partition_index);
  }

  virtual void Render() {
    for (int p = 0, np = num_output_partitions(); p < np; ++p) {
      CachedResult* partition = output_partition(p);
      if (partition->input_size() == 0) {
        continue;
      }

      // We need to be sure this is HTML to omit the "/" before the
      // ">".  If the content-type is not known then make sure we use
      // "<link ... />".
      if (filter_->driver()->MimeTypeXhtmlStatus() !=
          RewriteDriver::kIsNotXhtml) {
        int first_element_index = partition->input(0).index();
        HtmlElement* first_element = elements_[first_element_index];
        first_element->set_close_style(HtmlElement::BRIEF_CLOSE);
      }

      // We want to call this here so that we disable_further_processing
      // and delete elements in cases where we Render() but don't partition
      // (cache hits).
      DisableRemovedSlots(partition);

      combiner_.AddFileCountReduction(partition->input_size() - 1);
    }
  }

  virtual const UrlSegmentEncoder* encoder() const {
    return filter_->encoder();
  }
  virtual const char* id() const { return filter_->id(); }
  virtual OutputResourceKind kind() const { return kRewrittenResource; }

 private:
  void FinalizePartition(OutputPartitions* partitions,
                         CachedResult* partition,
                         OutputResourceVector* outputs) {
    if (partition != NULL) {
      OutputResourcePtr combination_output(combiner_.MakeOutput());
      if (combination_output.get() == NULL) {
        partitions->mutable_partition()->RemoveLast();
      } else {
        combination_output->UpdateCachedResultPreservingInputInfo(partition);
        outputs->push_back(combination_output);

        // We want to call this here so that we disable_further_processing
        // even in cases where we do not Render().
        DisableRemovedSlots(partition);
      }
      Reset();
    }
  }

  void DisableRemovedSlots(CachedResult* partition) {
    // Slot 0 will be replaced by the combined resource as part of
    // rewrite_context.cc.  But we still need to delete links for slots 1-N,
    // and to prevent further acting on them.
    for (int i = 1; i < partition->input_size(); ++i) {
      int slot_index = partition->input(i).index();
      slot(slot_index)->RequestDeleteElement();
    }
  }

  std::vector<HtmlElement*> elements_;
  RewriteFilter* filter_;
  CssCombineFilter::CssCombiner combiner_;
  bool new_combination_;
  DISALLOW_COPY_AND_ASSIGN(Context);
};

// TODO(jmarantz) We exhibit zero intelligence about which css files to
// combine; we combine whatever is possible.  This can reduce performance
// by combining highly cacheable shared resources with transient ones.
//
// TODO(jmarantz): We do not recognize IE directives as spriting boundaries.
// We should supply a meaningful IEDirective method as a boundary.
//
// TODO(jmarantz): allow combining of CSS elements found in the body, whether
// or not the head has already been flushed.
//
// TODO(jmaessen): The addition of 1 below avoids the leading ".";
// make this convention consistent and fix all code.
CssCombineFilter::CssCombineFilter(RewriteDriver* driver)
    : RewriteFilter(driver),
      css_tag_scanner_(driver),
      end_document_found_(false),
      css_links_(0),
      css_combine_opportunities_(driver->statistics()->GetVariable(
          kCssCombineOpportunities)) {
}

CssCombineFilter::~CssCombineFilter() {
}

void CssCombineFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kCssCombineOpportunities);
  statistics->AddVariable(kCssFileCountReduction);
}

void CssCombineFilter::StartDocumentImpl() {
  context_.reset(MakeContext());
  end_document_found_ = false;
  css_links_ = 0;
}

void CssCombineFilter::EndDocument() {
  end_document_found_ = true;
  if (css_links_ > 1) {
    // There are only opportunities to combine if there was more than one
    // css <link> in original HTML.
    css_combine_opportunities_->Add(css_links_ - 1);
  }
}

void CssCombineFilter::StartElementImpl(HtmlElement* element) {
  HtmlElement::Attribute* href;
  const char* media;
  int num_nonstandard_attributes;
  if (element->keyword() == HtmlName::kStyle) {
    // We can't reorder styles on a page, so if we are only combining <link>
    // tags, we can't combine them across a <style> tag.
    // TODO(sligocki): Maybe we should just combine <style>s too?
    // We can run outline_css first for now to make all <style>s into <link>s.
    NextCombination("inline style");
    return;
  } else if (css_tag_scanner_.ParseCssElement(element, &href, &media,
                                              &num_nonstandard_attributes)) {
    ++css_links_;
    // Element is a <link rel="stylesheet" ...>.
    if (driver()->HasChildrenInFlushWindow(element)) {
      LOG(DFATAL) << "HTML lexer allowed children in <link>.";
      NextCombination("children in flush window");
      return;
    }
    if (num_nonstandard_attributes > 0) {
      // TODO(jmaessen): allow more attributes.  This is the place it's
      // riskiest:  we can't combine multiple elements with an id, for
      // example, so we'd need to explicitly catch and handle that case.
      NextCombination("non-standard attributes");
      return;
    }
    // We cannot combine with a link in <noscript> tag and we cannot combine
    // over a link in a <noscript> tag, so this is a barrier.
    if (noscript_element() != NULL) {
      NextCombination("noscript");
      return;
    }
    // Figure out if media types match.
    if (context_->new_combination()) {
      context_->SetMedia(media);
    } else if (combiner()->media() != media) {
      // After the first CSS file, subsequent CSS files must have matching
      // media.
      // TODO(jmarantz): do media='' and media='display' mean the same
      // thing?  sligocki thinks mdsteele looked into this and it
      // depended on HTML version.  In one display was default, in the
      // other screen was IIRC.
      NextCombination("media mismatch");
      context_->SetMedia(media);
    }
    if (!context_->AddElement(element, href)) {
      NextCombination("resource not rewritable");
    }
  }
}

void CssCombineFilter::NextCombination(StringPiece debug_failure_reason) {
  if (!context_->empty()) {
    if (DebugMode() && !debug_failure_reason.empty()) {
      driver()->InsertComment(StrCat("combine_css: Could not combine over "
                                     "barrier: ", debug_failure_reason));
    }
    driver()->InitiateRewrite(context_.release());
    context_.reset(MakeContext());
  }
  context_->Reset();
}

// An IE directive that includes any stylesheet info should be a barrier
// for css combining.  It's OK to emit the combination we've seen so far.
void CssCombineFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  // TODO(sligocki): Figure out how to safely parse IEDirectives, for now we
  // just consider them black boxes / solid barriers.
  NextCombination("IE directive");
}

void CssCombineFilter::Flush() {
  // Note: We only want to log a debug comment on normal flushes, not the
  // end of document (which is not really a barrier).
  NextCombination(end_document_found_ ? "" : "flush");
}

bool CssCombineFilter::CssCombiner::WritePiece(
    int index, const Resource* input, OutputResource* combination,
    Writer* writer, MessageHandler* handler) {
  StringPiece contents = input->contents();
  GoogleUrl input_url(input->url());
  // Strip the BOM off of the contents (if it's there) if this is not the
  // first resource.
  if (index != 0) {
    StripUtf8Bom(&contents);
  }
  bool ret = false;
  switch (rewrite_driver_->ResolveCssUrls(
      input_url, combination->resolved_base(), contents, writer, handler)) {
    case RewriteDriver::kNoResolutionNeeded:
      ret = writer->Write(contents, handler);
      break;
    case RewriteDriver::kWriteFailed:
      break;
    case RewriteDriver::kSuccess:
      ret = true;
      break;
  }
  return ret;
}

CssCombineFilter::CssCombiner* CssCombineFilter::combiner() {
  return context_->combiner();
}

CssCombineFilter::Context* CssCombineFilter::MakeContext() {
  return new Context(driver(), this);
}

RewriteContext* CssCombineFilter::MakeRewriteContext() {
  return MakeContext();
}

void CssCombineFilter::DetermineEnabled() {
  set_is_enabled(!driver()->flushed_cached_html());
}

}  // namespace net_instaweb
