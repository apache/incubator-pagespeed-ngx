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

// Author: sligocki@google.com (Shawn Ligocki)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_COMMON_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_COMMON_FILTER_H_

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/atom.h"
#include "net/instaweb/util/public/google_url.h"

namespace net_instaweb {

class HtmlElement;
class HtmlParse;
class Resource;
class ResourceManager;
class RewriteOptions;
class OutputResource;
class UrlSegmentEncoder;

// CommonFilter encapsulates useful functionality that many filters will want.
// All filters who want this functionality should inherit from CommonFilter and
// define the Helper methods rather than the main methods.
//
// Currently, it stores whether we are in a <noscript> element (in
// which case, we should be careful about moving things out of this
// element).
//
// The base-tag is maintained in the RewriteDriver, although it can be
// accessed via a convenience method here for historical reasons.
//
// CommonFilters are given the opportunity to scan HTML elements for
// resources prior to the HTML rewriting phase to initiate fetching
// and rewriting.  See the Scan methods below.
class CommonFilter : public EmptyHtmlFilter {
 public:
  CommonFilter(RewriteDriver* driver);
  virtual ~CommonFilter();

  // Getters
  const GoogleUrl& base_url() const { return driver_->base_url(); }
  HtmlElement* noscript_element() const { return noscript_element_; }

  // Note: Don't overload these methods, overload the implementers instead!
  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);

  Resource* CreateInputResource(const StringPiece& url);
  Resource* CreateInputResourceAndReadIfCached(const StringPiece& url);
  Resource* CreateInputResourceFromOutputResource(
      UrlSegmentEncoder* encoder, OutputResource* output_resource);

  // Methods to help implement two-pass scanning of HTML documents, where:
  // 1.  In the first pass we make requests of an asynchronous cache
  // 2.  Between the two passes, we wait a bounded amount of time for caches
  //     and URL fetchers to respond
  // 3.  In the second pass we rewrite HTML.
  //
  // Each filter's Scan* methods will be called in the first pass.  During
  // this pass, the document and the filter state should not be mutated.
  // All that should happen in this pass is new URLs can be requested.
  //
  // Note: this adds overhead to the filter-dispatch mechanism, but
  // the absolute numbers are small.  In pending CL 19863734 there is
  // a test that calls Parse/Flush 1M times, and this takes ~4.3s with
  // this extra scan infrastructure, or less than 5us per requset.
  // Without this scan-infrastructure it takes ~4.0s, so this extra
  // scan overhead is cheap, but not free.  In the future if we need
  // to shave another 300ns from our parse time we can change this
  // dispatch mechanism to require an explicit registration step.
  virtual void ScanStartDocument() const;
  virtual void ScanEndDocument() const;
  virtual void ScanStartElement(HtmlElement* element) const;
  virtual void ScanEndElement(HtmlElement* element) const;
  virtual void ScanComment(HtmlCommentNode* comment) const;
  virtual void ScanIEDirective(HtmlIEDirectiveNode* directive) const;
  virtual void ScanCharacters(HtmlCharactersNode* characters) const;
  virtual void ScanDirective(HtmlDirectiveNode* directive) const;
  virtual void ScanCdata(HtmlCdataNode* cdata) const;

 protected:
  // Overload these implementer methods:
  // Intentionally left abstract so that implementers don't forget to change
  // the name from Blah to BlahImpl.
  virtual void StartDocumentImpl() = 0;
  virtual void StartElementImpl(HtmlElement* element) = 0;
  virtual void EndElementImpl(HtmlElement* element) = 0;

  // Protected pointers for inheriter's to use
  RewriteDriver* driver_;
  ResourceManager* resource_manager_;
  const RewriteOptions* rewrite_options_;

 private:
  HtmlElement* noscript_element_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CommonFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_COMMON_FILTER_H_
