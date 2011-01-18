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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_FILTER_H_
#define NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_FILTER_H_

#include <string>
#include "net/instaweb/htmlparse/public/html_parser_types.h"

namespace net_instaweb {

class HtmlFilter {
 public:
  HtmlFilter();
  virtual ~HtmlFilter();

  // Starts a new document.  Filters should clear their state in this function,
  // as the same Filter instance may be used for multiple HTML documents.
  virtual void StartDocument() = 0;
  // Note: EndDocument will be called imediately before the last Flush call.
  virtual void EndDocument() = 0;

  // When an HTML element is encountered during parsing, each filter's
  // StartElement method is called.  The HtmlElement lives for the entire
  // duration of the document.
  //
  // TODO(jmarantz): consider passing handles rather than pointers and
  // reference-counting them instead to save memory on long documents.
  virtual void StartElement(HtmlElement* element) = 0;
  virtual void EndElement(HtmlElement* element) = 0;

  // Called for CDATA blocks (e.g. <![CDATA[foobar]]>)
  virtual void Cdata(HtmlCdataNode* cdata) = 0;

  // Called for HTML comments that aren't IE directives (e.g. <!--foobar-->).
  virtual void Comment(HtmlCommentNode* comment) = 0;

  // Called for an IE directive; typically used for CSS styling.
  // See http://msdn.microsoft.com/en-us/library/ms537512(VS.85).aspx
  //
  // TODO(mdsteele): Should we try to maintain the nested structure of
  // the conditionals, in the same way that we maintain nesting of elements?
  virtual void IEDirective(HtmlIEDirectiveNode* directive) = 0;

  // Called for raw characters between tags.
  virtual void Characters(HtmlCharactersNode* characters) = 0;

  // Called for HTML directives (e.g. <!doctype foobar>).
  virtual void Directive(HtmlDirectiveNode* directive) = 0;

  // Notifies the Filter that a flush is occurring.  A filter that's
  // generating streamed output should flush at this time.  A filter
  // that's mutating elements can mutate any element seen since the
  // most recent flush; once an element is flushed it is already on
  // the wire to its destination and it's too late to mutate.  Flush
  // is initiated by an application calling HttpParse::Flush().
  //
  // Flush() is called after all other handlers during a HttpParse::Flush().
  virtual void Flush() = 0;

  // The name of this filter -- used for logging and debugging.
  virtual const char* Name() const = 0;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_FILTER_H_
