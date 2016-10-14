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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_SUMMARIZER_BASE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_SUMMARIZER_BASE_H_

#include <vector>

#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"

namespace Css {

class Stylesheet;

}  // namespace Css

namespace net_instaweb {

class AbstractMutex;
class HtmlCharactersNode;
class RewriteContext;
class RewriteDriver;
class Statistics;
class Variable;

// This class helps implement filters that try to compute some properties of all
// the screen-affecting CSS in the page except for scoped <style> blocks (which
// are left untouched). They are expected to override Summarize() to perform the
// per-CSS computation; then at SummariesDone() they can lookup summaries via
// NumStyles/GetSummaryForStyle.
class CssSummarizerBase : public RewriteFilter {
 public:
  static const char kNumCssUsedForCriticalCssComputation[];
  static const char kNumCssNotUsedForCriticalCssComputation[];

  explicit CssSummarizerBase(RewriteDriver* driver);
  virtual ~CssSummarizerBase();

  static void InitStats(Statistics* statistics);

 protected:
  enum SummaryState {
    // All OK!
    kSummaryOk,

    // Computation/Fetches ongoing, we don't have a result yet.
    kSummaryStillPending,

    // CSS parse error we can't recover from.
    kSummaryCssParseError,

    // Could not create the resource object, so its URL is malformed or we do
    // not have permission to rewrite it.
    kSummaryResourceCreationFailed,

    // Fetch result unusable, either error or not cacheable.
    kSummaryInputUnavailable,

    // Slot got removed by an another optimization.
    kSummarySlotRemoved,
  };

  struct SummaryInfo {
    SummaryInfo()
        : state(kSummaryStillPending),
          is_external(false),
          is_inside_noscript(false) {}

    // data actually computed by the subclass's Summarize method. Make sure to
    // check state == kSummaryOk before using it.
    GoogleString data;

    // State of computation of 'data'.
    SummaryState state;

    // Human-readable description of the location of the CSS. For use in debug
    // messages.
    GoogleString location;

    // Base to use for resolving links in the CSS resource.
    GoogleString base;

    // CSS media there were applied to the resource by the HTML.
    GoogleString media_from_html;

    // If it's an external stylesheet, the value of the rel attribute
    GoogleString rel;

    // True if it's a <link rel=stylesheet href=>, false for <style>
    bool is_external;

    // True if the style was included inside a noscript section.
    bool is_inside_noscript;
  };

  // This method should be overridden in case the subclass's summary computation
  // depends on things other than the input CSS.
  virtual GoogleString CacheKeySuffix() const;

  // This method should be overridden if some CSS should not go through the
  // summarization process (eg because it uses an inapplicable media type and
  // we'll just throw it away when we're done anyway).  By default all CSS
  // must be summarized.
  virtual bool MustSummarize(HtmlElement* element) const {
    return true;
  }

  // This should be overridden to compute a per-resource summary.
  // The method should not modify the object state, and only
  // put the result into *out as it may not be invoked in case of a
  // cache hit. The subclass may mutate *stylesheet if it so wishes.
  //
  // Note: this is called on a rewrite thread, so it should not access
  // HTML parser state.
  virtual void Summarize(Css::Stylesheet* stylesheet,
                         GoogleString* out) const = 0;

  // This can be optionally overridden to modify a CSS element based on a
  // successfully computed summary. It might not be invoked if cached
  // information is not readily available, and will not be invoked if CSS
  // parsing failed or some other error occurred. Invocation occurs from a
  // thread with HTML parser context state, so both DOM modification and
  // GetSummaryForStyle() are safe to use. If invoked, the method will be called
  // before SummariesDone().
  //
  // pos is the position of the element in the summary table.
  //
  // element points to the <link> or <style> element that was summarized.
  // If the element was a <style>, char_node will also point to its contents
  // node; otherwise it will be NULL. Overrides need to set is_element_deleted
  // to true if they delete the element.
  //
  // The default implementation does nothing.
  virtual void RenderSummary(int pos,
                             HtmlElement* element,
                             HtmlCharactersNode* char_node,
                             bool* is_element_deleted);

