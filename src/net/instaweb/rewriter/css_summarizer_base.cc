/*
 * Copyright 2013 Google Inc.
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

#include "net/instaweb/rewriter/public/css_summarizer_base.h"

#include <cstddef>
#include <memory>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/css_inline_filter.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/data_url_input_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/charset_util.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "pagespeed/kernel/html/html_keywords.h"
#include "webutil/css/parser.h"

namespace net_instaweb {

class UrlSegmentEncoder;

namespace {

// A slot we use when rewriting inline CSS --- there is no place or need
// to write out an output URL, so it has a no-op Render().
// TODO(morlovich): Dupe'd from CssFilter; refactor?
class InlineCssSummarizerSlot : public ResourceSlot {
 public:
  InlineCssSummarizerSlot(HtmlElement* element,
                          const ResourcePtr& resource,
                          const GoogleString& location)
      : ResourceSlot(resource), element_(element), location_(location) {}
  virtual ~InlineCssSummarizerSlot() {}
  virtual HtmlElement* element() const { return element_; }
  virtual void Render() {}
  virtual GoogleString LocationString() { return location_; }

 private:
  HtmlElement* element_;
  GoogleString location_;
  DISALLOW_COPY_AND_ASSIGN(InlineCssSummarizerSlot);
};

}  // namespace

// Rewrite context for CssSummarizerBase --- it invokes the filter's
// summarization functions on parsed CSS ASTs when available, and synchronizes
// them with the summaries_ table in the CssSummarizerBase.
class CssSummarizerBase::Context : public SingleRewriteContext {
 public:
  // pos denotes our position in the filters' summaries_ vector.
  Context(int pos, CssSummarizerBase* filter, RewriteDriver* driver);
  virtual ~Context();

  // Calls to finish initialization for given rewrite type; should be called
  // soon after construction.
  void SetupInlineRewrite(HtmlElement* element, HtmlCharactersNode* text);
  void SetupExternalRewrite(HtmlElement* element);

 protected:
  virtual void Render();
  virtual void WillNotRender();
  virtual void Cancel();
  virtual bool Partition(OutputPartitions* partitions,
                         OutputResourceVector* outputs);
  virtual void RewriteSingle(const ResourcePtr& input,
                             const OutputResourcePtr& output);
  virtual const char* id() const { return filter_->id(); }
  virtual OutputResourceKind kind() const { return kRewrittenResource; }
  virtual GoogleString CacheKeySuffix() const;
  virtual const UrlSegmentEncoder* encoder() const {
    return filter_->encoder();
  }

 private:
  // Reports completion of one summary (including failures).
  void ReportDone();

  int pos_;  // our position in the list of all styles in the page.
  CssSummarizerBase* filter_;

  HtmlElement* element_;
  HtmlCharactersNode* text_;

  // True if we're rewriting a <style> block, false if it's a <link>
  bool rewrite_inline_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

CssSummarizerBase::Context::Context(int pos,
                                    CssSummarizerBase* filter,
                                    RewriteDriver* driver)
    : SingleRewriteContext(driver, NULL /*parent*/, NULL /* resource_context*/),
      pos_(pos),
      filter_(filter),
      element_(NULL),
      text_(NULL),
      rewrite_inline_(false) {
}

CssSummarizerBase::Context::~Context() {
}

void CssSummarizerBase::Context::SetupInlineRewrite(HtmlElement* element,
                                                    HtmlCharactersNode* text) {
  rewrite_inline_ = true;
  element_ = element;
  text_ = text;
}

void CssSummarizerBase::Context::SetupExternalRewrite(HtmlElement* element) {
  rewrite_inline_ = false;
  element_ = element;
  text_ = NULL;
}

void CssSummarizerBase::Context::ReportDone() {
  bool should_report_all_done = false;
  {
    ScopedMutex hold(filter_->progress_lock_.get());
    --filter_->outstanding_rewrites_;
    if (filter_->saw_end_of_document_ &&
        (filter_->outstanding_rewrites_ == 0)) {
      should_report_all_done = true;
    }
  }
  if (should_report_all_done) {
    filter_->ReportSummariesDone();
  }
}

