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

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/fast_wildcard_group.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
class HtmlCommentNode;
class HtmlParse;

// Reduce the size of the HTML by removing all HTML comments (except those
// which are IE directives).  Note that this is a potentially dangerous
// optimization; if a site is using comments for some squirrelly purpose, then
// removing those comments might break something.
class RemoveCommentsFilter : public EmptyHtmlFilter {
 public:
  // Interface that allows policy injection into a RemoveCommentsFilter
  // instance. We cannot use RewriteOptions directly here since
  // RemoveCommentsFilter does not want to take on all of the
  // RewriteOptions dependencies.
  class OptionsInterface {
   public:
    OptionsInterface() {}
    virtual ~OptionsInterface();

    // Return true if the given comment should not be removed from the
    // HTML, false otherwise.
    virtual bool IsRetainedComment(const StringPiece& comment) const = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(OptionsInterface);
  };

  // Basic default implementation.
  class OptionsImpl : public OptionsInterface {
   public:
    OptionsImpl() {}

    void RetainComment(const StringPiece& comment) {
      retain_comments_.Allow(comment);
    }

    virtual bool IsRetainedComment(const StringPiece& comment) const {
      return retain_comments_.Match(comment, false);
    }

   private:
    FastWildcardGroup retain_comments_;

    DISALLOW_COPY_AND_ASSIGN(OptionsImpl);
  };

  explicit RemoveCommentsFilter(HtmlParse* html_parse)
      : html_parse_(html_parse),
        options_(NULL) {
  }

  // RemoveCommentsFilter takes ownership of the passed in
  // OptionsInterface instance. It is ok for OptionsInterface to be
  // NULL.
  RemoveCommentsFilter(HtmlParse* html_parse,
                       const OptionsInterface* options)
      : html_parse_(html_parse),
        options_(options) {
  }

  virtual ~RemoveCommentsFilter();

  virtual void Comment(HtmlCommentNode* comment);
  virtual const char* Name() const { return "RemoveComments"; }

 private:
  HtmlParse* html_parse_;
  scoped_ptr<const OptionsInterface> options_;

  DISALLOW_COPY_AND_ASSIGN(RemoveCommentsFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REMOVE_COMMENTS_FILTER_H_
