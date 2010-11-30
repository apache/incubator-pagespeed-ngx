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

// Author: sligocki@google.com (Shawn Ligocki)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_BASE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_BASE_FILTER_H_

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/util/public/atom.h"
#include "net/instaweb/util/public/google_url.h"

namespace net_instaweb {

class HtmlElement;
class HtmlParse;
class Resource;
class RewriteDriver;
class OutputResource;
class UrlSegmentEncoder;

// CommonFilter encapsulates useful functionality that many filters will want.
// All filters who want this functionality should inherit from CommonFilter and
// define the Helper methods rather than the main methods.
//
// Currently, it stores the current base URL (which can depend on where you
// are on a page since the <base> element does not have to be first)
// and whether we are in a <noscript> element (in which case, we should be
// careful about moving things out of this element).
class CommonFilter : public EmptyHtmlFilter {
 public:
  CommonFilter(RewriteDriver* driver);
  virtual ~CommonFilter();

  // Getters
  HtmlParse* html_parse() { return html_parse_; }
  const GURL& base_gurl() { return base_gurl_; }
  HtmlElement* noscript_element() { return noscript_element_; }

  // Note: Don't overload these methods, overload the implementers instead!
  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);

  Resource* CreateInputResource(const StringPiece& url);
  Resource* CreateInputResourceAbsolute(const StringPiece& url);
  Resource* CreateInputResourceAndReadIfCached(const StringPiece& url);
  Resource* CreateInputResourceFromOutputResource(
      UrlSegmentEncoder* encoder, OutputResource* output_resource);

 protected:
  // Overload these implementer methods:
  // Intentionally left abstract so that implementers don't forget to change
  // the name from Blah to BlahImpl.
  virtual void StartDocumentImpl() = 0;
  virtual void StartElementImpl(HtmlElement* element) = 0;
  virtual void EndElementImpl(HtmlElement* element) = 0;

 private:
  RewriteDriver* driver_;
  HtmlParse* html_parse_;
  // TODO(sligocki): Maybe: don't store a separate GURL in each filter.
  GURL base_gurl_;
  HtmlElement* noscript_element_;

  const Atom s_base_;
  const Atom s_href_;
  const Atom s_noscript_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CommonFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_BASE_FILTER_H_
