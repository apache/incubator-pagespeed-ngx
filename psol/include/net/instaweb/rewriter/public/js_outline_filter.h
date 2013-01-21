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

// Author: sligocki@google.com (Shawn Ligocki)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_JS_OUTLINE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_JS_OUTLINE_FILTER_H_

#include <cstddef>

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {
class HtmlCharactersNode;
class HtmlElement;
class MessageHandler;
class OutputResource;
class ServerContext;
class RewriteDriver;

// Filter to take explicit <style> and <script> tags and outline them to files.
class JsOutlineFilter : public CommonFilter {
 public:
  explicit JsOutlineFilter(RewriteDriver* driver);
  virtual ~JsOutlineFilter();
  static const char kFilterId[];

  virtual void StartDocumentImpl();

  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);

  virtual void Flush();

  // HTML Events we expect to be in <script> elements.
  virtual void Characters(HtmlCharactersNode* characters);

  virtual const char* Name() const { return "OutlineJs"; }

 private:
  bool WriteResource(const GoogleString& content, OutputResource* resource,
                     MessageHandler* handler);
  void OutlineScript(HtmlElement* element, const GoogleString& content);

  // The script element we are in (if it hasn't been flushed).
  // If we are not in a script element, inline_element_ == NULL.
  HtmlElement* inline_element_;
  // CharactersNode to be outlined.
  HtmlCharactersNode* inline_chars_;
  ServerContext* server_context_;
  size_t size_threshold_bytes_;
  ScriptTagScanner script_tag_scanner_;

  DISALLOW_COPY_AND_ASSIGN(JsOutlineFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JS_OUTLINE_FILTER_H_
