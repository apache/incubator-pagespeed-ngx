/*
 * Copyright 2011 Google Inc.
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
// Author: atulvasu@google.com (Atul Vasu)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_JS_DEFER_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_JS_DEFER_FILTER_H_

#include <vector>

#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"

namespace net_instaweb {

class HtmlParse;
class HtmlElement;
class HtmlCharactersNode;

// Implements deferring of javascripts into post onload.
// Essentially inline scripts will be replaced with
//  mod_pagespeed_defer_str("inline_script()");
//
// And external scripts will be replaced by:
//  mod_pagespeed_defer_url("http://url_to_resource_after_rewrite");
//
// These scripts will be added at the end of the body tag in which the
// script node occurred.
class JsDeferFilter : public EmptyHtmlFilter {
 public:
  typedef std::vector<HtmlCharactersNode*> HtmlCharNodeVector;
  static const char* kDeferJsCode;

  explicit JsDeferFilter(HtmlParse* html_parse);
  virtual ~JsDeferFilter();

  virtual void StartDocument();
  virtual void Characters(HtmlCharactersNode* characters);
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);
  virtual void EndDocument();
  virtual void Flush();
  virtual const char* Name() const { return "Defer Javascript"; }

 private:
  inline void CompleteScriptInProgress();
  inline void RewriteInlineScript();
  inline void RewriteExternalScript();
  inline void AddDeferJsFunc(const StringPiece& func, const StringPiece& arg);
  StringPiece FlattenBuffer(GoogleString* script_buffer);

  HtmlParse* html_parse_;
  HtmlCharNodeVector buffer_;
  HtmlElement* script_in_progress_;
  HtmlElement::Attribute* script_src_;
  ScriptTagScanner script_tag_scanner_;
  // The script that will be inlined at the end of BODY.
  GoogleString defer_js_;

  DISALLOW_COPY_AND_ASSIGN(JsDeferFilter);

};

} // namespace net_instaweb
#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JS_DEFER_FILTER_H_