  // Like RenderSummary, but called in cases where we're unable to render a
  // summary for some reason (including not being able to compute one).
  // Note: not called when we're canceled due to disable_further_processing.
  //
  // Like with RenderSummary, this corresponds to entry [pos] in the summary
  // table, and elements points to the <link> or <style> containing CSS,
  // with char_node being non-null in case it was a <style>.
  virtual void WillNotRenderSummary(int pos,
                                    HtmlElement* element,
                                    HtmlCharactersNode* char_node);

  // This is called at the end of the document when all outstanding summary
  // computations have completed, regardless of whether successful or not. It
  // will not be called at all if they are still ongoing, however.
  //
  // It's called from a context which allows HTML parser state access.  You can
  // insert things at end of document by constructing an HtmlNode* using the
  // factories in HtmlParse and calling CommonFilter::InsertNodeAtBodyEnd(node).
  //
  // Note that the timing of this can vary widely --- it can occur during
  // initial parse, during the render phase, or even at RenderDone, so
  // implementors should not make assumptions about what other filters may have
  // done to the DOM.
  //
  // Base version does nothing.
  virtual void SummariesDone();

  // Returns total number of <link> and <style> elements we encountered.
  // This includes those for which we had problem computing summary information.
  //
  // Should be called from a thread context that has HTML parser state access.
  int NumStyles() const { return static_cast<int>(summaries_.size()); }

  // Returns summary computed for the pos'th style in the document.
  //
  // pos must be in [0, NumStyles())
  //
  // Should be called from a thread context that has HTML parser state access.
  const SummaryInfo& GetSummaryForStyle(int pos) const {
    return summaries_.at(pos);
  }

  // Overrides of the filter APIs. You MUST call through to this class's
  // implementations if you override them.
  virtual void StartDocumentImpl();
  virtual void EndDocument();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual void EndElementImpl(HtmlElement* element);
  virtual void RenderDone();

  virtual RewriteContext* MakeRewriteContext();

 private:
  class Context;

  // Clean out private data.
  void Clear();

  // Invokes SummariesDone and, if the debug filter is on, injects a comment
  // describing what happened with every CSS resource.
  void ReportSummariesDone();

  // Starts the asynchronous rewrite process for inline CSS 'text', contained
  // within the style element 'style'.
  void StartInlineRewrite(HtmlElement* style, HtmlCharactersNode* text);

  // Starts the asynchronous rewrite process for external CSS referenced by
  // attribute 'src' of 'link', whose rel attribute is 'rel'.
  void StartExternalRewrite(HtmlElement* link, HtmlElement::Attribute* src,
                            StringPiece rel);

  // Creates our rewrite context for the given slot and registers it
  // with the summaries_ vector, filling in the SummaryInfo struct in
  // a pending state.  The context will still need to have SetupInlineRewrite
  // or SetupExternalRewrite and InitiateRewrite called on it.
  // location is used to identify the resource in debug comments.
  Context* CreateContextAndSummaryInfo(const HtmlElement* element,
                                       bool external,
                                       const ResourceSlotPtr& slot,
                                       const GoogleString& location,
                                       StringPiece base_for_resources,
                                       StringPiece rel);

  ResourceSlotPtr MakeSlotForInlineCss(HtmlCharactersNode* char_node);

  // Stores all the computed summaries.
  std::vector<SummaryInfo> summaries_;

  scoped_ptr<AbstractMutex> progress_lock_;
  int outstanding_rewrites_;  // guarded by progress_lock_
  bool saw_end_of_document_;  // guarded by progress_lock_
  // Lists indexes into summaries_ vector that got canceled due to
  // disable_further_processing. It's written to in the Rewrite thread,
  // and then pulled into summaries_ from an HTML thread.
  std::vector<int> canceled_summaries_;  // guarded by progress_lock_

  HtmlElement* style_element_;  // The element we are in, or NULL.

  Variable* num_css_used_for_critical_css_computation_;
  Variable* num_css_not_used_for_critical_css_computation_;

  DISALLOW_COPY_AND_ASSIGN(CssSummarizerBase);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_SUMMARIZER_BASE_H_
