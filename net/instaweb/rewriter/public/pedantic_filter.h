/*
 * Copyright 2012 Google Inc.
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

// Author: jkarlin@google.com (Josh Karlin)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_PEDANTIC_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_PEDANTIC_FILTER_H_

#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/html/empty_html_filter.h"

namespace net_instaweb {
class HtmlElement;
class HtmlParse;

// Add mime types for <style> and <script> so that the output passes HTML4+5
// validation.
class PedanticFilter : public EmptyHtmlFilter {
 public:
  explicit PedanticFilter(HtmlParse* html_parse);
  virtual ~PedanticFilter();

  virtual void StartElement(HtmlElement* element);
  virtual const char* Name() const { return "Pedantic"; }

 private:
  HtmlParse* html_parse_;
  ScriptTagScanner script_scanner_;

  DISALLOW_COPY_AND_ASSIGN(PedanticFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_PEDANTIC_FILTER_H_
