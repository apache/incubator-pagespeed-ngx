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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_COMBINE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_COMBINE_FILTER_H_

#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/util/url_multipart_encoder.h"

namespace net_instaweb {

class HtmlElement;
class HtmlIEDirectiveNode;
class RewriteContext;
class RewriteDriver;
class Statistics;
class UrlSegmentEncoder;
class Variable;

class CssCombineFilter : public RewriteFilter {
 public:
  // Statistic names:
  // # of CSS links which could ideally have been reduced (# CSS links on
  // original page - 1 for each page).
  static const char kCssCombineOpportunities[];
  // CSS file reduction (Optimally this equals kCssCombineOpportunities).
  static const char kCssFileCountReduction[];

  explicit CssCombineFilter(RewriteDriver* rewrite_driver);
  virtual ~CssCombineFilter();

  static void InitStats(Statistics* statistics);

  virtual void StartDocumentImpl();
  virtual void EndDocument();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element) {}
  virtual void Flush();
  virtual void DetermineEnabled(GoogleString* disabled_reason);
  virtual void IEDirective(HtmlIEDirectiveNode* directive);
  virtual const char* Name() const { return "CssCombine"; }
  virtual const UrlSegmentEncoder* encoder() const {
    return &multipart_encoder_;
  }

  virtual RewriteContext* MakeRewriteContext();
  virtual const char* id() const { return RewriteOptions::kCssCombinerId; }

 private:
  class Context;
  class CssCombiner;

  CssCombiner* combiner();
  void NextCombination(StringPiece debug_help);
  Context* MakeContext();

  scoped_ptr<Context> context_;
  UrlMultipartEncoder multipart_encoder_;
  bool end_document_found_;
  int css_links_;  // # of CSS <link>s found on this page.

  Variable* css_combine_opportunities_;

  DISALLOW_COPY_AND_ASSIGN(CssCombineFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_COMBINE_FILTER_H_
