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
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/doctype.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_combiner.h"
#include "net/instaweb/rewriter/public/resource_combiner_template.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

class MessageHandler;
class RequestHeaders;
class ResponseHeaders;
class HtmlIEDirectiveNode;
class UrlSegmentEncoder;

// {0xEF, 0xBB, 0xBF, 0x0}
const char CssCombineFilter::kUtf8Bom[] = "\0357\0273\0277";

namespace {

// names for Statistics variables.
const char kCssFileCountReduction[] = "css_file_count_reduction";

}  // namespace

// Combining helper. Takes care of checking that media matches, that we do not
// produce @import's in the middle and of URL absolutification.
class CssCombineFilter::CssCombiner
    : public ResourceCombinerTemplate<HtmlElement*> {
 public:
  CssCombiner(RewriteDriver* driver,
              CssTagScanner* css_tag_scanner,
              CssCombineFilter* filter)
      : ResourceCombinerTemplate<HtmlElement*>(
          driver, kContentTypeCss.file_extension() + 1, filter),
        css_tag_scanner_(css_tag_scanner),
        css_file_count_reduction_(NULL) {
    Statistics* stats = resource_manager_->statistics();
    if (stats != NULL) {
      css_file_count_reduction_ = stats->GetVariable(kCssFileCountReduction);
    }
  }

  virtual bool ResourceCombinable(Resource* resource, MessageHandler* handler) {
    // styles containing @import cannot be appended to others, as any
    // @import in the middle will be ignored.
    return ((num_urls() == 0)
            || !CssTagScanner::HasImport(resource->contents(), handler));
  }

  OutputResourcePtr MakeOutput() {
    return Combine(kContentTypeCss, rewrite_driver_->message_handler());
  }

  // Try to combine all the CSS files we have seen so far.
  // Insert the combined resource where the first original CSS link was.
  void TryCombineAccumulated();

  bool Write(const ResourceVector& in, const OutputResourcePtr& out) {
    return WriteCombination(in, out, rewrite_driver_->message_handler());
  }

  void set_media(const char* media) { media_ = media; }
  const GoogleString& media() const { return media_; }

 private:
  virtual bool WritePiece(const Resource* input, OutputResource* combination,
                          Writer* writer, MessageHandler* handler);

  void StripUTF8BOM(StringPiece* contents) const;

  // Returns true iff all elements in current combination can be rewritten.
  bool CanRewrite() const {
    bool ret = (num_urls() > 0);
    for (int i = 0; ret && (i < num_urls()); ++i) {
      ret = rewrite_driver_->IsRewritable(element(i));
    }
    return ret;
  }

  GoogleString media_;
  CssTagScanner* css_tag_scanner_;
  Variable* css_file_count_reduction_;
};

class CssCombineFilter::Context : public RewriteContext {
 public:
  Context(RewriteDriver* driver, CssTagScanner* scanner,
          CssCombineFilter* filter)
      : RewriteContext(driver, NULL, NULL),
        filter_(filter),
        combiner_(driver, scanner, filter),
        new_combination_(true) {
  }

  CssCombiner* combiner() { return &combiner_; }

  bool AddElement(HtmlElement* element, HtmlElement::Attribute* href) {
    bool ret = true;
    if (filter_->HasAsyncFlow()) {
      ResourcePtr resource(filter_->CreateInputResource(href->value()));
      if (resource.get() != NULL) {
        ResourceSlotPtr slot(Driver()->GetSlot(resource, element, href));
        AddSlot(slot);
        elements_.push_back(element);
      } else {
        ret = false;
      }
    } else {
      AddToCombiner(element, href);
    }
    return ret;
  }

