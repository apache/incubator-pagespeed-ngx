// Copyright 2010 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PAGESPEED_HTML_MINIFY_JS_CSS_FILTER_H_
#define PAGESPEED_HTML_MINIFY_JS_CSS_FILTER_H_

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"

namespace pagespeed {

namespace html {

class MinifyJsCssFilter : public net_instaweb::EmptyHtmlFilter {
 public:
  explicit MinifyJsCssFilter(net_instaweb::HtmlParse* html_parse);

  virtual void Characters(net_instaweb::HtmlCharactersNode* characters);
  virtual const char* Name() const { return "MinifyJsCss"; }

 private:
  net_instaweb::HtmlParse* html_parse_;

  DISALLOW_COPY_AND_ASSIGN(MinifyJsCssFilter);
};

}  // namespace html

}  // namespace pagespeed

#endif  // PAGESPEED_HTML_MINIFY_JS_CSS_FILTER_H_
