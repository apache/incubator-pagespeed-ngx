/*
 * Copyright 2015 Google Inc.
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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_MENU_RENDER_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_MENU_RENDER_FILTER_H_

#include "net/instaweb/rewriter/mobilize_menu.pb.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/html/html_element.h"

namespace net_instaweb {

class MobilizeMenuRenderFilter : public CommonFilter {
 public:
  static const char kMobilizeMenuPropertyName[];

  explicit MobilizeMenuRenderFilter(RewriteDriver* driver);

  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element) {}
  virtual void EndDocument();
  virtual void RenderDone();
  virtual void DetermineEnabled(GoogleString* disabled_reason);

  virtual const char* Name() const { return "MobilizeMenuRenderFilter"; }

 private:
  class MenuComputation;

  bool saw_end_document_;
  bool menu_computed_;
  scoped_ptr<const MobilizeMenu> menu_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_MOBILIZE_MENU_RENDER_FILTER_H_
