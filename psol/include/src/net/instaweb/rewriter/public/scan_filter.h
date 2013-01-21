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

#include "net/instaweb/htmlparse/public/empty_html_filter.h"

namespace net_instaweb {

class HtmlCdataNode;
class HtmlCharactersNode;
class HtmlCommentNode;
class HtmlDirectiveNode;
class HtmlElement;
class HtmlIEDirectiveNode;
class RewriteDriver;

// Filter that is run before any other, to help track base-tag usage and
// changes to help identify and deal conservatively with situation where HTML
// files update the base-tag more than once or use the base-tag prior to it
// being changed.  Such situations are not well-defined and what we want to
// do is avoid rewriting any resources whose interpretation might be hard
// to predict due to browser differences.
class ScanFilter : public EmptyHtmlFilter {
 public:
  explicit ScanFilter(RewriteDriver* driver);
  virtual ~ScanFilter();

  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void Cdata(HtmlCdataNode* cdata);
  virtual void Comment(HtmlCommentNode* comment);
  virtual void IEDirective(HtmlIEDirectiveNode* directive);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual void Directive(HtmlDirectiveNode* directive);
  virtual void Flush();

  virtual const char* Name() const { return "Scan"; }

 private:
  RewriteDriver* driver_;
  bool seen_any_nodes_;
  bool seen_refs_;
  bool seen_base_;
  bool seen_meta_tag_charset_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SCAN_FILTER_H_
