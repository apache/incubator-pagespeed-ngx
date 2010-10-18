/**
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

// Author: mdsteele@google.com (Matthew D. Steele)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_JS_INLINE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_JS_INLINE_FILTER_H_

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/util/public/atom.h"
#include <string>

namespace net_instaweb {

class ResourceManager;
class RewriteDriver;

// Inline small Javascript files.
class JsInlineFilter : public EmptyHtmlFilter {
 public:
  JsInlineFilter(HtmlParse* html_parse,
                 ResourceManager* resource_manager,
                 size_t size_threshold_bytes);
  virtual ~JsInlineFilter();

  virtual void StartDocument();
  virtual void EndDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual const char* Name() const { return "InlineJs"; }

 private:
  HtmlParse* const html_parse_;
  ResourceManager* const resource_manager_;
  const Atom script_atom_;
  const Atom src_atom_;
  const size_t size_threshold_bytes_;

  std::string domain_;  // The domain of the HTML file we're parsing

  // This is set to true during StartElement() for a <script> tag that we
  // should maybe inline, but may be set back to false by Characters().  If it
  // is still true when we hit the corresponding EndElement(), then we'll
  // inline the script (and set it back to false).  It should never be true
  // outside of <script> and </script>.
  bool should_inline_;

  DISALLOW_COPY_AND_ASSIGN(JsInlineFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JS_INLINE_FILTER_H_
