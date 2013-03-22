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

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
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
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "webutil/css/parser.h"

namespace net_instaweb {

class UrlSegmentEncoder;

namespace {

// A slot we use when rewriting inline CSS --- there is no place or need
// to write out an output URL, so it has a no-op Render().
// TODO(morlovich): Dupe'd from CssFilter; refactor?
class InlineCssSlot : public ResourceSlot {
 public:
  InlineCssSlot(const ResourcePtr& resource, const GoogleString& location)
      : ResourceSlot(resource), location_(location) {}
  virtual ~InlineCssSlot() {}
  virtual void Render() {}
  virtual GoogleString LocationString() { return location_; }

 private:
  GoogleString location_;
  DISALLOW_COPY_AND_ASSIGN(InlineCssSlot);
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
  void SetupInlineRewrite(HtmlElement* style_element, HtmlCharactersNode* text);
  void SetupExternalRewrite(const GoogleUrl& base_gurl);

 protected:
  virtual void Render();
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

  // Base URL against which CSS in here is resolved.
  GoogleUrl css_base_gurl_;

  // Style element containing inline CSS (see StartInlineRewrite)
  HtmlElement* rewrite_inline_element_;

  // Node with inline CSS to rewrite, or NULL if we're rewriting external stuff.
  HtmlCharactersNode* rewrite_inline_char_node_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

CssSummarizerBase::Context::Context(int pos,
                                    CssSummarizerBase* filter,
                                    RewriteDriver* driver)
    : SingleRewriteContext(driver, NULL /*parent*/, NULL /* resource_context*/),
      pos_(pos),
      filter_(filter),
      rewrite_inline_element_(NULL),
      rewrite_inline_char_node_(NULL) {
}

CssSummarizerBase::Context::~Context() {
}

void CssSummarizerBase::InjectSummaryData(HtmlNode* data) {
  if (injection_point_ != NULL && driver()->IsRewritable(injection_point_)) {
    driver()->AppendChild(injection_point_, data);
  } else {
    driver()->InsertElementBeforeCurrent(data);
  }
}

void CssSummarizerBase::Context::SetupInlineRewrite(
    HtmlElement* style_element, HtmlCharactersNode* text) {
  // To handle nested rewrites of inline CSS, we internally handle it
  // as a rewrite of a data: URL.
  css_base_gurl_.Reset(filter_->decoded_base_url());
  DCHECK(css_base_gurl_.is_valid());
  rewrite_inline_element_ = style_element;
  rewrite_inline_char_node_ = text;
}

void CssSummarizerBase::Context::SetupExternalRewrite(
    const GoogleUrl& base_gurl) {
  css_base_gurl_.Reset(base_gurl);
  rewrite_inline_element_ = NULL;
  rewrite_inline_char_node_ = NULL;
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
  if (num_output_partitions() == 0) {
    // Failed at partition -> resource fetch failed or uncacheable.
    summary_info.state = kSummaryInputUnavailable;
  } else {
    const CachedResult& result = *output_partition(0);
    // Transfer the result out to the summary table; we have to do it here
    // so it's available on the cache hit. Conveniently this will also never
    // race with the HTML thread, so the summary accessors will be safe to
    // access off parser events.
    if (result.has_inlined_data()) {
      summary_info.state = kSummaryOk;
      summary_info.data = result.inlined_data();
    } else {
      summary_info.state = kSummaryCssParseError;
    }
  }

  ReportDone();
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
    filter_->Summarize(*stylesheet, result->mutable_inlined_data());
  }

