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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_JS_DEFER_DISABLED_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_JS_DEFER_DISABLED_FILTER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"

namespace net_instaweb {

class RewriteDriver;
class HtmlElement;
class Statistics;

// Implements deferring of javascripts into post onload.
// JsDisableFilter moves scripts inside a noscript tag. This
// filter adds a javascript that goes through every noscript
// tag to defer them to be executed at onload of window.
class JsDeferDisabledFilter : public EmptyHtmlFilter {
 public:
  static const char* kDeferJsCode;

  explicit JsDeferDisabledFilter(RewriteDriver* driver);
  virtual ~JsDeferDisabledFilter();

  virtual void StartDocument();
  virtual void EndElement(HtmlElement* element);
  virtual void EndDocument();
  virtual const char* Name() const { return "JsDeferDisabledFilter"; }

  static StringPiece defer_js_code() { return *opt_defer_js_; }

  static void Initialize(Statistics* statistics);
  static void Terminate();

 private:
  // The script that will be inlined at the end of BODY.
  static GoogleString* opt_defer_js_;  // Minified version.
  static GoogleString* debug_defer_js_;  // Debug version.

  RewriteDriver* rewrite_driver_;

  // The script that will be inlined at the end of BODY.
  bool script_written_;
  bool defer_js_enabled_;
  bool debug_;

  DISALLOW_COPY_AND_ASSIGN(JsDeferDisabledFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JS_DEFER_DISABLED_FILTER_H_
