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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_SCAN_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_SCAN_FILTER_H_

#include <vector>
#include "net/instaweb/htmlparse/public/html_filter.h"

namespace net_instaweb {

class CommonFilter;
class RewriteDriver;

// Filter that is run before any other, to help pre-scan for URLs
// that need to be asynchronously fetched.
class ScanFilter : public HtmlFilter {
 public:
  explicit ScanFilter(RewriteDriver* driver) : driver_(driver) {}
  virtual ~ScanFilter();

  virtual void StartDocument();
  virtual void EndDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);
  virtual void Cdata(HtmlCdataNode* cdata);
  virtual void Comment(HtmlCommentNode* comment);
  virtual void IEDirective(HtmlIEDirectiveNode* directive);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual void Directive(HtmlDirectiveNode* directive);
  virtual void Flush();

  virtual const char* Name() const { return "Scan"; }

  // Adds a filter in to the scan-list so that the filters have
  // the opportunity to scan for URLs to initiate fetches.
  void add_filter(CommonFilter* filter) { filters_.push_back(filter); }

 private:
  std::vector<CommonFilter*> filters_;
  RewriteDriver* driver_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SCAN_FILTER_H_
