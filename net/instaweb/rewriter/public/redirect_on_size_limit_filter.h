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

// Author: nikhilmadan@google.com (Nikhil Madan)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REDIRECT_ON_SIZE_LIMIT_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REDIRECT_ON_SIZE_LIMIT_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_filter.h"

namespace net_instaweb {

// Inserts a redirect to "ModPagespeed=off" if we have exceeded the limit on the
// maximum number of bytes that should be parsed.
class RedirectOnSizeLimitFilter : public CommonFilter {
 public:
  explicit RedirectOnSizeLimitFilter(RewriteDriver* rewrite_driver);
  virtual ~RedirectOnSizeLimitFilter();

  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);

  virtual const char* Name() const { return "RedirectOnSizeLimit"; }
  ScriptUsage GetScriptUsage() const override { return kWillInjectScripts; }

 private:
  void InsertScriptIfNeeded(HtmlElement* element, bool is_start);

  bool redirect_inserted_;

  DISALLOW_COPY_AND_ASSIGN(RedirectOnSizeLimitFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REDIRECT_ON_SIZE_LIMIT_FILTER_H_
