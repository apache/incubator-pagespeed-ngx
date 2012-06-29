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

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/util/public/basictypes.h"

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

  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual const char* Name() const { return "SupportNoscript"; }

 private:
  RewriteDriver* rewrite_driver_;  // We do not own this.
  bool noscript_inserted_;

  DISALLOW_COPY_AND_ASSIGN(SupportNoscriptFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SUPPORT_NOSCRIPT_FILTER_H_
