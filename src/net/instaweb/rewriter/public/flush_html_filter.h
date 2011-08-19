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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_FLUSH_HTML_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_FLUSH_HTML_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;

// This filter is run immediately after lexing when streaming HTML into
// the system.  It is used to monitor the HTML and try to figure out good
// times to flush, based on document structure and timing.
class FlushHtmlFilter : public CommonFilter {
 public:
  explicit FlushHtmlFilter(RewriteDriver* driver);
  virtual ~FlushHtmlFilter();

  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);
  virtual void Flush();

  virtual const char* Name() const { return "FlushHtmlFilter"; }

 private:
  ResourceTagScanner tag_scanner_;
  int score_;

  DISALLOW_COPY_AND_ASSIGN(FlushHtmlFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_FLUSH_HTML_FILTER_H_
