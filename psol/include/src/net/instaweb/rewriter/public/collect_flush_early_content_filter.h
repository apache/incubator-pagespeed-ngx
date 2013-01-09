/*
 * Copyright 2012 Google Inc.
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
// Author: nikhilmadan@google.com (Nikhil Madan)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_COLLECT_FLUSH_EARLY_CONTENT_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_COLLECT_FLUSH_EARLY_CONTENT_FILTER_H_

#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;

// CollectFlushEarlyContentFilter extracts the html for non-inlined resources
// that we want to flush early
// and stores it in property cache to be used by FlushEarlyFlow. If a request is
// flushed early then this HTML is used to make the client download resources
// early.
// pagespeed_size attribute is added to the html elements which are being
// flushed in the flush early flow. This information will be used by
// FlushEarlyContentWriterFilter to flush more resources based on origin server
// fetch latency.
class CollectFlushEarlyContentFilter : public RewriteFilter {
 public:
  explicit CollectFlushEarlyContentFilter(RewriteDriver* driver);

  virtual void StartDocumentImpl();
  virtual void EndDocument();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);

  virtual const char* Name() const {
    return "Collect Flush Early Content Filter";
  }
  virtual const char* id() const {
    return RewriteOptions::kCollectFlushEarlyContentFilterId;
  }

 protected:
  virtual void Clear();

 private:
  class Context;

  void AppendToHtml(StringPiece url, semantic_type::Category category,
                    HtmlElement* element);
  void AppendAttribute(HtmlName::Keyword keyword, HtmlElement* element);
  // Enable writing of the property cache DOM cohort in the RewriteDriver.
  virtual bool UsesPropertyCacheDomCohort() const { return true; }

  // It will be false if there is no resource to flush early.
  bool found_resource_;
  GoogleString resource_html_;  // The html text containing resource elements.

  DISALLOW_COPY_AND_ASSIGN(CollectFlushEarlyContentFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_COLLECT_FLUSH_EARLY_CONTENT_FILTER_H_
