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

// Author: kspoelstra@we-amp.com (Kees Spoelstra)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_STRIP_SUBRESOURCE_HINTS_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_STRIP_SUBRESOURCE_HINTS_FILTER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/html/empty_html_filter.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;

// Removes rel=subresource links.
class StripSubresourceHintsFilter : public EmptyHtmlFilter {
 public:
  explicit StripSubresourceHintsFilter(RewriteDriver* driver);
  ~StripSubresourceHintsFilter() override;

  void StartDocument() override;
  void StartElement(HtmlElement* element) override;
  const char* Name() const override { return "StripSubresourceHints"; }

 private:
  bool ShouldStrip(HtmlElement* element);

  RewriteDriver* driver_;
  HtmlElement* delete_element_;
  bool remove_script_;
  bool remove_style_;
  bool remove_image_;
  bool remove_any_;

  DISALLOW_COPY_AND_ASSIGN(StripSubresourceHintsFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_STRIP_SUBRESOURCE_HINTS_FILTER_H_
