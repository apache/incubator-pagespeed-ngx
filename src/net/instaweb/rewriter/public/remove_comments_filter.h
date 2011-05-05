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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REMOVE_COMMENTS_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REMOVE_COMMENTS_FILTER_H_

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {
class HtmlCommentNode;
class HtmlParse;
class RewriteOptions;

// Reduce the size of the HTML by removing all HTML comments (except those
// which are IE directives).  Note that this is a potentially dangerous
// optimization; if a site is using comments for some squirrelly purpose, then
// removing those comments might break something.
class RemoveCommentsFilter : public EmptyHtmlFilter {
 public:
  explicit RemoveCommentsFilter(HtmlParse* html_parse)
      : html_parse_(html_parse),
        rewrite_options_(NULL) {
  }

  RemoveCommentsFilter(HtmlParse* html_parse, const RewriteOptions* options)
      : html_parse_(html_parse),
        rewrite_options_(options) {
  }

  virtual void Comment(HtmlCommentNode* comment);
  virtual const char* Name() const { return "RemoveComments"; }

 private:
  HtmlParse* html_parse_;
  const RewriteOptions* rewrite_options_;

  DISALLOW_COPY_AND_ASSIGN(RemoveCommentsFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REMOVE_COMMENTS_FILTER_H_
