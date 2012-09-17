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

#include "net/instaweb/rewriter/public/redirect_on_size_limit_filter.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/util/public/escaping.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kScript[] = "window.location=\"%s\";";

}  // namespace

namespace net_instaweb {

RedirectOnSizeLimitFilter::RedirectOnSizeLimitFilter(
    RewriteDriver* rewrite_driver)
    : CommonFilter(rewrite_driver),
      redirect_inserted_(false) {
}

RedirectOnSizeLimitFilter::~RedirectOnSizeLimitFilter() {
}

void RedirectOnSizeLimitFilter::StartDocumentImpl() {
  redirect_inserted_ = false;
}

void RedirectOnSizeLimitFilter::StartElementImpl(HtmlElement* element) {
  InsertScriptIfNeeded(element, true);
}


void RedirectOnSizeLimitFilter::EndElementImpl(HtmlElement* element) {
  InsertScriptIfNeeded(element, false);
}

void RedirectOnSizeLimitFilter::InsertScriptIfNeeded(HtmlElement* element,
                                                     bool is_start) {
  if (!redirect_inserted_ && noscript_element() == NULL &&
      driver()->size_limit_exceeded()) {
    scoped_ptr<GoogleUrl> url_with_psa_off(
        driver()->google_url().CopyAndAddQueryParam(
            RewriteQuery::kModPagespeed, "off"));
    GoogleString url_str;
    EscapeToJsStringLiteral(url_with_psa_off->Spec(), false, &url_str);

    HtmlElement* script = driver()->NewElement(
        element, HtmlName::kScript);
    driver()->AddAttribute(script, HtmlName::kType, "text/javascript");
    HtmlNode* script_code = driver()->NewCharactersNode(
      script, StringPrintf(kScript, url_str.c_str()));
    // For HTML elements, add the script as a child. Otherwise, insert the
    // script before or after the element.
    if (element->keyword() == HtmlName::kHtml) {
      if (is_start) {
        driver()->PrependChild(element, script);
      } else {
        driver()->AppendChild(element, script);
      }
    } else if (is_start) {
      driver()->InsertElementBeforeElement(element, script);
    } else {
      driver()->InsertElementAfterElement(element, script);
    }
    driver()->AppendChild(script, script_code);
    redirect_inserted_ = true;
  }
}

}  // namespace net_instaweb
