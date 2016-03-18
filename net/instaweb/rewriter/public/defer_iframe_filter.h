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

// Author: pulkitg@google.com (Pulkit Goyal)
//
// Contains the implementation of the DeferIframeFilter which defers the iframe
// using JsDeferDisabledJavascriptFilter. This filter should be called before
// JsDeferDisabledJavascriptFilter. This filter renames all the iframe tags to
// pagespeed_iframe and add a script which converts pagespeed_iframe back to
// iframe and the added script is deferred by JsDeferDisabledJavascriptFilter.
//
// Html input to this filter looks like:
// <html>
//  <head>
//  </head>
//  <body>
//   <iframe src="1.html"></iframe>
//  </body>
// </html>
//
// Output for the above html will be:
// <html>
//  <head>
//  </head>
//  <body>
//   <script>
//    defer_iframe script.
//   </script>
//   <pagespeed_iframe src="1.html">
//    <script>
//     Script which changes above pagespeed_iframe tag name to iframe.
//    </script>
//   </pagespeed_iframe>
//  </body>
// </html>
//
// Above script which converts pagespeed_iframe to iframe will be deferred
// by JsDeferDisabledJavascriptFilter, hence loading of iframe is also deferred.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DEFER_IFRAME_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DEFER_IFRAME_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_filter.h"

namespace net_instaweb {

class StaticAssetManager;

class DeferIframeFilter : public CommonFilter {
 public:
  static const char kDeferIframeInit[];
  static const char kDeferIframeIframeJs[];
  explicit DeferIframeFilter(RewriteDriver* driver);
  ~DeferIframeFilter();

  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);
  virtual void DetermineEnabled(GoogleString* disabled_reason);

  virtual const char* Name() const { return "DeferIframe"; }
  ScriptUsage GetScriptUsage() const override { return kWillInjectScripts; }

 private:
  StaticAssetManager* static_asset_manager_;
  bool script_inserted_;

  DISALLOW_COPY_AND_ASSIGN(DeferIframeFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DEFER_IFRAME_FILTER_H_
