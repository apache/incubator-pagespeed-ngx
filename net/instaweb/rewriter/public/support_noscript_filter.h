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

// Author: sriharis@google.com (Srihari Sukumaran)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_SUPPORT_NOSCRIPT_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_SUPPORT_NOSCRIPT_FILTER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/html/empty_html_filter.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;

// Inserts a noscript tag as the first element of body.  This noscript redirects
// to "ModPagespeed=off" to prevent breakage when pages rewritten by filters
// that depend on script execution (such as lazyload_images) are rendered on
// browsers with script execution disabled.
class SupportNoscriptFilter : public EmptyHtmlFilter {
 public:
  explicit SupportNoscriptFilter(RewriteDriver* rewrite_driver);
  virtual ~SupportNoscriptFilter();

  virtual void DetermineEnabled(GoogleString* disabled_reason);

  virtual void StartElement(HtmlElement* element);
  virtual const char* Name() const { return "SupportNoscript"; }

  // Make sure this filter gets turned off when a document is declared as AMP.
  //
  // This is a little confusing; SupportNoscript does not itself
  // inject scripts, but it injects http-equiv tags which prevent
  // AMP-HTML from being validated.  This filter is a special
  // snowflake that is never enabled by users, but is implied by the
  // initial enabling of *other* filters that *do* inject scripts.
  //
  // Because this filter only changes HTML on behalf of filters that are
  // themselves kWillInjectScripts, [falsely] declaring that this script
  // as kWillInjectScripts has no particular downside.
  //
  // TODO(jmarantz): consider an alterantive mechanism that is more intuitive,
  // and doesn't entail GetScriptUsage lying to induce the right amp behavior.
  ScriptUsage GetScriptUsage() const override { return kWillInjectScripts; }

 private:
  bool IsAnyFilterRequiringScriptExecutionEnabled() const;
  RewriteDriver* rewrite_driver_;  // We do not own this.
  bool should_insert_noscript_;

  DISALLOW_COPY_AND_ASSIGN(SupportNoscriptFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SUPPORT_NOSCRIPT_FILTER_H_
