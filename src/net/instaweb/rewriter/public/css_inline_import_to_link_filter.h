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

// Author: matterbury@google.com (Matt Atterbury)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_INLINE_IMPORT_TO_LINK_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_INLINE_IMPORT_TO_LINK_FILTER_H_

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_parser_types.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class RewriteDriver;
class Statistics;
class Variable;

// Filter to rewrite style tags of the form:
//   <style type="text/css" ...>@import url(URL) ;</style>
// to
//   <link type="text/css" ... rel="stylesheet" href="URL"/>
//
class CssInlineImportToLinkFilter : public EmptyHtmlFilter {
 public:
  explicit CssInlineImportToLinkFilter(RewriteDriver* driver,
                                       Statistics* statistics);
  virtual ~CssInlineImportToLinkFilter();

  static void InitStats(Statistics* statistics);

  virtual void StartDocument();
  virtual void EndDocument();

  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);

  virtual void Flush();

  // HTML Events we expect to be in a <style> element.
  virtual void Characters(HtmlCharactersNode* characters);

  virtual const char* Name() const { return "InlineImportToLinkCss"; }

 private:
  void ResetState();
  void InlineImportToLinkStyle();

  RewriteDriver* driver_;
  // The style element we are in (if it hasn't been flushed).
  // If we are not in a style element, style_element_ == NULL.
  HtmlElement* style_element_;
  // The characters inside the style element we are in.
  HtmlCharactersNode* style_characters_;
  // Statistics count of the number of times we rewrite a style element.
  Variable* counter_;

  DISALLOW_COPY_AND_ASSIGN(CssInlineImportToLinkFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_INLINE_IMPORT_TO_LINK_FILTER_H_
