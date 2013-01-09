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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_HANDLE_NOSCRIPT_REDIRECT_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_HANDLE_NOSCRIPT_REDIRECT_FILTER_H_

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;

// This filter applies only for requests that are redirects to
// "?ModPagespeed=noscript".  It inserts a "<link rel=canonical href="URL
// without the query param" >" element in the head.
// TODO(sriharis): Set a cookie so that subsequent requests from the same client
// do not cause redirects.
class HandleNoscriptRedirectFilter : public EmptyHtmlFilter {
 public:
  explicit HandleNoscriptRedirectFilter(RewriteDriver* rewrite_driver);
  virtual ~HandleNoscriptRedirectFilter();

  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);
  virtual const char* Name() const { return "HandleNoscriptRedirect"; }

 private:
  void Init();

  RewriteDriver* rewrite_driver_;  // We do not own this.
  bool canonical_present_;
  bool canonical_inserted_;

  DISALLOW_COPY_AND_ASSIGN(HandleNoscriptRedirectFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_HANDLE_NOSCRIPT_REDIRECT_FILTER_H_
