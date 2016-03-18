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

#ifndef PAGESPEED_KERNEL_HTML_EMPTY_HTML_FILTER_H_
#define PAGESPEED_KERNEL_HTML_EMPTY_HTML_FILTER_H_

#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/html/html_filter.h"

namespace net_instaweb {

class HtmlCdataNode;
class HtmlCharactersNode;
class HtmlCommentNode;
class HtmlDirectiveNode;
class HtmlElement;
class HtmlIEDirectiveNode;

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
  virtual void DetermineEnabled(GoogleString* disabled_reason);

  // This filter and derived classes will not rewrite urls.  If a derived filter
  // wants to rewrite urls, override this function.
  virtual bool CanModifyUrls() { return false; }

  // Invoked by the rewrite driver to query whether this filter may
  // inject scripts.
  ScriptUsage GetScriptUsage() const override { return kNeverInjectsScripts; }

  // Note -- this does not provide an implementation for Name().  This
  // must be supplied by derived classes.
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTML_EMPTY_HTML_FILTER_H_
