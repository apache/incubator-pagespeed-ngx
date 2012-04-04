/*
 * Copyright 2011 Google Inc.
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

// Author: atulvasu@google.com (Atul Vasu)
//         sriharis@google.com (Srihari Sukumaran)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DETECT_REFLOW_JS_DEFER_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DETECT_REFLOW_JS_DEFER_FILTER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"

namespace net_instaweb {

class RewriteDriver;
class HtmlElement;
class Statistics;

// Similar to JsDeferDisabledFilter, but adds some extra js to figure out
// potential page rendering reflows due to deferred script execution.
class DetectReflowJsDeferFilter : public EmptyHtmlFilter {
 public:
  explicit DetectReflowJsDeferFilter(RewriteDriver* driver);
  virtual ~DetectReflowJsDeferFilter();

  virtual void StartDocument();
  virtual void EndElement(HtmlElement* element);
  virtual void EndDocument();
  virtual const char* Name() const { return "DetectReflowJsDeferFilter"; }

  static void Initialize(Statistics* statistics);
  static void Terminate();

 private:
  RewriteDriver* rewrite_driver_;

  // The script that will be inlined at the end of BODY.
  bool script_written_;
  bool defer_js_enabled_;
  bool debug_;

  DISALLOW_COPY_AND_ASSIGN(DetectReflowJsDeferFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DETECT_REFLOW_JS_DEFER_FILTER_H_