void CssSummarizerBase::Context::Render() {
  DCHECK_LE(0, pos_);
  DCHECK_LT(static_cast<size_t>(pos_), filter_->summaries_.size());
  SummaryInfo& summary_info = filter_->summaries_[pos_];
  bool is_element_deleted = false;
  if (num_output_partitions() == 0) {
    // Failed at partition -> resource fetch failed or uncacheable.
    summary_info.state = kSummaryInputUnavailable;
    filter_->WillNotRenderSummary(pos_, element_, text_, &is_element_deleted);
  } else {
    const CachedResult& result = *output_partition(0);
    // Transfer the summarization result from the metadata cache (where it was
    // stored by RewriteSingle) to the summary table;  we have to do it here
    // so it's available on a cache hit. Conveniently this will also never race
    // with the HTML thread, so the summary accessors will be safe to access
    // off parser events.
    if (result.has_inlined_data()) {
      summary_info.state = kSummaryOk;
      summary_info.data = result.inlined_data();
      // For external resources, fix up base to refer to the current URL in
      // the slot, as it may have been changed by an earlier filter.
      if (summary_info.is_external) {
        summary_info.base = slot(0)->resource()->url();
      }
      filter_->RenderSummary(pos_, element_, text_, &is_element_deleted);
    } else {
      summary_info.state = kSummaryCssParseError;
      filter_->WillNotRenderSummary(pos_, element_, text_, &is_element_deleted);
    }
  }
  if (is_element_deleted) {
    slot(0)->set_disable_further_processing(true);
  }
  ReportDone();
}

void CssSummarizerBase::Context::WillNotRender() {
  bool is_element_deleted = false;
  filter_->WillNotRenderSummary(pos_, element_, text_, &is_element_deleted);
  if (is_element_deleted) {
    slot(0)->set_disable_further_processing(true);
  }
}

void CssSummarizerBase::Context::Cancel() {
  ScopedMutex hold(filter_->progress_lock_.get());
  filter_->canceled_summaries_.push_back(pos_);
}

void CssSummarizerBase::Context::RewriteSingle(
    const ResourcePtr& input_resource,
    const OutputResourcePtr& output_resource) {
  StringPiece input_contents = input_resource->contents();

  // TODO(morlovich): Should we keep track of this so it can be restored?
  StripUtf8Bom(&input_contents);

  // Load stylesheet w/o expanding background attributes and preserving as
  // much content as possible from the original document.
  Css::Parser parser(input_contents);
  parser.set_preservation_mode(true);

  // We avoid quirks-mode so that we do not "fix" something we shouldn't have.
  parser.set_quirks_mode(false);

  scoped_ptr<Css::Stylesheet> stylesheet(parser.ParseRawStylesheet());
  CachedResult* result = output_partition(0);
  if (stylesheet.get() == NULL ||
      parser.errors_seen_mask() != Css::Parser::kNoError) {
    // TODO(morlovich): do we want a stat here?
    result->clear_inlined_data();
  } else {
    filter_->Summarize(stylesheet.get(), result->mutable_inlined_data());
  }
  if (CssInlineFilter::HasClosingStyleTag(result->inlined_data())) {
    result->clear_inlined_data();
  }

  // We never produce output --- just write to the CachedResult; so we
  // technically fail.
  RewriteDone(kRewriteFailed, 0);
}

bool CssSummarizerBase::Context::Partition(OutputPartitions* partitions,
                                           OutputResourceVector* outputs) {
  if (num_slots() != 1) {
    return false;
  }
  ResourcePtr resource(slot(0)->resource());
  if (!rewrite_inline_ && !resource->IsSafeToRewrite(rewrite_uncacheable())) {
    // TODO(anupama): Shouldn't we check the closing style tag portion of
    // ShouldInline(resource) here?
    return false;
  }
  // We don't want an output resource but still want a non-trivial partition.
  // We use kOmitInputHash here as this is for content that will be inlined.
  CachedResult* partition = partitions->add_partition();
  resource->AddInputInfoToPartition(Resource::kOmitInputHash, 0, partition);
  outputs->push_back(OutputResourcePtr(NULL));
  return true;
}

GoogleString CssSummarizerBase::Context::CacheKeySuffix() const {
  return filter_->CacheKeySuffix();
}

const char CssSummarizerBase::kNumCssUsedForCriticalCssComputation[] =
    "num_css_used_for_critical_css_computation";
const char CssSummarizerBase::kNumCssNotUsedForCriticalCssComputation[] =
    "num_css_not_used_for_critical_css_computation";

CssSummarizerBase::CssSummarizerBase(RewriteDriver* driver)
    : RewriteFilter(driver),
      progress_lock_(driver->server_context()->thread_system()->NewMutex()) {
  Statistics* stats = server_context()->statistics();
  num_css_used_for_critical_css_computation_ =
      stats->GetVariable(kNumCssUsedForCriticalCssComputation);
  num_css_not_used_for_critical_css_computation_ =
      stats->GetVariable(kNumCssNotUsedForCriticalCssComputation);
  Clear();
}

CssSummarizerBase::~CssSummarizerBase() {
  Clear();
}

void CssSummarizerBase::InitStats(Statistics* statistics) {
  statistics->AddVariable(kNumCssUsedForCriticalCssComputation);
  statistics->AddVariable(kNumCssNotUsedForCriticalCssComputation);
}

