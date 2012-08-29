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
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
class GoogleUrl;
class HtmlElement;
class ServerContext;
class ResponseHeaders;
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

  // URL of the requested HTML or resource.
  const GoogleUrl& base_url() const;

  // For rewritten resources, decoded_base_url() is the base of the original
  // (un-rewritten) resource's URL.
  const GoogleUrl& decoded_base_url() const;

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

  // Returns whether or not the base url is valid.  This value will change
  // as a filter processes the document.  E.g. If there are url refs before
  // the base tag is reached, it will return false until the filter sees the
  // base tag.  After the filter sees the base tag, it will return true.
  bool BaseUrlIsValid() const;

  RewriteDriver* driver() { return driver_; }

  // Returns whether the current options specify the "debug" filter.
  // If set, then other filters can annotate output HTML with HTML
  // comments indicating why they did or did not do an optimization,
  // using HtmlParse::InsertComment.
  bool DebugMode() const { return driver_->DebugMode(); }

  // Utility function to extract the mime type and/or charset from a meta tag,
  // either the HTML4 http-equiv form or the HTML5 charset form:
  // element is the meta tag element to process.
  // headers is optional: if provided it is checked to see if it already has
  //         a content type with the tag's value; if so, returns false.
  // content is set to the content attribute's value, http-equiv form only.
  // mime_type is set to the extracted mime type, if any.
  // charset is the set to the extracted charset, if any.
  // returns true if the details were extracted, false if not. If true is
  // returned then content will be empty for the HTML5 charset form and
  // non-empty for the HTML4 http-equiv form; also an http-equiv attribute
  // with a blank mime type returns false as it's not a valid format.
  static bool ExtractMetaTagDetails(const HtmlElement& element,
                                    const ResponseHeaders* headers,
                                    GoogleString* content,
                                    GoogleString* mime_type,
                                    GoogleString* charset);

  // Add this filter to the logged list of applied rewriters. The intended
  // semantics of this are that it should only include filters that modified the
  // content of the response to the request being processed.
  // This class logs using Name(); subclasses may do otherwise.
  virtual void LogFilterModifiedContent();

 protected:
  // Overload these implementer methods:
  // Intentionally left abstract so that implementers don't forget to change
  // the name from Blah to BlahImpl.
  virtual void StartDocumentImpl() = 0;
  virtual void StartElementImpl(HtmlElement* element) = 0;
  virtual void EndElementImpl(HtmlElement* element) = 0;

  // Protected pointers for inheriter's to use
  RewriteDriver* driver_;
  ServerContext* resource_manager_;
  const RewriteOptions* rewrite_options_;

 private:
  HtmlElement* noscript_element_;
  bool seen_base_;

  DISALLOW_COPY_AND_ASSIGN(CommonFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_COMMON_FILTER_H_
