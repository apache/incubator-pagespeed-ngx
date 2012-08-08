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

// Author: rahulbansal@google.com (Rahul Bansal)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_BLINK_BACKGROUND_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_BLINK_BACKGROUND_FILTER_H_

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;
class RewriteOptions;

// This class does the preprocessing required to apply blink.
class BlinkBackgroundFilter : public EmptyHtmlFilter {
 public:
  explicit BlinkBackgroundFilter(RewriteDriver* rewrite_driver);
  virtual ~BlinkBackgroundFilter();

  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);
  virtual const char* Name() const { return "ProcessBlinkInBackgroundFilter"; }

 private:
  RewriteDriver* rewrite_driver_;
  const RewriteOptions* rewrite_options_;
  ScriptTagScanner script_tag_scanner_;
  bool script_written_;

  // Inserts blink js and defer js init code into the head element. If no head
  // tag in the page, it inserts one before body tag.
  void InsertBlinkJavascript(HtmlElement* element);

  DISALLOW_COPY_AND_ASSIGN(BlinkBackgroundFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_BLINK_BACKGROUND_FILTER_H_
