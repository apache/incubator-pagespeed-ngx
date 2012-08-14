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

#include "net/instaweb/rewriter/public/handle_noscript_redirect_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/string_util.h"

namespace {
const char kCanonical[] = "canonical";
}  // namespace

namespace net_instaweb {

HandleNoscriptRedirectFilter::HandleNoscriptRedirectFilter(
    RewriteDriver* rewrite_driver) : rewrite_driver_(rewrite_driver) {
  Init();
}

HandleNoscriptRedirectFilter::~HandleNoscriptRedirectFilter() {
}

void HandleNoscriptRedirectFilter::Init() {
  canonical_present_ = false;
  canonical_inserted_ = false;
}

void HandleNoscriptRedirectFilter::StartDocument() {
  Init();
}

void HandleNoscriptRedirectFilter::StartElement(HtmlElement* element) {
  if (!canonical_inserted_ && !canonical_present_ &&
      element->keyword() == HtmlName::kLink) {
    // Checks if a <link rel=canonical href=...> is present.
    HtmlElement::Attribute* rel_attr = element->FindAttribute(HtmlName::kRel);
    HtmlElement::Attribute* href_attr = element->FindAttribute(HtmlName::kHref);
    canonical_present_ = (rel_attr != NULL && href_attr != NULL &&
                          StringCaseEqual(rel_attr->DecodedValueOrNull(),
                                          kCanonical));
  }
}

void HandleNoscriptRedirectFilter::EndElement(HtmlElement* element) {
  if (!canonical_inserted_ && !canonical_present_ &&
      element->keyword() == HtmlName::kHead) {
    // We insert the <link rel=canonical href=original_url> at the end of the
    // first head, if the first head did not already contain a
    // <link rel=canonical href=...>
    // TODO(sriharis):  Get the query param stripped in driver in apache.
    // TODO(sriharis):  Should we check all heads for
    // <link rel=canonical href=...> ?   If we want to do this then if there is
    // no such element, to insert our link element we might need to add a head
    // (since all heads might have been flushed already).
    HtmlCharactersNode* link_node = rewrite_driver_->NewCharactersNode(
        element, StringPrintf(kLinkRelCanonicalFormatter,
                              rewrite_driver_->url()));
    rewrite_driver_->AppendChild(element, link_node);
    canonical_inserted_ = true;
  }
}

}  // namespace net_instaweb
