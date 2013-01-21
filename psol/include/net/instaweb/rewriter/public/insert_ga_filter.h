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

extern const char kGAFuriousSnippet[];
extern const char kGAJsSnippet[];
extern const char kGASpeedTracking[];

// This class is the implementation of insert_ga_snippet filter, which adds
// a Google Analytics snippet into the head of html pages.
class InsertGAFilter : public CommonFilter {
 public:
  explicit InsertGAFilter(RewriteDriver* rewrite_driver);
  virtual ~InsertGAFilter();

  // Set up statistics for this filter.
  static void InitStats(Statistics* stats);

  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);
  // HTML Events we expect to be in <script> elements.
  virtual void Characters(HtmlCharactersNode* characters);

  virtual const char* Name() const { return "InsertGASnippet"; }

 private:
  // Construct the custom variable part for Furious of the GA snippet.
  GoogleString ConstructFuriousSnippet() const;

  // Construct a stand-alone GA snippet to send back Furious info.
  GoogleString MakeFullFuriousSnippet() const;

  // If appropriate, insert the GA snippet at the end of the body element.
  void HandleEndBody(HtmlElement* body);

  // Adds a script node with text as its contents either as a child of the
  // current_element or immediately after current element.
  void AddScriptNode(HtmlElement* current_element, GoogleString text,
                     bool insert_immediately_after_current);

  // Look to see if the script had a GA snippet, and modify our code
  // appropriately.
  void HandleEndScript(HtmlElement* script);

  // Indicates whether or not buffer_ contains a GA snippet with the
  // same id as ga_id_.
  bool FoundSnippetInBuffer() const;

  // Stats on how many tags we moved.
  Variable* inserted_ga_snippets_count_;

  // Script element we're currently in, so we can check it to see if
  // it has the GA snippet already.
  HtmlElement* script_element_;
  // Whether we added the analytics js or not.
  bool added_analytics_js_;
  // Whether we added the furious snippet or not.
  bool added_furious_snippet_;

  // GA ID for this site.
  GoogleString ga_id_;

  // Buffer in which we collect the contents of any script element we're
  // looking for the GA snippet in.
  GoogleString buffer_;

  // Indicates whether or not we've already found a GA snippet.
  bool found_snippet_;

  // Increase site-speed tracking to the max allowed.
  bool increase_speed_tracking_;

  DISALLOW_COPY_AND_ASSIGN(InsertGAFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_INSERT_GA_FILTER_H_