  // We never produce output --- just write to the CachedResult; so we
  // technically fail.
  RewriteDone(kRewriteFailed, 0);
}

bool CssSummarizerBase::Context::Partition(OutputPartitions* partitions,
                                           OutputResourceVector* outputs) {
  bool ok;
  if (rewrite_inline_element_ == NULL) {
    ok = SingleRewriteContext::Partition(partitions, outputs);
    ok = ok && (partitions->partition_size() != 0);
  } else {
    // In the case where we're rewriting inline CSS, we don't want an output
    // resource but still want a non-trivial partition.
    // We use kOmitInputHash here as this is for inline content.
    CachedResult* partition = partitions->add_partition();
    slot(0)->resource()->AddInputInfoToPartition(
        Resource::kOmitInputHash, 0, partition);
    outputs->push_back(OutputResourcePtr(NULL));
    ok = true;
  }

  return ok;
}

GoogleString CssSummarizerBase::Context::CacheKeySuffix() const {
  GoogleString suffix;
  if (rewrite_inline_element_ != NULL) {
    // Incorporate the base path of the HTML as part of the key --- it
    // matters for inline CSS since resources are resolved against
    // that (while it doesn't for external CSS, since that uses the
    // stylesheet as the base).
    // TODO(morlovich): this doesn't actually matter for what we use this for,
    // though?
    const Hasher* hasher = FindServerContext()->lock_hasher();
    StrAppend(&suffix, "_@", hasher->Hash(css_base_gurl_.AllExceptLeaf()));
  }

  return suffix;
}

CssSummarizerBase::CssSummarizerBase(RewriteDriver* driver)
    : RewriteFilter(driver),
      progress_lock_(driver->server_context()->thread_system()->NewMutex()) {
  Clear();
}

CssSummarizerBase::~CssSummarizerBase() {
  Clear();
}

void CssSummarizerBase::Clear() {
  outstanding_rewrites_ = 0;
  saw_end_of_document_ = false;
  style_element_ = NULL;
  injection_point_ = NULL;
  summaries_.clear();
}

void CssSummarizerBase::StartDocumentImpl() {
  // TODO(morlovich): we hold on to the summaries_ memory too long; refine this
  // once the data type is refined.
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
  if (element->keyword() == HtmlName::kStyle) {
    style_element_ = element;
  }
  // We deal with <link> elements in EndElement.
}

void CssSummarizerBase::Characters(HtmlCharactersNode* characters_node) {
  if (style_element_ != NULL) {
    // Note: HtmlParse should guarantee that we only get one CharactersNode
    // per <style> block even if it is split by a flush. However, this code
    // will still mostly work if we somehow got multiple CharacterNodes.
    // TODO(morlovich): Validate media
    injection_point_ = NULL;
    StartInlineRewrite(characters_node);
  } else if (injection_point_ != NULL &&
             !OnlyWhitespace(characters_node->contents())) {
    // Ignore whitespace between </body> and </html> or after </html> when
    // deciding whether </body> is a safe injection point.  Otherwise, there's
    // content after the injection_point_ and we should inject at end of
    // document instead.
    injection_point_ = NULL;
  }
}

void CssSummarizerBase::EndElementImpl(HtmlElement* element) {
  if (style_element_ != NULL) {
    // End of an inline style.
    CHECK_EQ(style_element_, element);  // HtmlParse should not pass unmatching.
    style_element_ = NULL;
    return;
  }
  switch (element->keyword()) {
    case HtmlName::kLink: {
      // Rewrite an external style.
      // TODO(morlovich): Validate media
      // TODO(morlovich): This is wrong with alternate; current
      //     CssTagScanner is wrong with title=
      injection_point_ = NULL;
      if (CssTagScanner::IsStylesheetOrAlternate(
              element->AttributeValue(HtmlName::kRel))) {
        HtmlElement::Attribute* element_href = element->FindAttribute(
            HtmlName::kHref);
        if (element_href != NULL) {
          // If it has a href= attribute
          StartExternalRewrite(element, element_href);
        }
      }
      break;
    }
    case HtmlName::kBody:
      // Preferred injection location
      injection_point_ = element;
      break;
    case HtmlName::kHtml:
      if ((injection_point_ == NULL ||
           !driver()->IsRewritable(injection_point_)) &&
          driver()->IsRewritable(element)) {
        // Try to inject before </html> if before </body> won't work.
        injection_point_ = element;
      }
      break;
    default:
      // There were (possibly implicit) close tags after </body> or </html>, so
      // throw that point away.
      injection_point_ = NULL;
      break;
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
          StrAppend(&comment, "Unrecoverable CSS parse error\n");
          break;
        case kSummaryResourceCreationFailed:
          StrAppend(&comment, "Cannot create resource; is it authorized and "
                              "is URL well-formed?\n");
          break;
        case kSummaryInputUnavailable:
          StrAppend(&comment,
                    "Fetch failed or resource not publicly cacheable\n");
          break;
      }
    }
    InjectSummaryData(driver()->NewCommentNode(NULL, comment));
  }

  SummariesDone();
}

void CssSummarizerBase::StartInlineRewrite(HtmlCharactersNode* text) {
  // TODO(sligocki): Clean this up to not need to pass parent around explicitly.
  // The few places that actually need to know the parent can call
  // text->parent() themselves.
  HtmlElement* element = text->parent();
  ResourceSlotPtr slot(MakeSlotForInlineCss(text->contents()));
  Context* context = CreateContextForSlot(slot, slot->LocationString());
  context->SetupInlineRewrite(element, text);
  driver_->InitiateRewrite(context);
}

void CssSummarizerBase::StartExternalRewrite(
    HtmlElement* link, HtmlElement::Attribute* src) {
  // Create the input resource for the slot.
  ResourcePtr input_resource(CreateInputResource(src->DecodedValueOrNull()));
  if (input_resource.get() == NULL) {
    // Record a failure, so the subclass knows of it.
    summaries_.push_back(SummaryInfo());
    summaries_.back().state = kSummaryResourceCreationFailed;
    const char* url = src->DecodedValueOrNull();
    summaries_.back().location = (url != NULL ? url : driver_->UrlLine());

    // TODO(morlovich): Stat?
    if (DebugMode()) {
      driver_->InsertComment(StrCat(
          Name(), ": unable to create resource; is it authorized?"));
    }
    return;
  }
  ResourceSlotPtr slot(driver_->GetSlot(input_resource, link, src));
  Context* context = CreateContextForSlot(slot, input_resource->url());
  GoogleUrl input_resource_gurl(input_resource->url());
  context->SetupExternalRewrite(input_resource_gurl);
  driver_->InitiateRewrite(context);
}

ResourceSlot* CssSummarizerBase::MakeSlotForInlineCss(
    const StringPiece& content) {
  // Create the input resource for the slot.
  GoogleString data_url;
  // TODO(morlovich): This does a lot of useless conversions and
  // copying. Get rid of them.
  DataUrl(kContentTypeCss, PLAIN, content, &data_url);
  ResourcePtr input_resource(DataUrlInputResource::Make(data_url,
                                                        server_context_));
  return new InlineCssSlot(input_resource, driver_->UrlLine());
}

CssSummarizerBase::Context* CssSummarizerBase::CreateContextForSlot(
    const ResourceSlotPtr& slot, const GoogleString& location) {
  int id = summaries_.size();
  summaries_.push_back(SummaryInfo());
  summaries_.back().location = location;
  ++outstanding_rewrites_;

  Context* context = new Context(id, this, driver_);
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
