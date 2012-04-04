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

// Author: guptaa@google.com (Ashish Gupta)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_STATIC_JAVASCRIPT_MANAGER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_STATIC_JAVASCRIPT_MANAGER_H_

#include <vector>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class RewriteOptions;
class UrlNamer;

// Composes URLs for the javascript files injected by the various PSA filters.
class StaticJavascriptManager {
 public:
  static const char kGStaticBase[];
  static const char kBlinkGstaticSuffix[];
  static const char kBlinkRelativePath[];

  enum JsModule {
    kDeferJs,
    kDelayImagesJs,
    kDelayImagesInlineJs,
    kLazyloadImagesJs,
    kDetectReflowJs,
    kEndOfModules,  // Keep this as the last enum value.
  };

  StaticJavascriptManager(
      UrlNamer* url_namer,
      bool serve_js_from_gstatic,
      const GoogleString& blink_hash);

  ~StaticJavascriptManager();

  // Returns the blink js url based on the value of debug filter and the value
  // of serve_js_from_gstatic flag.
  const GoogleString& GetBlinkJsUrl(const RewriteOptions* options) const;

  const char* GetJsSnippet(JsModule module,
                           const RewriteOptions* options);

 private:
  typedef std::vector<const char*> StaticJsVector;
  // Uses enum JsModule as the key.
  StaticJsVector opt_js_vector_;
  StaticJsVector debug_js_vector_;

  // Composes the URL for blink javascript.
  void InitBlink(const GoogleString& hash);

  void InitializeJsStrings();

  // Set in the constructor, this class does not own this object.
  UrlNamer* url_namer_;
  bool serve_js_from_gstatic_;
  GoogleString blink_javascript_gstatic_url_;
  GoogleString blink_javascript_handler_url_;

  DISALLOW_COPY_AND_ASSIGN(StaticJavascriptManager);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_STATIC_JAVASCRIPT_MANAGER_H_
