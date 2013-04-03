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

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace Css {

class Stylesheet;

}  // namespace Css

namespace net_instaweb {

class AbstractMutex;
class HtmlCharactersNode;
class HtmlNode;
class RewriteContext;
class RewriteDriver;

// This class helps implement filters that try to compute some properties of
// all the screen-affecting CSS in the page. They are expected to override
// Summarize() to perform the per-CSS computation; then at SummariesDone() they
// can lookup summaries via NumStyles/GetSummaryForStyle.
class CssSummarizerBase : public RewriteFilter {
 public:
  explicit CssSummarizerBase(RewriteDriver* driver);
  virtual ~CssSummarizerBase();

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
    kSummaryInputUnavailable
  };

  struct SummaryInfo {
    SummaryInfo() : state(kSummaryStillPending) {}

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

    // True if it's a <link rel=stylesheet href=>, false for <style>
    bool is_external;
  };

  // This method should be overridden in case the subclass's summary computation
  // depends on things other than the input CSS.
  virtual GoogleString CacheKeySuffix() const;

  // This should be overridden to compute a per-resource summary.
  // The method should not modify the object state, and only
  // put the result into *out as it may not be invoked in case of a
  // cache hit. The subclass may mutate *stylesheet if it so wishes.
  //
  // Note: this is called on a rewrite thread, so it should not access
  // HTML parser state.
  virtual void Summarize(Css::Stylesheet* stylesheet,
                         GoogleString* out) const = 0;

  // This is called at the end of the document when all outstanding summary
  // computations have completed, regardless of whether successful or not. It
  // will not be called at all if they are still ongoing, however.
  //
  // It's called from a context which allows HTML parser state access.  You can
  // insert things at end of document by constructing an HtmlNode* using the
  // factories in HtmlParse and calling InjectSummaryData(element).
  virtual void SummariesDone() = 0;

  // Inject summary data at the end of the document.  Intended to be called from
  // SummariesDone().  Tries to inject just before </body> if nothing else
  // intervenes; otherwise tries to inject before </html> or, failing that, at
  // the end of all content.  This latter case still works in browsers, but is
  // incredibly ugly.  It can be necessitated by other post-</html> content, or
  // by flushes in the body.
  void InjectSummaryData(HtmlNode* data);

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

  // These notify the subclass of external or inline CSS the summarizer has
  // noticed. Overriding these is optional, and base implementations do nothing.
  // TODO(morlovich): wire up external media; or should we store them in the
  //                  summaries_ vector?
  // Note that the inline method is called before </style> is called, so you
  // may need to wait until EndElementImpl if you need to alter the DOM for it.
  virtual void NotifyInlineCss(HtmlElement* style_element,
                               HtmlCharactersNode* content);
  virtual void NotifyExternalCss(HtmlElement* link);

  // Overrides of the filter APIs. You should call through to this class's
  // implementations if you override them.
  virtual void StartDocumentImpl();
  virtual void EndDocument();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual void EndElementImpl(HtmlElement* element);

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
  // attribute 'src' of 'link'.
  void StartExternalRewrite(HtmlElement* link, HtmlElement::Attribute* src);

  // Creates our rewrite context for the given slot and registers it
  // with the summaries_ vector, filling in the SummaryInfo struct in
  // a pending state.  The context will still need to have SetupInlineRewrite
  // or SetupExternalRewrite and InitiateRewrite called on it.
  // location is used to identify the resource in debug comments.
  Context* CreateContextAndSummaryInfo(const HtmlElement* element,
                                       bool external,
                                       const ResourceSlotPtr& slot,
                                       const GoogleString& location,
                                       StringPiece base_for_resources);

  ResourceSlot* MakeSlotForInlineCss(const StringPiece& content);

  // Stores all the computed summaries.
  std::vector<SummaryInfo> summaries_;

  scoped_ptr<AbstractMutex> progress_lock_;
  int outstanding_rewrites_;  // guarded by progress_lock_
  bool saw_end_of_document_;  // guarded by progress_lock_

  HtmlElement* style_element_;  // The element we are in, or NULL.
  HtmlElement* injection_point_;  // Preferred location for InjectSummaryData

  DISALLOW_COPY_AND_ASSIGN(CssSummarizerBase);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_SUMMARIZER_BASE_H_
