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

#ifndef PAGESPEED_KERNEL_HTML_HTML_FILTER_H_
#define PAGESPEED_KERNEL_HTML_HTML_FILTER_H_

#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

class HtmlCdataNode;
class HtmlCharactersNode;
class HtmlCommentNode;
class HtmlDirectiveNode;
class HtmlElement;
class HtmlIEDirectiveNode;

// Base-class used to register for HTML Parser Callbacks.  Derive from this
// class and register with HtmlParse::AddFilter to use the HTML Parser.
class HtmlFilter {
 public:
  // Describes a filter's relationship with scripts.
  enum ScriptUsage {
    // Indicates that this filter generally needs to inject scripts, and
    // therefore should be disabled in environments where scripts are
    // not allowed, such as amp.  The system also DCHECKs of scripts are
    // injected from a filter where CanInjectScripts() is false.
    kWillInjectScripts,

    // Indicates that this filter may in some cases inject scripts,
    // but still has value even if scripts are forbidden.  For the
    // rare cases where this value is appropriate, the filter must be
    // explicitly verified to function correctly.
    kMayInjectScripts,

    // TODO(jmarantz): Remove kRequiresScriptExecutionFilterSet in
    // rewrite_options.cc, and instead add new enum choices here covering
    // combinations of requiring 'noscript' behavior and their injection
    // behavior.
    kNeverInjectsScripts    // Indicates this filter never injects scripts.
  };

  HtmlFilter();
  virtual ~HtmlFilter();

  // Starts a new document.  Filters should clear their state in this function,
  // as the same Filter instance may be used for multiple HTML documents.
  virtual void StartDocument() = 0;

  // Note: EndDocument will be called immediately before the last Flush call.
  // (which also means that in the RewriteDriver use it is called before
  //  rendering for the last flush window).
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
  // Flush() is called after all other handlers during a HttpParse::Flush(),
  // except RenderDone(), which (if in use) happens after Flush().
  virtual void Flush() = 0;

  // Notifies a filter that an asynchronous rewrite & render computation
  // phase has finished. This is not used by HtmlParse itself, but only by
  // RewriteDriver for pre-render filters. Happens after the corresponding
  // flush, for every flush window. Default implementation does nothing.
  // TODO(morlovich): Push this down into CommonFilter and convert all the
  // pre-render filters to inherit off it.
  virtual void RenderDone();

  // Invoked by rewrite driver where all filters should determine whether
  // they are enabled for this request. The re-writer my optionally set
  // disabled_reason to explain why it disabled itself, which will appear
  // in the debug output.
  virtual void DetermineEnabled(GoogleString* disabled_reason) = 0;

  // Intended to be called from DetermineEnabled implementations in filters.
  // Returns whether a filter is enabled.
  bool is_enabled() const { return is_enabled_; }

  // Set whether this filter is enabled or not.  Note that a filter
  // may be included in the filter-chain for a configuration, but
  // be disabled for a request based on the request properties, or
  // even due to content (see HtmlParse::set_is_buffered()).
  void set_is_enabled(bool is_enabled) { is_enabled_ = is_enabled; }

  // Invoked by the rewrite driver to query whether this filter will
  // rewrite any urls.
  virtual bool CanModifyUrls() = 0;

  // Note: there is also kRequiresScriptExecutionFilterSet in
  // rewrite_options.cc, which identifies filters that will leave broken
  // pages if javascript is disabled, and hence require noscript handing.
  // The set of filters that CanInjectScripts is larger, as it includes
  // filters that might inject beacons or other optional functionality
  // that is not page-critical.
  virtual ScriptUsage GetScriptUsage() const = 0;

  // The name of this filter -- used for logging and debugging.
  virtual const char* Name() const = 0;

 private:
  bool is_enabled_;
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTML_HTML_FILTER_H_
