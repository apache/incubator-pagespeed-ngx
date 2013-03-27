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

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CachedResult;
class GoogleUrl;
class HtmlElement;
class RewriteDriver;

// Inline small CSS files.
class CssInlineFilter : public CommonFilter {
 public:
  explicit CssInlineFilter(RewriteDriver* driver);
  virtual ~CssInlineFilter();

  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element);
  virtual const char* Name() const { return "InlineCss"; }

 private:
  class Context;
  friend class Context;

  bool ContainsNonStandardAttributes(const HtmlElement* element);
  bool ShouldInline(const ResourcePtr& resource,
                    const StringPiece& attrs_attribute) const;
  void RenderInline(const ResourcePtr& resource, const CachedResult& cached,
                    const GoogleUrl& base_url, const StringPiece& text,
                    HtmlElement* element);

  const size_t size_threshold_bytes_;

  GoogleString domain_;
  CssTagScanner css_tag_scanner_;

  DISALLOW_COPY_AND_ASSIGN(CssInlineFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_INLINE_FILTER_H_
