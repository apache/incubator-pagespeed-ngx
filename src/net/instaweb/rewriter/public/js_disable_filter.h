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

// Author: gagansingh@google.com (Gagan Singh)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_JS_DISABLE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_JS_DISABLE_FILTER_H_

#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;
class RewriteOptions;

// Disables javscript by converting input html:
//   <script src="1.js">var a = 1...</script>
// to:
//   <noscript disabled="true">
//     <script src="1.js">var a = 1...</script>
//   </noscript>
//
class JsDisableFilter : public CommonFilter {
 public:
  explicit JsDisableFilter(RewriteDriver* driver);
  ~JsDisableFilter();

  static const char kEnableJsExperimental[];
  static const char kElementOnloadCode[];

  virtual void DetermineEnabled();

  virtual const char* Name() const {
    return "JsDisableFilter";
  }

 private:
  virtual void StartDocumentImpl();

  virtual void StartElementImpl(HtmlElement* element);

  virtual void EndElementImpl(HtmlElement* element);

  virtual void EndDocument();

  // Inserts the experimental js enable/disable code.
  void InsertJsDeferExperimentalScript();

  // Insert meta tag with 'X-UA-Compatible'. This will avoid IE going to quirks
  // mode. More information about this can be found in
  // http://webdesign.about.com/od/metataglibraries/p/x-ua-compatible-meta-tag.htm
  void InsertMetaTagForIE(HtmlElement* element);

  RewriteDriver* rewrite_driver_;
  ScriptTagScanner script_tag_scanner_;
  int index_;
  bool ie_meta_tag_written_;
  int prefetch_js_elements_count_;
  int max_prefetch_js_elements_;
  GoogleString prefetch_js_elements_;
  bool should_look_for_prefetch_js_elements_;
  UserAgentMatcher::PrefetchMechanism prefetch_mechanism_;

  DISALLOW_COPY_AND_ASSIGN(JsDisableFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JS_DISABLE_FILTER_H_
