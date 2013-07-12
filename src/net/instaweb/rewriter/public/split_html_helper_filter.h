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

// Author: bharathbhushan@google.com (Bharath Bhushan Kowshik Raghupathi)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_SPLIT_HTML_HELPER_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_SPLIT_HTML_HELPER_FILTER_H_

#include "base/logging.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;
class SplitHtmlConfig;
class SplitHtmlState;

// Filter which helps in the presence of split html filter. Based on the xpath
// configuration it will decide the above-the-fold panels and below-the-fold
// panels and makes sure that downstream filters like inline preview images and
// lazyload images can work well in the absence of critical image information.
//
// When the above-the-fold html fragment is requested it allows the images in
// those panels to be inline previewed. When the below-the-fold html fragment is
// requested it allows the images in those panels to be lazyloaded.
class SplitHtmlHelperFilter : public CommonFilter {
 public:
  explicit SplitHtmlHelperFilter(RewriteDriver* rewrite_driver);
  virtual ~SplitHtmlHelperFilter();

  virtual void DetermineEnabled();

  virtual void StartDocumentImpl();
  virtual void EndDocument();

  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);

  const HtmlElement* current_panel_element() const {
    return current_panel_element_;
  }

  void set_current_panel_element(const HtmlElement* element) {
    DCHECK(element == NULL || current_panel_element_ == NULL);
    current_panel_element_ = element;
  }

  virtual const char* Name() const { return "SplitHtmlHelperFilter"; }

 private:
  // Pops the html element from the top of the stack.
  void EndPanelInstance();

  // Pushes the element corresponding to the current panel on the stack.
  void StartPanelInstance(HtmlElement* element, const GoogleString& panelid);

  const SplitHtmlConfig* config_;  // Owned by the RewriteDriver.

  scoped_ptr<SplitHtmlState> state_;

  // Not owned by this class. This is the element corresponding to the current
  // below-the-fold panel.
  const HtmlElement* current_panel_element_;

  DISALLOW_COPY_AND_ASSIGN(SplitHtmlHelperFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SPLIT_HTML_HELPER_FILTER_H_
