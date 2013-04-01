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
// foot of the webpage and lazy-loaded via JS
#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_SELECTOR_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_SELECTOR_FILTER_H_

#include <vector>

#include "net/instaweb/rewriter/public/css_summarizer_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace Css {

class Stylesheet;

}  // namespace Css

namespace net_instaweb {

class CriticalSelectorSummarizedCss;
class HtmlCharactersNode;
class HtmlElement;
class RewriteDriver;

class CriticalSelectorFilter : public CssSummarizerBase {
 public:
  static const char kAddStylesScript[];
  static const char kSummarizedCssProperty[];

  explicit CriticalSelectorFilter(RewriteDriver* rewrite_driver);
  virtual ~CriticalSelectorFilter();

  virtual const char* Name() const { return "CriticalSelectorFilter"; }
  virtual const char* id() const { return "cl"; }

 protected:
  // Overrides of CssSummarizerBase summary API. These help us compute
  // the critical portions of the various fragments in the page, and to
  // write them out to the property cache.
  virtual void Summarize(Css::Stylesheet* stylesheet,
                         GoogleString* out) const;
  virtual void SummariesDone();

  // Since our computation depends on the selectors that are relevant to the
  // webpage, we incorporate them into the cache key as well.
  virtual GoogleString CacheKeySuffix() const;

  // Overrides of CssSummarizerBase CSS notification API. These hand us
  // the entirety of CSS, so we can handle it with lower priority.
  virtual void NotifyInlineCss(HtmlElement* style_element,
                               HtmlCharactersNode* content);
  virtual void NotifyExternalCss(HtmlElement* link);

  // Parser callbacks.
  virtual void StartDocumentImpl();
  virtual void EndDocument();
  virtual void EndElementImpl(HtmlElement* element);

  // Filter control API.
  virtual void DetermineEnabled();
  virtual bool UsesPropertyCacheDomCohort() const;

 private:
  class CssElement;
  class CssStyleElement;
  typedef std::vector<CssElement*> CssElementVector;

  // If insert_before is NULL, we're at end of document.
  void InsertCriticalCssIfNeeded(HtmlElement* insert_before);

  // If non-NULL, contains critical CSS for this page as available from
  // property cache.
  scoped_ptr<CriticalSelectorSummarizedCss> critical_css_;

  // Selectors that are critical for this page.
  // These are just copied over from the finder and turned into a set for easier
  // membership checking.
  StringSet critical_selectors_;

  // Summary of critical_selectors_ as a short string.
  GoogleString cache_key_suffix_;

  // Info on CSS we are delaying till the end
  CssElementVector css_elements_;

  // Style element to delete at end.
  HtmlElement* style_element_to_delete_;

  // True if we have already insert the critical CSS.
  bool inserted_critical_css_;

  DISALLOW_COPY_AND_ASSIGN(CriticalSelectorFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_SELECTOR_FILTER_H_
