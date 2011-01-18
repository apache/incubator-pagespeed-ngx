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

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/util/public/atom.h"
#include <string>

namespace net_instaweb {

class MessageHandler;
class ResponseHeaders;
class OutputResource;
class ResourceManager;

// Filter to take explicit <style> and <script> tags and outline them to files.
class JsOutlineFilter : public HtmlFilter {
 public:
  explicit JsOutlineFilter(RewriteDriver* driver);
  static const char kFilterId[];

  virtual void StartDocument();

  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);

  virtual void Flush();

  // HTML Events we expect to be in <script> elements.
  virtual void Characters(HtmlCharactersNode* characters);

  // HTML Events we do not expect to be in <style> and <script> elements.
  virtual void Comment(HtmlCommentNode* comment);
  virtual void Cdata(HtmlCdataNode* cdata);
  virtual void IEDirective(HtmlIEDirectiveNode* directive);

  // Ignored HTML Events.
  virtual void EndDocument() {}
  virtual void Directive(HtmlDirectiveNode* directive) {}

  virtual const char* Name() const { return "OutlineJs"; }

 private:
  bool WriteResource(const std::string& content, OutputResource* resource,
                     MessageHandler* handler);
  void OutlineScript(HtmlElement* element, const std::string& content);

  // The style or script element we are in (if it hasn't been flushed).
  // If we are not in a script or style element, inline_element_ == NULL.
  HtmlElement* inline_element_;
  // Temporarily buffers the content between open and close of inline_element_.
  std::string buffer_;
  HtmlParse* html_parse_;
  ResourceManager* resource_manager_;
  size_t size_threshold_bytes_;
  // HTML strings interned into a symbol table.
  Atom s_script_;
  Atom s_src_;
  Atom s_type_;
  ScriptTagScanner script_tag_scanner_;

  DISALLOW_COPY_AND_ASSIGN(JsOutlineFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JS_OUTLINE_FILTER_H_
