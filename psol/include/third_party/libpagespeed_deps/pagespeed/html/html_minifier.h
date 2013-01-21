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

// This class takes an HTML document as input and produces another document as
// output that is equivalent but hopefully smaller in size.

#ifndef PAGESPEED_HTML_HTML_MINIFIER_H_
#define PAGESPEED_HTML_HTML_MINIFIER_H_

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/rewriter/public/collapse_whitespace_filter.h"
#include "net/instaweb/rewriter/public/elide_attributes_filter.h"
#include "net/instaweb/rewriter/public/html_attribute_quote_removal.h"
#include "net/instaweb/rewriter/public/remove_comments_filter.h"
#include "net/instaweb/util/public/message_handler.h"

#include "pagespeed/html/minify_js_css_filter.h"

namespace pagespeed {

namespace html {

class HtmlMinifier {
 public:
  explicit HtmlMinifier();
  ~HtmlMinifier();

  // Return true if successful, false on error.
  bool MinifyHtml(const std::string& input_name,
                  const std::string& input,
                  std::string* output);
  // Same as above, but accept an explicit Content-Type header, which may be
  // used to disambiguate between HTML and XHTML.
  bool MinifyHtmlWithType(const std::string& input_name,
                          const std::string& input_content_type,
                          const std::string& input,
                          std::string* output);

 private:
  scoped_ptr<net_instaweb::MessageHandler> message_handler_;
  net_instaweb::HtmlParse html_parse_;
  net_instaweb::RemoveCommentsFilter remove_comments_filter_;
  net_instaweb::ElideAttributesFilter elide_attributes_filter_;
  net_instaweb::HtmlAttributeQuoteRemoval quote_removal_filter_;
  net_instaweb::CollapseWhitespaceFilter collapse_whitespace_filter_;
  MinifyJsCssFilter minify_js_css_filter_;
  net_instaweb::HtmlWriterFilter html_writer_filter_;

  DISALLOW_COPY_AND_ASSIGN(HtmlMinifier);
};

}  // namespace html

}  // namespace pagespeed

#endif  // PAGESPEED_HTML_HTML_MINIFIER_H_
