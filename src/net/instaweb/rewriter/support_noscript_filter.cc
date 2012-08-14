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
// Author: sriharis@google.com (Srihari Sukumaran)

#include "net/instaweb/rewriter/public/support_noscript_filter.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

SupportNoscriptFilter::SupportNoscriptFilter(RewriteDriver* rewrite_driver)
    : rewrite_driver_(rewrite_driver),
      noscript_inserted_(false) {
}

SupportNoscriptFilter::~SupportNoscriptFilter() {
}

void SupportNoscriptFilter::StartDocument() {
  noscript_inserted_ = false;
}

void SupportNoscriptFilter::StartElement(HtmlElement* element) {
  if (!noscript_inserted_ && element->keyword() == HtmlName::kBody) {
    scoped_ptr<GoogleUrl> url_with_psa_off(
        rewrite_driver_->google_url().CopyAndAddQueryParam(
            RewriteQuery::kModPagespeed, RewriteQuery::kNoscriptValue));
    GoogleString url_str(url_with_psa_off->Spec().data(),
                         url_with_psa_off->Spec().size());
    HtmlCharactersNode* noscript_node = rewrite_driver_->NewCharactersNode(
        element, StringPrintf(kNoScriptRedirectFormatter,
                              url_str.c_str(), url_str.c_str()));
    rewrite_driver_->PrependChild(element, noscript_node);
    noscript_inserted_ = true;
  }
  // TODO(sriharis):  Handle the case where there is no body -- insert a body in
  // EndElement of kHtml?
}

}  // namespace net_instaweb
