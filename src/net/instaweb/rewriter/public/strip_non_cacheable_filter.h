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

// Author: rahulbansal@google.com (Rahul Bansal)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_STRIP_NON_CACHEABLE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_STRIP_NON_CACHEABLE_FILTER_H_

#include <map>
#include <vector>

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/rewriter/public/blink_util.h"

namespace net_instaweb {

class RewriteDriver;
class RewriteOptions;

// This class strips off the non cacheable content from the html. It is assumed
// that the entire html would be in the same flush window so its safe to delete
// elements.
class StripNonCacheableFilter : public EmptyHtmlFilter {
 public:
  explicit StripNonCacheableFilter(RewriteDriver* rewrite_driver);
  virtual ~StripNonCacheableFilter();

  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual const char* Name() const { return "StripNonCacheableFilter"; }

 private:
  RewriteDriver* rewrite_driver_;
  const RewriteOptions* rewrite_options_;
  AttributesToNonCacheableValuesMap attribute_non_cacheable_values_map_;
  std::vector<int> panel_number_num_instances_;

  void InsertPanelStub(HtmlElement* element, const GoogleString& panel_id);

  DISALLOW_COPY_AND_ASSIGN(StripNonCacheableFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_STRIP_NON_CACHEABLE_FILTER_H_
