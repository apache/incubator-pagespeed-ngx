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
// can lookup summaries via NumStyles/GetSummary.
class CssSummarizerBase : public RewriteFilter {
 public:
  explicit CssSummarizerBase(RewriteDriver* driver);
  virtual ~CssSummarizerBase();

 protected:
  // This should be overridden to compute a per-resource summary.
  // The method should not modify the object state, and only
  // put the result into *out as it may not be invoked in case of a
  // cache hit.
  //
  // Note: this is called on a rewrite thread, so it should not access
  // HTML parser state.
  virtual void Summarize(const Css::Stylesheet& stylesheet,
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

  // Returns summary computed for the pos'th style in the document; or NULL
  // if it's not available. (Could be because of parse error, or because
  // we don't have the result ready in time, etc.)
  //
  // pos must be in [0, NumStyles())
  //
  // Should be called from a thread context that has HTML parser state access.
  const GoogleString* GetSummary(int pos) const {
    return summaries_.at(pos);
  }

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

  // Starts the asynchronous rewrite process for inline CSS 'text'.
  void StartInlineRewrite(HtmlCharactersNode* text);

  // Starts the asynchronous rewrite process for external CSS referenced by
  // attribute 'src' of 'link'.
  void StartExternalRewrite(HtmlElement* link, HtmlElement::Attribute* src);

  // Creates our rewrite context for the given slot. Also registers it
  // with the summaries_ vector and gives it an id. The context will
  // still need to have SetupInlineRewrite / SetupExternalRewrite and
  // InitiateRewrite called on it.
  Context* CreatContextForSlot(const ResourceSlotPtr& slot);

  ResourceSlot* MakeSlotForInlineCss(const StringPiece& content);

  // Stores all the computed summaries.
  StringStarVector summaries_;

  scoped_ptr<AbstractMutex> progress_lock_;
  int outstanding_rewrites_;  // guarded by progress_lock_
  bool saw_end_of_document_;  // guarded by progress_lock_

  HtmlElement* style_element_;  // The element we are in, or NULL.
  HtmlElement* injection_point_;  // Preferred location for InjectSummaryData

  DISALLOW_COPY_AND_ASSIGN(CssSummarizerBase);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_SUMMARIZER_BASE_H_
