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
//
// This implements a filter which generate HTTP2 push or preload fetch hints.
// (e.g. Link: <foo>; rel=preload HTTP headers)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_PUSH_PRELOAD_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_PUSH_PRELOAD_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/html/html_element.h"

namespace net_instaweb {

class PushPreloadFilter : public CommonFilter {
 public:
  explicit PushPreloadFilter(RewriteDriver* rewrite_driver);
  ~PushPreloadFilter() override;

  // TODO(morlovich): Proper statistics.
  // static void InitStats(Statistics* stats);

  void StartDocumentImpl() override;
  void StartElementImpl(HtmlElement* element) override {}
  void EndElementImpl(HtmlElement* element) override {}

  const char* Name() const override { return "PushPreload"; }

  void DetermineEnabled(GoogleString* disabled_reason) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PushPreloadFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_PUSH_PRELOAD_FILTER_H_
