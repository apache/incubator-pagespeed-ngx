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
// Author: bharathbhushan@google.com (Bharath Bhushan)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_INSERT_DNS_PREFETCH_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_INSERT_DNS_PREFETCH_FILTER_H_

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"  // for StringSet, etc

namespace net_instaweb {

class FlushEarlyInfo;
class RewriteDriver;

// Injects <link rel="dns-prefetch" href="//www.example.com"> tags in the HEAD
// to enable the browser to do DNS prefetching.
class InsertDnsPrefetchFilter : public CommonFilter {
 public:
  explicit InsertDnsPrefetchFilter(RewriteDriver* driver);
  virtual ~InsertDnsPrefetchFilter();

  virtual void StartDocumentImpl();
  virtual void EndDocument();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);

  virtual const char* Name() const { return "InsertDnsPrefetchFilter"; }
  virtual const char* id() const { return "idp"; }

 private:
  void Clear();

  // Add a domain found in the page to the list of domains for which DNS
  // prefetch tags can be inserted.
  void MarkAlreadyInHead(HtmlElement::Attribute* urlattr);

  // Returns true if the list of domains for DNS prefetch tags is "stable".
  // Refer to the implementation for details about stability. This filter will
  // insert the tags into the HEAD once the list is stable.
  bool IsDomainListStable(const FlushEarlyInfo& flush_early_info) const;
  void DebugPrint(const char* msg);

  // This flag is useful if multiple HEADs are present. This filter inserts the
  // DNS prefetch tags only in the first HEAD.
  bool dns_prefetch_inserted_;

  // This flag indicates if we are currently processing elements in HEAD.
  bool in_head_;

  // The set of domains seen in resource links in HEAD.
  StringSet domains_in_head_;

  // The set of domains seen in resource links in BODY and not already seen in
  // HEAD.
  StringSet domains_in_body_;

  // The list of domains for which DNS prefetch tags can be inserted, in the
  // order they were seen in BODY.
  StringVector dns_prefetch_domains_;

  DISALLOW_COPY_AND_ASSIGN(InsertDnsPrefetchFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_INSERT_DNS_PREFETCH_FILTER_H_
