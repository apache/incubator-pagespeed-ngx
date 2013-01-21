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

#ifndef NET_INSTAWEB_HTMLPARSE_PUBLIC_EMPTY_HTML_FILTER_H_
#define NET_INSTAWEB_HTMLPARSE_PUBLIC_EMPTY_HTML_FILTER_H_

#include "net/instaweb/htmlparse/public/html_filter.h"

// This 'string' is not needed for this include.  But removing it breaks
// PageSpeed Insights.  See
// third_party/pagespeed/src/pagespeed/html/external_resource_filter.h
// Remove this once that one is fixed.
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class HtmlCdataNode;
class HtmlCharactersNode;
class HtmlCommentNode;
class HtmlDirectiveNode;
class HtmlElement;
class HtmlIEDirectiveNode;

// TODO(jmarantz): remove this -- it's IWYU-superfluous but is needed until
// PageSpeed Insights puts this declaration in.  Note that IWYU annoyingly
// ignores the pragma on forward declarations, but the intent should be clear.
class HtmlParse;

// Base class for rewriting filters that don't need to be sure to
// override every filter method.  Other filters that need to be sure
// they override every method would derive directly from HtmlFilter.
class EmptyHtmlFilter : public HtmlFilter {
 public:
  EmptyHtmlFilter();
  virtual ~EmptyHtmlFilter();

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
  virtual void DetermineEnabled();

  // Note -- this does not provide an implementation for Name().  This
  // must be supplied by derived classes.
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTMLPARSE_PUBLIC_EMPTY_HTML_FILTER_H_
