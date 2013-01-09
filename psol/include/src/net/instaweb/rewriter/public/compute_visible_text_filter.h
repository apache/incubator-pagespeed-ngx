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

// Author: guptaa@google.com (Ashish Gupta)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_COMPUTE_VISIBLE_TEXT_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_COMPUTE_VISIBLE_TEXT_FILTER_H_

#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class HtmlCdataNode;
class HtmlCharactersNode;
class HtmlCommentNode;
class HtmlDirectiveNode;
class HtmlElement;
class HtmlIEDirectiveNode;
class RewriteDriver;

// This rewriter allows only the raw characters between the tags to be written.
// Following tags are entirely deleted by this filter:
// script
// style
// cdata
// directive
// iedirective
// noscript
// This is an experimental rewriter and should be used carefully.
class ComputeVisibleTextFilter : public HtmlWriterFilter {
 public:
  explicit ComputeVisibleTextFilter(RewriteDriver* rewrite_driver);
  virtual ~ComputeVisibleTextFilter();

  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);
  virtual void Cdata(HtmlCdataNode* cdata);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual void Comment(HtmlCommentNode* comment);
  virtual void IEDirective(HtmlIEDirectiveNode* directive);
  virtual void Directive(HtmlDirectiveNode* directive);
  virtual void EndDocument();
  virtual const char* Name() const { return "ComputeVisibleTextFilter"; }

 private:
  RewriteDriver* rewrite_driver_;
  StringWriter writer_;
  GoogleString buffer_;

  DISALLOW_COPY_AND_ASSIGN(ComputeVisibleTextFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_COMPUTE_VISIBLE_TEXT_FILTER_H_
