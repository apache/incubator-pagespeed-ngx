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

// Author: mdsteele@google.com (Matthew D. Steele)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_COLLAPSE_WHITESPACE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_COLLAPSE_WHITESPACE_FILTER_H_

#include <vector>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/util/public/atom.h"

namespace net_instaweb {

// Reduce the size of the HTML by collapsing whitespace (except within certain
// tags, e.g. <pre> and <script>).  Note that this is a dangerous filter, as
// CSS can be used to make the HTML whitespace-sensitive in unpredictable
// places; thus, it should only be used for content that you are sure will not
// do this.
//
// TODO(mdsteele): Use the CSS parser (once it's finished) to try to
// intelligently determine when the CSS "white-space: pre" property is in use;
// that would make this filter much safer.
class CollapseWhitespaceFilter : public EmptyHtmlFilter {
 public:
  explicit CollapseWhitespaceFilter(HtmlParse* html_parse);

  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);
  virtual void Characters(HtmlCharactersNode* characters);
  virtual const char* Name() const { return "CollapseWhitespace"; }

 private:
  HtmlParse* html_parse_;
  std::vector<HtmlName::Keyword> keyword_stack_;

  DISALLOW_COPY_AND_ASSIGN(CollapseWhitespaceFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_COLLAPSE_WHITESPACE_FILTER_H_
