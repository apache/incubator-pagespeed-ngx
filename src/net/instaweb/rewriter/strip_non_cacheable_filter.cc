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

#include <vector>

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/strip_non_cacheable_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

const char StripNonCacheableFilter::kNoScriptRedirectFormatter[] =
    "<noscript><meta HTTP-EQUIV=\"refresh\" content=\"0;url=%s\">"
    "<style><!--table,div,span,font,p{display:none} --></style>"
    "<div style=\"display:block\">Please click <a href=\"%s\">here</a> "
    "if you are not redirected within a few seconds.</div></noscript>";

StripNonCacheableFilter::StripNonCacheableFilter(
    RewriteDriver* rewrite_driver)
    : rewrite_driver_(rewrite_driver),
      rewrite_options_(rewrite_driver->options()) {
}

StripNonCacheableFilter::~StripNonCacheableFilter() {}

void StripNonCacheableFilter::StartDocument() {
  BlinkUtil::PopulateAttributeToNonCacheableValuesMap(
      rewrite_options_->prioritize_visible_content_non_cacheable_elements(),
      &attribute_non_cacheable_values_map_,
      &panel_number_num_instances_);
}

void StripNonCacheableFilter::StartElement(HtmlElement* element) {
  int panel_number = BlinkUtil::GetPanelNumberForNonCacheableElement(
      attribute_non_cacheable_values_map_, element);
  if (panel_number != -1) {
    GoogleString panel_id = BlinkUtil::GetPanelId(
        panel_number, panel_number_num_instances_[panel_number]);
    panel_number_num_instances_[panel_number]++;
    InsertPanelStub(element, panel_id);
    rewrite_driver_->DeleteElement(element);
  }

  if (element->keyword() == HtmlName::kBody) {
    GoogleUrl url(rewrite_driver_->url());
    GoogleUrl* url_with_psa_off = url.CopyAndAddQueryParam(
        RewriteQuery::kModPagespeed, "off");
    StringPiece url_str = url_with_psa_off->Spec();

    HtmlCharactersNode* noscript_node = rewrite_driver_->NewCharactersNode(
        element,
        StringPrintf(StripNonCacheableFilter::kNoScriptRedirectFormatter,
                     url_str.data(), url_str.data()));
    rewrite_driver_->PrependChild(element, noscript_node);
    delete url_with_psa_off;
  }
}

void StripNonCacheableFilter::InsertPanelStub(HtmlElement* element,
                                              const GoogleString& panel_id) {
  HtmlCommentNode* comment = rewrite_driver_->NewCommentNode(
      element->parent(),
      StrCat(RewriteOptions::kPanelCommentPrefix, " begin ", panel_id));
  rewrite_driver_->InsertElementBeforeCurrent(comment);
  // Append end stub to json.
  comment = rewrite_driver_->NewCommentNode(
      element->parent(),
      StrCat(RewriteOptions::kPanelCommentPrefix, " end ", panel_id));
  rewrite_driver_->InsertElementBeforeCurrent(comment);
}

}   // namespace net_instaweb
