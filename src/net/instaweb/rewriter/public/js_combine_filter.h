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

// Author: morlovich@google.com (Maksim Orlovich)
//
// This provides the JsCombineFilter class which combines multiple external JS
// scripts into a single one in order to reduce the amount of fetches that need
// to be done.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_JS_COMBINE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_JS_COMBINE_FILTER_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/url_multipart_encoder.h"

namespace net_instaweb {

class HtmlCharactersNode;
class HtmlIEDirectiveNode;
class RewriteDriver;
class RewriteContext;
class Statistics;
class UrlSegmentEncoder;

// Implements combining of multiple external JS files into one via the
// following transformation:
//
// <script src="a.js">
// <stuff>
// <script src="b.js">
//
// gets turned into:
//
// <script src="a.js+b.js">
// <script>eval(mod_pagespeed_${hash("a.js")})</script>
// <stuff>
// <script>eval(mod_pagespeed_${hash("b.js")})</script>
//
// where $hash stands for using the active Hasher and tweaking the result to
// be a valid identifier continuation. Further, the combined source file
// has the code:
// var mod_pagespeed_${hash("a.js")} = "code of a.js as a string literal";
// var mod_pagespeed_${hash("b.js")} = "code of b.js as a string literal";
class JsCombineFilter : public RewriteFilter {
 public:
  static const char kJsFileCountReduction[];  // statistics variable name

  // rewrite_driver is the context owning us, and filter_id is the ID we
  // are registered under.
  explicit JsCombineFilter(RewriteDriver* rewrite_driver);
  virtual ~JsCombineFilter();

  // Registers the provided statistics variable names with 'statistics'.
  static void Initialize(Statistics* statistics);
  virtual const char* id() const {
    return RewriteOptions::kJavascriptCombinerId;
  }

 protected:
  // RewriteFilter overrides --- HTML parsing event handlers.
  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual void Flush();
  virtual void IEDirective(HtmlIEDirectiveNode* directive);
  virtual const char* Name() const { return "JsCombine"; }
  virtual RewriteContext* MakeRewriteContext();
  virtual const UrlSegmentEncoder* encoder() const {
    return &encoder_;
  }

 private:
  class JsCombiner;
  class Context;

  friend class JsCombineFilterTest;

  void ConsiderJsForCombination(HtmlElement* element,
                                HtmlElement::Attribute* src);

  // Returns JS variable name where code for given URL should be stored.
  GoogleString VarName(const GoogleString& url) const;

  void NextCombination();

  Context* MakeContext();

  JsCombiner* combiner() const;

  ScriptTagScanner script_scanner_;
  int script_depth_;  // how many script elements we are inside
  // current outermost <script> not with JavaScript we are inside, or NULL
  HtmlElement* current_js_script_;  // owned by the html parser.
  scoped_ptr<Context> context_;
  UrlMultipartEncoder encoder_;

  DISALLOW_COPY_AND_ASSIGN(JsCombineFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JS_COMBINE_FILTER_H_
