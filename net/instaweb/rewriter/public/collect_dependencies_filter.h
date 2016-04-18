/*
 * Copyright 2016 Google Inc.
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
// Author: morlovich@google.com (Maksim Orlovich)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_COLLECT_DEPENDENCIES_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_COLLECT_DEPENDENCIES_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/html/html_element.h"

namespace net_instaweb {

// This observes resources in the HTML page, including how they are rewritten,
// and passes info on them on to DependencyTracker.
class CollectDependenciesFilter : public CommonFilter {
 public:
  explicit CollectDependenciesFilter(RewriteDriver* driver);

  void StartDocumentImpl() override;
  void StartElementImpl(HtmlElement* element) override;
  void EndElementImpl(HtmlElement* element) override;

  const char* Name() const override {
    return "Collect Dependencies Filter";
  }

 private:
  class Context;

  DISALLOW_COPY_AND_ASSIGN(CollectDependenciesFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_COLLECT_DEPENDENCIES_FILTER_H_
