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

// Author: sriharis@google.com (Srihari Sukumaran)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_FIX_REFLOW_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_FIX_REFLOW_FILTER_H_

#include "base/macros.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/empty_html_filter.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;

// Fixes reflow due to defer_javascript.
//
class FixReflowFilter : public EmptyHtmlFilter {
 public:
  static const char kElementRenderedHeightPropertyName[];

  explicit FixReflowFilter(RewriteDriver* driver);
  virtual ~FixReflowFilter();

  virtual void DetermineEnabled(GoogleString* disabled_reason);
  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);

  virtual const char* Name() const {
    return "FixReflowFilter";
  }

 private:
  typedef StringStringMap ElementHeightMap;
  ElementHeightMap element_height_map_;

  // We do not own this.
  RewriteDriver* rewrite_driver_;

  DISALLOW_COPY_AND_ASSIGN(FixReflowFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_FIX_REFLOW_FILTER_H_
