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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_INLINE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_INLINE_FILTER_H_

#include <cstddef>

#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CachedResult;
class GoogleUrl;
class HtmlElement;
class Statistics;
class Variable;

// Inline small CSS files.
class CssInlineFilter : public CommonFilter {
 public:
  static const char kNumCssInlined[];

  explicit CssInlineFilter(RewriteDriver* driver);
  virtual ~CssInlineFilter();

  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element);
  virtual const char* Name() const { return "InlineCss"; }
  // Inlining css from unauthorized domains into HTML is considered
  // safe because it does not cause any new content to be executed compared
  // to the unoptimized page.
  virtual RewriteDriver::InlineAuthorizationPolicy AllowUnauthorizedDomain()
      const {
    return driver()->options()->HasInlineUnauthorizedResourceType(
               semantic_type::kStylesheet) ?
           RewriteDriver::kInlineUnauthorizedResources :
           RewriteDriver::kInlineOnlyAuthorizedResources;
  }
  virtual bool IntendedForInlining() const { return true; }

  static void InitStats(Statistics* statistics);
  static bool HasClosingStyleTag(StringPiece contents);

 protected:
  // Changes filter id code (which shows up in cache keys and
  // .pagespeed.id. URLs). Expects id to be a literal.
  void set_id(const char* id) { id_ = id; }

  // Delegated from InlineRewriteContext::CreateResource --- see there
  // for semantics.
  virtual ResourcePtr CreateResource(const char* url, bool* is_authorized);

 private:
  class Context;
  friend class Context;

  bool ShouldInline(const ResourcePtr& resource,
                    const StringPiece& attrs_attribute,
                    GoogleString* reason) const;
  void RenderInline(const ResourcePtr& resource, const CachedResult& cached,
                    const GoogleUrl& base_url, const StringPiece& text,
                    HtmlElement* element);

  const char* id_;  // filter ID code.
  const size_t size_threshold_bytes_;

  GoogleString domain_;
  CssTagScanner css_tag_scanner_;

  Variable* num_css_inlined_;

  DISALLOW_COPY_AND_ASSIGN(CssInlineFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_INLINE_FILTER_H_
