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

// Author: nforman@google.com (Naomi Forman)
//
// This provides the InsertGAFilter class which adds a Google Analytics
// snippet to html pages.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_INSERT_GA_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_INSERT_GA_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {
class HtmlCharactersNode;
class HtmlElement;
class RewriteDriver;
class Statistics;
class Variable;

// TODO(nforman): Replace this with our knowledge of
// document.location.protocol.
const char kGASnippet[] =
    "var _gaq = _gaq || [];"
    "_gaq.push(['_setAccount', '%s']);"
    "_gaq.push(['_trackPageview']);"
    "(function() {"
    "var ga = document.createElement('script'); ga.type = 'text/javascript';"
    "ga.async = true;"
    "ga.src = ('https:' == document.location.protocol ?"
    "'https://ssl' : 'http://www') + '.google-analytics.com/ga.js';"
    "var s = document.getElementsByTagName('script')[0];"
    "s.parentNode.insertBefore(ga, s);"
    "})();";

// This class is the implementation of insert_ga_snippet filter, which adds
// a Google Analytics snippet into the head of html pages.
class InsertGAFilter : public CommonFilter {
 public:
  explicit InsertGAFilter(RewriteDriver* rewrite_driver);
  virtual ~InsertGAFilter();

  static void Initialize(Statistics* stats);

  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);
  // HTML Events we expect to be in <script> elements.
  virtual void Characters(HtmlCharactersNode* characters);
  virtual void Flush();

  virtual const char* Name() const { return "InsertGASnippet"; }

 private:
  bool FoundSnippetInBuffer();
  // Stats on how many tags we moved.
  Variable* inserted_snippets_count_;
  HtmlElement* script_element_;
  HtmlElement* added_snippet_element_;
  GoogleString ga_id_;
  GoogleString buffer_;
  bool found_snippet_;
  DISALLOW_COPY_AND_ASSIGN(InsertGAFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_INSERT_GA_FILTER_H_
