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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_FILTER_H_

#include <vector>

#include "base/basictypes.h"
#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"
#include "net/instaweb/util/public/atom.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace Css {

class Stylesheet;

}  // namespace Css

namespace net_instaweb {

class HtmlParse;
class MessageHandler;
class OutputResource;
class Resource;
class ResourceManager;

// Find and parse all CSS in the page and apply transformations including:
// minification, combining, refactoring, and optimizing sub-resources.
//
// Currently only does basic minification.
//
// Note that CssCombineFilter currently does combining (although there is a bug)
// but CssFilter will eventually replace this.
//
// Currently only deals with inline <style> tags and external <link> resources.
// It does not consider style= attributes on arbitrary elements.
class CssFilter : public RewriteSingleResourceFilter {
 public:
  CssFilter(RewriteDriver* driver, const StringPiece& filter_prefix);

  static void Initialize(Statistics* statistics);
  static void Terminate();

  // Note: AtExitManager needs to be initialized or you get a nasty error:
  // Check failed: false. Tried to RegisterCallback without an AtExitManager.
  // This is called by Initialize.
  static void InitializeAtExitManager();

  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual void EndElementImpl(HtmlElement* element);

  virtual const char* Name() const { return "CssFilter"; }

  static const char kFilesMinified[];
  static const char kMinifiedBytesSaved[];
  static const char kParseFailures[];

 private:
  bool RewriteCssText(const StringPiece& in_text, std::string* out_text,
                      const std::string& id, MessageHandler* handler);
  bool RewriteExternalCss(const StringPiece& in_url, std::string* out_url);
  bool RewriteExternalCssToResource(Resource* input_resource,
                                    OutputResource* output_resource);

  virtual bool RewriteLoadedResource(const Resource* input_resource,
                                     OutputResource* output_resource);

  Css::Stylesheet* CombineStylesheets(
      std::vector<Css::Stylesheet*>* stylesheets);
  bool LoadAllSubStylesheets(Css::Stylesheet* stylesheet_with_imports,
                             std::vector<Css::Stylesheet*>* result_stylesheets);

  Css::Stylesheet* LoadStylesheet(const StringPiece& url) { return NULL; }

  HtmlParse* html_parse_;
  ResourceManager* resource_manager_;

  bool in_style_element_;  // Are we in a style element?
  // These are meaningless if in_style_element_ is false:
  HtmlElement* style_element_;  // The element we are in.
  HtmlCharactersNode* style_char_node_;  // The single character node in style.

  Atom s_style_;
  Atom s_link_;
  Atom s_rel_;
  Atom s_href_;

  // Statistics
  Variable* num_files_minified_;
  Variable* minified_bytes_saved_;
  Variable* num_parse_failures_;

  DISALLOW_COPY_AND_ASSIGN(CssFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_FILTER_H_
