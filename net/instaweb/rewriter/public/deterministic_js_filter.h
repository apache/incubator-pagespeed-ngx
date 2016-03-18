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

// Author: mmohabey@google.com (Megha Mohabey)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DETERMINISTIC_JS_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DETERMINISTIC_JS_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_filter.h"

namespace net_instaweb {

// Injects javascript at the beginning of the head tag to make it deterministic.
// The JS redefines functions like Math.random and Date. This filter is useful
// for testing and measurement but does not provide any latency gains. A head
// element is added if it is not already present in the html.
class DeterministicJsFilter : public CommonFilter {
 public:
  explicit DeterministicJsFilter(RewriteDriver* driver);
  virtual ~DeterministicJsFilter();

  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element) { }
  virtual const char* Name() const { return "DeterministicJs"; }
  ScriptUsage GetScriptUsage() const override { return kWillInjectScripts; }

 private:
  bool found_head_;

  DISALLOW_COPY_AND_ASSIGN(DeterministicJsFilter);
};

}  // namespace net_instaweb


#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DETERMINISTIC_JS_FILTER_H_