  void AddToCombiner(HtmlElement* element, HtmlElement::Attribute* href) {
    const char* url = href->value();
    MessageHandler* handler = filter_->driver()->message_handler();

    if (!combiner_.AddElement(element, url, handler).value) {
      // This element can't be included in the previous combination,
      // so try to flush out what we have.
      combiner_.TryCombineAccumulated();

      // Now we'll try to start a new partnership with this CSS file --
      // perhaps we ran out out of space in the previous combination
      // or this file is simply in a different authorized domain, or
      // contained @Import.
      //
      // Note that it's OK if this fails; we will simply not rewrite
      // the element in that case
      combiner_.AddElement(element, url, handler);
    }
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
      HtmlElement* element = elements_[i];

      if (resource->IsValidAndCacheable()) {
        if (combiner_.AddElementNoFetch(element, resource, handler).value) {
          // This new element works in the existing partition.
          add_input = true;
        } else {
          // This new element does not work in the existing partition,
          // so close out that partition if it's non-empty.
          if (partition != NULL) {
            FinalizePartition(partitions, partition, outputs);
            partition = NULL;
            if (combiner_.AddElementNoFetch(element, resource, handler).value) {
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
    RewriteSingleResourceFilter::RewriteResult result =
        RewriteSingleResourceFilter::kRewriteOk;
    // OutputResource CHECK-fails if you try to Write twice, which
    // would happen in the html-rewrite phase without this check.
    if (!output->IsWritten()) {
      ResourceVector resources;
      for (int i = 0, n = num_slots(); i < n; ++i) {
        ResourcePtr resource(slot(i)->resource());
        resources.push_back(resource);
      }
      if (!combiner_.Write(resources, output)) {
        result = RewriteSingleResourceFilter::kRewriteFailed;
      }
    }
    RewriteDone(result, partition_index);
  }

  virtual void Render() {
    // Slot 0 will be replaced by the combined resource as part of
    // rewrite_context.cc.  But we still need to delete slots 1-N.
    for (int p = 0, np = num_output_partitions(); p < np; ++p) {
      CachedResult* partition = output_partition(p);
      if (partition->input_size() == 0) {
        continue;
      }

      if (filter_->driver()->doctype().IsXhtml()) {
        int first_element_index = partition->input(0).index();
        HtmlElement* first_element = elements_[first_element_index];
        first_element->set_close_style(HtmlElement::BRIEF_CLOSE);
      }
      for (int i = 1; i < partition->input_size(); ++i) {
        int slot_index = partition->input(i).index();
        slot(slot_index)->set_should_delete_element(true);
      }
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
      }
      Reset();
    }
  }

  std::vector<HtmlElement*> elements_;
  RewriteFilter* filter_;
  CssCombineFilter::CssCombiner combiner_;
  bool new_combination_;
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
      css_tag_scanner_(driver_) {
}

CssCombineFilter::~CssCombineFilter() {
}

void CssCombineFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kCssFileCountReduction);
}

void CssCombineFilter::StartDocumentImpl() {
  context_.reset(MakeContext());
}

void CssCombineFilter::StartElementImpl(HtmlElement* element) {
  HtmlElement::Attribute* href;
  const char* media;
  if (!driver_->HasChildrenInFlushWindow(element) &&
      css_tag_scanner_.ParseCssElement(element, &href, &media)) {
    // We cannot combine with a link in <noscript> tag and we cannot combine
    // over a link in a <noscript> tag, so this is a barrier.
    if (noscript_element() != NULL) {
      NextCombination();
    } else {
      if (context_->new_combination()) {
        context_->SetMedia(media);
      } else if (combiner()->media() != media) {
        // After the first CSS file, subsequent CSS files must have matching
        // media.
        // TODO(jmarantz): do media='' and media='display mean the same
        // thing?  sligocki thinks mdsteele looked into this and it
        // depended on HTML version.  In one display was default, in the
        // other screen was IIRC.
        NextCombination();
        context_->SetMedia(media);
      }
      if (!context_->AddElement(element, href)) {
        NextCombination();
      }
    }
  } else if (element->keyword() == HtmlName::kStyle) {
    // We can't reorder styles on a page, so if we are only combining <link>
    // tags, we can't combine them across a <style> tag.
    // TODO(sligocki): Maybe we should just combine <style>s too?
    // We can run outline_css first for now to make all <style>s into <link>s.
    NextCombination();
  }
}

void CssCombineFilter::NextCombination() {
  if (driver_->asynchronous_rewrites()) {
    if (!context_->empty()) {
      driver_->InitiateRewrite(context_.release());
      context_.reset(MakeContext());
    }
  } else {
    combiner()->TryCombineAccumulated();
  }
  context_->Reset();
}

// An IE directive that includes any stylesheet info should be a barrier
// for css combining.  It's OK to emit the combination we've seen so far.
void CssCombineFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  // TODO(sligocki): Figure out how to safely parse IEDirectives, for now we
  // just consider them black boxes / solid barriers.
  NextCombination();
}

void CssCombineFilter::Flush() {
  NextCombination();
}

void CssCombineFilter::CssCombiner::TryCombineAccumulated() {
  if (CanRewrite()) {
    MessageHandler* handler = rewrite_driver_->message_handler();
    OutputResourcePtr combination(Combine(kContentTypeCss, handler));
    if (combination.get() != NULL) {
      // Ideally like to have a data-driven service tell us which elements
      // should be combined together.  Note that both the resources and the
      // elements are managed, so we don't delete them even if the spriting
      // fails.

      HtmlElement* combine_element =
          rewrite_driver_->NewElement(NULL, HtmlName::kLink);
      if (rewrite_driver_->doctype().IsXhtml()) {
        combine_element->set_close_style(HtmlElement::BRIEF_CLOSE);
      }
      rewrite_driver_->AddAttribute(
          combine_element, HtmlName::kRel, "stylesheet");
      rewrite_driver_->AddAttribute(combine_element, HtmlName::kType,
                                    kContentTypeCss.mime_type());
      rewrite_driver_->AddAttribute(combine_element, HtmlName::kHref,
                                    combination->url());
      if (!media_.empty()) {
        rewrite_driver_->AddAttribute(
            combine_element, HtmlName::kMedia, media_);
      }

      // TODO(sligocki): Put at top of head/flush-window.
      // Right now we're putting it where the first original element used to be.
      rewrite_driver_->InsertElementBeforeElement(element(0),
                                                  combine_element);
      // ... and removing originals from the DOM.
      for (int i = 0; i < num_urls(); ++i) {
        rewrite_driver_->DeleteElement(element(i));
      }
      rewrite_driver_->InfoHere("Combined %d CSS files into one at %s",
                                num_urls(), combination->url().c_str());
      if (css_file_count_reduction_ != NULL) {
        css_file_count_reduction_->Add(num_urls() - 1);
      }
    }
  }
  Reset();
}

// In addition to specifying the encoding in the ContentType header,
// one can also specify it at the beginning of the file using a Byte Order Mark.
//
// Bytes        Encoding Form
// 00 00 FE FF  UTF-32, big-endian
// FF FE 00 00  UTF-32, little-endian
// FE FF        UTF-16, big-endian
// FF FE        UTF-16, little-endian
// EF BB BF     UTF-8
// See: http://www.unicode.org/faq/utf_bom.html
// TODO(nforman): Possibly handle stripping BOMs from non-utf-8 files.
// We currently handle only utf-8 BOM because we assume the resources
// we get are not in utf-16 or utf-32 when we read and parse them, anyway.
// TODO(nforman): Figure out earlier on (and more rigorously) if a resource
// is encoded in one of these other formats and cache that fact so we
// don't continue to try to rewrite it.
void CssCombineFilter::CssCombiner::StripUTF8BOM(StringPiece* contents) const {
  if (contents->starts_with(kUtf8Bom)) {
    contents->remove_prefix(STATIC_STRLEN(kUtf8Bom));
  }
}

bool CssCombineFilter::CssCombiner::WritePiece(
    const Resource* input, OutputResource* combination, Writer* writer,
    MessageHandler* handler) {
  StringPiece contents = input->contents();
  GoogleUrl input_url(input->url());
  StringPiece input_dir = input_url.AllExceptLeaf();
  // Strip the BOM off of the contents (if it's there) if this is not the
  // first resource.
  if (resources().size() > 0 && resources()[0].get() != input) {
    StripUTF8BOM(&contents);
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

bool CssCombineFilter::Fetch(const OutputResourcePtr& resource,
                             Writer* writer,
                             const RequestHeaders& request_header,
                             ResponseHeaders* response_headers,
                             MessageHandler* message_handler,
                             UrlAsyncFetcher::Callback* callback) {
  DCHECK(!driver_->asynchronous_rewrites());
  context_.reset(MakeContext());
  return combiner()->Fetch(resource, writer, request_header, response_headers,
                           message_handler, callback);
}

CssCombineFilter::CssCombiner* CssCombineFilter::combiner() {
  return context_->combiner();
}

bool CssCombineFilter::HasAsyncFlow() const {
  return driver_->asynchronous_rewrites();
}

CssCombineFilter::Context* CssCombineFilter::MakeContext() {
  return new Context(driver_, &css_tag_scanner_, this);
}

RewriteContext* CssCombineFilter::MakeRewriteContext() {
  return MakeContext();
}

}  // namespace net_instaweb
