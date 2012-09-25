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

#include <set>                          // for _Rb_tree_const_iterator, etc

#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_keywords.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

SupportNoscriptFilter::SupportNoscriptFilter(RewriteDriver* rewrite_driver)
    : rewrite_driver_(rewrite_driver),
      should_insert_noscript_(true) {
}

SupportNoscriptFilter::~SupportNoscriptFilter() {
}

void SupportNoscriptFilter::StartDocument() {
  // Insert a NOSCRIPT tag only if at least one of the filters requiring
  // JavaScript for execution is enabled.
  if (IsAnyFilterRequiringScriptExecutionEnabled()) {
    should_insert_noscript_ = true;
  } else {
    should_insert_noscript_ = false;
  }
}

void SupportNoscriptFilter::StartElement(HtmlElement* element) {
  if (rewrite_driver_->options()->Enabled(
      RewriteOptions::kProcessBlinkInBackground)) {
    // Lazyload filter can be enabled for blink requests and hence this filter
    // will get attached.
    return;
  }
  if (should_insert_noscript_ && element->keyword() == HtmlName::kBody) {
    scoped_ptr<GoogleUrl> url_with_psa_off(
        rewrite_driver_->google_url().CopyAndAddQueryParam(
            RewriteQuery::kModPagespeed, RewriteQuery::kNoscriptValue));
    GoogleString escaped_url;
    HtmlKeywords::Escape(url_with_psa_off->Spec(), &escaped_url);
    // TODO(sriharis): Replace the usage of HtmlCharactersNode with HtmlElement
    // and Attribute.
    HtmlCharactersNode* noscript_node = rewrite_driver_->NewCharactersNode(
        element, StringPrintf(kNoScriptRedirectFormatter,
                              escaped_url.c_str(), escaped_url.c_str()));
    rewrite_driver_->PrependChild(element, noscript_node);
    should_insert_noscript_ = false;
  }
  // TODO(sriharis):  Handle the case where there is no body -- insert a body in
  // EndElement of kHtml?
}

bool SupportNoscriptFilter::IsAnyFilterRequiringScriptExecutionEnabled() const {
  RewriteOptions::FilterSet js_filters;
  rewrite_driver_->options()->GetEnabledFiltersRequiringScriptExecution(
      &js_filters);
  if (!js_filters.empty()) {
    for (RewriteOptions::FilterSet::const_iterator p = js_filters.begin(),
         e = js_filters.end(); p != e; ++p) {
      RewriteOptions::Filter filter = *p;
      bool filter_enabled = true;
      switch (filter) {
        case RewriteOptions::kDeferIframe:
        case RewriteOptions::kDeferJavascript:
        case RewriteOptions::kDetectReflowWithDeferJavascript:
          filter_enabled = rewrite_driver_->UserAgentSupportsJsDefer();
          break;
        case RewriteOptions::kDelayImages:
        case RewriteOptions::kLazyloadImages:
        case RewriteOptions::kLocalStorageCache:
          filter_enabled = rewrite_driver_->UserAgentSupportsImageInlining();
          break;
        case RewriteOptions::kFlushSubresources:
          filter_enabled = rewrite_driver_->UserAgentSupportsFlushEarly();
          break;
        default:
          break;
      }
      if (filter_enabled) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace net_instaweb
