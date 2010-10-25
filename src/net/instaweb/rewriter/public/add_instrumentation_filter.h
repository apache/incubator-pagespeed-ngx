/**
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

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/util/public/atom.h"

namespace net_instaweb {

class Statistics;
class Variable;

// Injects javascript instrumentation for monitoring page-rendering time.
class AddInstrumentationFilter : public EmptyHtmlFilter {
 public:
  explicit AddInstrumentationFilter(HtmlParse* parser,
                                    const StringPiece& beacon_url,
                                    Statistics* statistics);

  static void Initialize(Statistics* statistics);
  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);
  virtual const char* Name() const { return "AddInstrumentation"; }
  // Handles an incoming beacon request by incrementing the appropriate
  // variables.  Returns true if the url was parsed and handled correctly; in
  // this case a 204 No Content response should be sent.  Returns false if the
  // url could not be parsed; in this case the request should be declined.
  bool HandleBeacon(const StringPiece& unparsed_url);
 private:
  HtmlParse* html_parse_;
  bool found_head_;
  Atom s_head_;
  Atom s_body_;
  Variable* total_page_load_ms_;
  Variable* page_load_count_;
  std::string beacon_url_;

  DISALLOW_COPY_AND_ASSIGN(AddInstrumentationFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_ADD_INSTRUMENTATION_FILTER_H_
