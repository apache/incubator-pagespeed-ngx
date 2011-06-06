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

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
class GoogleUrl;
class HtmlElement;
class ResourceManager;
class RewriteDriver;
class RewriteOptions;

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
class CommonFilter : public EmptyHtmlFilter {
 public:
  explicit CommonFilter(RewriteDriver* driver);
  virtual ~CommonFilter();

  // Getters
  const GoogleUrl& base_url() const;
  HtmlElement* noscript_element() const { return noscript_element_; }

  // Note: Don't overload these methods, overload the implementers instead!
  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);

  // Creates an input resource with the url evaluated based on input_url
  // which may need to be absolutified relative to base_url().  Returns NULL if
  // the input resource url isn't valid, or can't legally be rewritten in the
  // context of this page.
  ResourcePtr CreateInputResource(const StringPiece& input_url);

  // Create input resource from input_url, if it is legal in the context of
  // base_url(), and if the resource can be read from cache.  If it's not in
  // cache, initiate an asynchronous fetch so it will be on next access.  This
  // is a common case for filters.
  ResourcePtr CreateInputResourceAndReadIfCached(const StringPiece& input_url);

  // Returns whether or not the base url is valid.  This value will change
  // as a filter processes the document.  E.g. If there are url refs before
  // the base tag is reached, it will return false until the filter sees the
  // base tag.  After the filter sees the base tag, it will return true.
  bool BaseUrlIsValid() const;

  RewriteDriver* driver() { return driver_; }

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
  bool seen_base_;

  DISALLOW_COPY_AND_ASSIGN(CommonFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_COMMON_FILTER_H_
