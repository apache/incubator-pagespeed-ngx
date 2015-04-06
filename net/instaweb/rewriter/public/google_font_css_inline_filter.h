/*
 * Copyright 2013 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)
//
// Filter that inlines small loader CSS files made by Google Font Service.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_GOOGLE_FONT_CSS_INLINE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_GOOGLE_FONT_CSS_INLINE_FILTER_H_

#include "net/instaweb/rewriter/public/css_inline_filter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

class GoogleUrl;
class RewriteDriver;
class Statistics;

class GoogleFontCssInlineFilter : public CssInlineFilter {
 public:
  // Note: this also registers our resource url claimant with the driver.
  explicit GoogleFontCssInlineFilter(RewriteDriver* driver);
  virtual ~GoogleFontCssInlineFilter();

  static void InitStats(Statistics* statistics);

  virtual const char* Name() const { return "InlineGoogleFontCss"; }

 protected:
  virtual ResourcePtr CreateResource(const char* url, bool* is_authorized);

 private:
  void ResetAndExplainReason(const char* reason, ResourcePtr* resource);
  void CheckIfFontServiceUrl(const GoogleUrl& url, bool* result);

  DISALLOW_COPY_AND_ASSIGN(GoogleFontCssInlineFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_GOOGLE_FONT_CSS_INLINE_FILTER_H_
