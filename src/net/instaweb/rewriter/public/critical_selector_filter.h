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

// Author: slamm@google.com (Stephen Lamm),
//         morlovich@google.com (Maksim Orlovich)
//
// This filters helps inline a subset of CSS critical to initial rendering of
// the webpage by focusing only on declarations whose selectors match
// elements critical to such rendering. The full original CSS is moved to the
// foot of the webpage and lazy-loaded via JS.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_SELECTOR_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_SELECTOR_FILTER_H_

#include <vector>

#include "net/instaweb/rewriter/public/css_summarizer_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace Css {

class Stylesheet;

}  // namespace Css

namespace net_instaweb {

class HtmlCharactersNode;
class HtmlElement;
class RewriteDriver;

class CriticalSelectorFilter : public CssSummarizerBase {
 public:
  static const char kAddStylesFunction[];
  static const char kAddStylesInvocation[];
  static const char kApplyFlushEarlyCss[];
  static const char kInvokeFlushEarlyCssTemplate[];
  static const char kMoveScriptId[];
  static const char kNoscriptStylesClass[];

  explicit CriticalSelectorFilter(RewriteDriver* rewrite_driver);
  virtual ~CriticalSelectorFilter();

  virtual const char* Name() const { return "CriticalSelectorFilter"; }
  virtual const char* id() const { return "cl"; }

 protected:
  // Overrides of CssSummarizerBase summary API. These help us compute
  // the critical portions of the various fragments in the page, and to
  // write them out to the page. We also use this to pick up the output
  // of filters before us, like rewrite_css; so we run this even on things
  // that will not contain on-screen critical CSS.
  virtual void Summarize(Css::Stylesheet* stylesheet,
                         GoogleString* out) const;
  virtual void RenderSummary(int pos,
                             HtmlElement* element,
                             HtmlCharactersNode* char_node,
                             bool* is_element_deleted);
  virtual void WillNotRenderSummary(int pos,
                                    HtmlElement* element,
                                    HtmlCharactersNode* char_node,
                                    bool* is_element_deleted);

  // Since our computation depends on the selectors that are relevant to the
  // webpage, we incorporate them into the cache key as well.
  virtual GoogleString CacheKeySuffix() const;

  // Parser callbacks.
  virtual void StartDocumentImpl();
  virtual void EndDocument();
  virtual void RenderDone();

  // Filter control API.
  virtual void DetermineEnabled();

 private:
  class CssElement;
  class CssStyleElement;
  typedef std::vector<CssElement*> CssElementVector;

  void RememberFullCss(int pos,
                       HtmlElement* element,
                       HtmlCharactersNode* char_node);

  bool IsCssFlushedEarly(const GoogleString& url) const;
  void ApplyCssFlushedEarly(HtmlElement* element,
                            const GoogleString& style_id,
                            const char* media);

  // Selectors that are critical for this page.
  // These are just copied over from the finder and turned into a set for easier
  // membership checking.
  StringSet critical_selectors_;

  // Summary of critical_selectors_ as a short string.
  GoogleString cache_key_suffix_;

  // Info on all the CSS in the page, potentially as optimized by other filters.
  // We will emit code to lazy-load it at the very end of the document.
  // May contain NULL pointers.
  CssElementVector css_elements_;

  // True if EndDocument was called; helps us identify last flush window.
  bool saw_end_document_;

  // True if we rendered any block at all.
  bool any_rendered_;

  // True if flush early script to move links has been added.
  bool is_flush_script_added_;

  DISALLOW_COPY_AND_ASSIGN(CriticalSelectorFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_SELECTOR_FILTER_H_