GoogleString CssSummarizerBase::CacheKeySuffix() const {
  return GoogleString();
}

void CssSummarizerBase::SummariesDone() {
}

void CssSummarizerBase::RenderSummary(
    int pos, HtmlElement* element, HtmlCharactersNode* char_node,
    bool* is_element_deleted) {
}

void CssSummarizerBase::WillNotRenderSummary(
    int pos, HtmlElement* element, HtmlCharactersNode* char_node,
    bool* is_element_deleted) {
}

void CssSummarizerBase::Clear() {
  outstanding_rewrites_ = 0;
  saw_end_of_document_ = false;
  style_element_ = NULL;
  summaries_.clear();
  canceled_summaries_.clear();
}

void CssSummarizerBase::StartDocumentImpl() {
  // TODO(morlovich): we hold on to the summaries_ memory too long; refine this
  // once the data type is refined.
  DCHECK(canceled_summaries_.empty());
  Clear();
}

void CssSummarizerBase::EndDocument() {
  bool should_report_all_done = false;
  {
    ScopedMutex hold(progress_lock_.get());
    saw_end_of_document_ = true;
    if (outstanding_rewrites_ == 0) {
      // All done before it even got to us!
      should_report_all_done = true;
    }
  }

  if (should_report_all_done) {
    ReportSummariesDone();
  }
}

void CssSummarizerBase::StartElementImpl(HtmlElement* element) {
  // HtmlParse should not pass us elements inside a style element.
  CHECK(style_element_ == NULL);
  if (element->keyword() == HtmlName::kStyle &&
      element->FindAttribute(HtmlName::kScoped) == NULL) {
    style_element_ = element;
  }
  // We deal with <link> elements in EndElement.
  // We ignore scoped style elements, as they are already inlined,
  // can't safely be moved, and take precedence in cascade order
  // regardless of their position relative to non-scoped CSS.
}

void CssSummarizerBase::Characters(HtmlCharactersNode* characters_node) {
  CommonFilter::Characters(characters_node);
  if (style_element_ != NULL) {
    // Note: HtmlParse should guarantee that we only get one CharactersNode
    // per <style> block even if it is split by a flush.
    if (MustSummarize(style_element_)) {
      StartInlineRewrite(style_element_, characters_node);
    }
  }
}

void CssSummarizerBase::EndElementImpl(HtmlElement* element) {
  if (style_element_ != NULL) {
    // End of an inline style.
    CHECK_EQ(style_element_, element);  // HtmlParse should not pass unmatching.
    style_element_ = NULL;
    return;
  }
  if (element->keyword() == HtmlName::kLink) {
    // Rewrite an external style.
    StringPiece rel = element->AttributeValue(HtmlName::kRel);
    if (CssTagScanner::IsStylesheetOrAlternate(rel)) {
      HtmlElement::Attribute* element_href = element->FindAttribute(
          HtmlName::kHref);
      if (element_href != NULL) {
        // If it has a href= attribute
        if (MustSummarize(element)) {
          StartExternalRewrite(element, element_href, rel);
        }
      }
    }
  }
}

void CssSummarizerBase::RenderDone() {
  bool should_report_all_done = false;

  {
    ScopedMutex hold(progress_lock_.get());
    // Transfer from canceled_summaries_ to summaries_.
    for (int i = 0, n = canceled_summaries_.size(); i < n; ++i) {
      int pos = canceled_summaries_[i];
      summaries_[pos].state = kSummarySlotRemoved;
    }

    if (!canceled_summaries_.empty()) {
      outstanding_rewrites_ -= canceled_summaries_.size();
      if (outstanding_rewrites_ == 0) {
        should_report_all_done = saw_end_of_document_;
      }
    }
    canceled_summaries_.clear();
  }

  if (should_report_all_done) {
    ReportSummariesDone();
  }
}

void CssSummarizerBase::ReportSummariesDone() {
  if (DebugMode()) {
    GoogleString comment = "Summary computation status for ";
    StrAppend(&comment, Name(), "\n");
    for (int i = 0, n = summaries_.size(); i < n; ++i) {
      StrAppend(&comment, "Resource ", IntegerToString(i),
                " ", summaries_[i].location, ": ");
      switch (summaries_[i].state) {
        case kSummaryOk:
          StrAppend(&comment, "Computed OK\n");
          break;
        case kSummaryStillPending:
          StrAppend(&comment, "Computation still pending\n");
          break;
        case kSummaryCssParseError:
          StrAppend(&comment, "Unrecoverable CSS parse error or resource "
                              "contains closing style tag\n");
          break;
        case kSummaryResourceCreationFailed:
          StrAppend(&comment, kCreateResourceFailedDebugMsg, "\n");
          break;
        case kSummaryInputUnavailable:
          StrAppend(&comment,
                    "Fetch failed or resource not publicly cacheable\n");
          break;
        case kSummarySlotRemoved:
          StrAppend(&comment,
                    "Resource removed by another filter\n");
          break;
      }
    }
    GoogleString escaped;
    HtmlKeywords::Escape(comment, &escaped);
    InsertNodeAtBodyEnd(driver()->NewCommentNode(NULL, escaped));
  }
  for (int i = 0, n = summaries_.size(); i < n; ++i) {
    if (summaries_[i].state == kSummaryOk) {
      num_css_used_for_critical_css_computation_->Add(1);
    } else {
      num_css_not_used_for_critical_css_computation_->Add(1);
    }
  }
  SummariesDone();
}

