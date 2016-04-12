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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_INSERT_AMP_LINK_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_INSERT_AMP_LINK_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_filter.h"

namespace net_instaweb {

// Inserts a <link rel=amphtml> link
class InsertAmpLinkFilter : public CommonFilter {
 public:
  explicit InsertAmpLinkFilter(RewriteDriver* driver);
  ~InsertAmpLinkFilter() override;

  void StartDocumentImpl() override;
  void StartElementImpl(HtmlElement* element) override;
  void EndElementImpl(HtmlElement* element) override;
  void DetermineEnabled(GoogleString* disabled_reason) override;

  const char* Name() const override { return "InsertAmpLink"; }

  // TODO(jmarantz): Make an explicit filter method declaring this filter as
  // something we don't want to run in an AMP document.  In the meantime, we
  // can induce disabling for AMP documents with this:
  ScriptUsage GetScriptUsage() const override { return kWillInjectScripts; }

 private:
  // Returns the AMP URL to insert into the link tag.
  GoogleString GetAmpUrl();
  // True if the filter is enabled
  bool enabled_;
  // True if an AMP link has already been found in the document.
  bool amp_link_found_;

  DISALLOW_COPY_AND_ASSIGN(InsertAmpLinkFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_INSERT_AMP_LINK_FILTER_H_
