/*
 * Copyright 2010 Google Inc.
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

// Author: abliss@google.com (Adam Bliss)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_ADD_INSTRUMENTATION_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_ADD_INSTRUMENTATION_FILTER_H_

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;
class Statistics;
class Variable;

// Injects javascript instrumentation for monitoring page-rendering time.
class AddInstrumentationFilter : public EmptyHtmlFilter {
 public:
  static const char kLoadTag[];
  static const char kUnloadTag[];
  static GoogleString* kUnloadScriptFormatXhtml;
  static GoogleString* kTailScriptFormatXhtml;

  // Counters.
  static const char kInstrumentationScriptAddedCount[];

  explicit AddInstrumentationFilter(RewriteDriver* driver);
  virtual ~AddInstrumentationFilter();

  static void Initialize(Statistics* statistics);

  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);
  virtual const char* Name() const { return "AddInstrumentation"; }

 protected:
  // The total number of times instrumentation script is added.
  Variable* instrumentation_script_added_count_;

 private:
  // Adds a script node to given element using the specified format and
  // tag name.
  void AddScriptNode(HtmlElement* element, const GoogleString& script_format,
                     const GoogleString& tag_name);

  RewriteDriver* driver_;
  bool found_head_;
  bool use_cdata_hack_;
  bool added_tail_script_;
  bool added_unload_script_;

  DISALLOW_COPY_AND_ASSIGN(AddInstrumentationFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_ADD_INSTRUMENTATION_FILTER_H_