void CssSummarizerBase::StartInlineRewrite(
    HtmlElement* style, HtmlCharactersNode* text) {
  ResourceSlotPtr slot(MakeSlotForInlineCss(style, text->contents()));
  Context* context =
      CreateContextAndSummaryInfo(style, false /* not external */,
                                  slot, slot->LocationString(),
                                  driver()->decoded_base(),
                                  StringPiece() /* rel, none since inline */);
  context->SetupInlineRewrite(style, text);
  driver()->InitiateRewrite(context);
}

void CssSummarizerBase::StartExternalRewrite(
    HtmlElement* link, HtmlElement::Attribute* src, StringPiece rel) {
  // Create the input resource for the slot.
  bool is_authorized;
  ResourcePtr input_resource(CreateInputResource(src->DecodedValueOrNull(),
                                                 &is_authorized));
  if (input_resource.get() == NULL) {
    // Record a failure, so the subclass knows of it.
    summaries_.push_back(SummaryInfo());
    summaries_.back().state = kSummaryResourceCreationFailed;
    const char* url = src->DecodedValueOrNull();
    summaries_.back().location = (url != NULL ? url : driver()->UrlLine());

    bool is_element_deleted = false;  // unused after call because no slot here
    WillNotRenderSummary(summaries_.size() - 1, link, NULL /* char_node */,
                         &is_element_deleted);

    // TODO(morlovich): Stat?
    if (DebugMode()) {
      if (is_authorized || url == NULL) {
        driver()->InsertComment(StrCat(
            Name(), ": ", kCreateResourceFailedDebugMsg));
      } else {
        // Do not write a debug message in this case because that has already
        // been done by the CSS rewriting filter.
      }
    }
    return;
  }
  ResourceSlotPtr slot(driver()->GetSlot(input_resource, link, src));
  Context* context = CreateContextAndSummaryInfo(
      link, true /* external*/, slot, input_resource->url() /* location*/,
      input_resource->url() /* base */, rel);
  context->SetupExternalRewrite(link);
  driver()->InitiateRewrite(context);
}

ResourceSlot* CssSummarizerBase::MakeSlotForInlineCss(
    HtmlElement* element, const StringPiece& content) {
  // Create the input resource for the slot.
  GoogleString data_url;
  // TODO(morlovich): This does a lot of useless conversions and
  // copying. Get rid of them.
  DataUrl(kContentTypeCss, PLAIN, content, &data_url);
  ResourcePtr input_resource(DataUrlInputResource::Make(data_url, driver()));
  return new InlineCssSummarizerSlot(
      element, input_resource, driver()->UrlLine());
}

CssSummarizerBase::Context* CssSummarizerBase::CreateContextAndSummaryInfo(
    const HtmlElement* element, bool external, const ResourceSlotPtr& slot,
    const GoogleString& location, StringPiece base_for_resources,
    StringPiece rel) {
  int id = summaries_.size();
  summaries_.push_back(SummaryInfo());
  SummaryInfo& new_summary = summaries_.back();
  new_summary.location = location;
  base_for_resources.CopyToString(&new_summary.base);
  const HtmlElement::Attribute* media_attribute =
        element->FindAttribute(HtmlName::kMedia);
  if (media_attribute != NULL &&
      media_attribute->DecodedValueOrNull() != NULL) {
    new_summary.media_from_html = media_attribute->DecodedValueOrNull();
  }
  rel.CopyToString(&new_summary.rel);
  new_summary.is_external = external;
  new_summary.is_inside_noscript = (noscript_element() != NULL);

  ++outstanding_rewrites_;

  Context* context = new Context(id, this, driver());
  context->AddSlot(slot);
  return context;
}

RewriteContext* CssSummarizerBase::MakeRewriteContext() {
  // We should not be registered under our id as a rewrite filter, since we
  // don't expect to answer fetches.
  LOG(DFATAL) << "CssSummarizerBase subclasses should not be registered "
                 "as handling fetches";
  return NULL;
}

}  // namespace net_instaweb
