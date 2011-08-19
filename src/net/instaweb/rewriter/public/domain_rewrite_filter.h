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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DOMAIN_REWRITE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DOMAIN_REWRITE_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class HtmlElement;
class GoogleUrl;
class RewriteDriver;
class Statistics;
class Variable;

// Filter that rewrites URL domains for resources that are not
// otherwise rewritten.  For example, the user may want to
// domain-shard adding a hash to their URL leaves, or domain shard
// resources that are not cacheable.
class DomainRewriteFilter : public CommonFilter {
 public:
  DomainRewriteFilter(RewriteDriver* rewrite_driver, Statistics* stats);
  ~DomainRewriteFilter();
  static void Initialize(Statistics* statistics);
  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element) {}

  virtual const char* Name() const { return "DomainRewrite"; }

  // Rewrites the specified URL (which might be relative to the base tag)
  // into an absolute sharded url.
  bool Rewrite(const StringPiece& input_url, const GoogleUrl& base_url,
               GoogleString* output_url);

 private:
  ResourceTagScanner tag_scanner_;
  // Stats on how much domain-rewriting we've done.
  Variable* rewrite_count_;

  DISALLOW_COPY_AND_ASSIGN(DomainRewriteFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DOMAIN_REWRITE_FILTER_H_
