/*
 * Copyright 2014 Google Inc.
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
// Author: chenyu@google.com (Yu Chen), morlovich@google.com (Maks Orlovich)

#include "net/instaweb/rewriter/public/make_show_ads_async_filter.h"

#include <map>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/opt/ads/ads_util.h"
#include "pagespeed/opt/ads/ads_attribute.h"

namespace net_instaweb {

// Names for statistics variables.
const char MakeShowAdsAsyncFilter::kShowAdsSnippetsConverted[] =
    "show_ads_snippets_converted";
const char MakeShowAdsAsyncFilter::kShowAdsSnippetsNotConverted[] =
    "show_ads_snippets_not_converte";
// This variable is used to track mispairs between showads data <script>
// elements and the <script> elements that call showads API.
const char MakeShowAdsAsyncFilter::kShowAdsApiReplacedForAsync[] =
    "show_ads_api_replaced_for_async";

MakeShowAdsAsyncFilter::MakeShowAdsAsyncFilter(RewriteDriver* rewrite_driver)
    : CommonFilter(rewrite_driver),
      current_script_element_(NULL),
      has_ads_by_google_js_(false),
      num_pending_show_ads_api_call_replacements_(0) {
  Statistics* statistics = rewrite_driver->statistics();
  show_ads_snippets_converted_count_ = statistics->FindVariable(
      kShowAdsSnippetsConverted);
  show_ads_snippets_not_converted_count_ = statistics->FindVariable(
      kShowAdsSnippetsNotConverted);
  show_ads_api_replaced_for_async_ = statistics->FindVariable(
      kShowAdsApiReplacedForAsync);
}

MakeShowAdsAsyncFilter::~MakeShowAdsAsyncFilter() {
}

void MakeShowAdsAsyncFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kShowAdsSnippetsConverted);
  statistics->AddVariable(kShowAdsSnippetsNotConverted);
  statistics->AddVariable(kShowAdsApiReplacedForAsync);
}

void MakeShowAdsAsyncFilter::StartDocumentImpl() {
  current_script_element_ = NULL;
  current_script_element_contents_.clear();
  has_ads_by_google_js_ = false;
  num_pending_show_ads_api_call_replacements_ = 0;
}

void MakeShowAdsAsyncFilter::StartElementImpl(HtmlElement* element) {
  // If it is a script, updates whether a script pointing to adsbygoogle JS has
  // been seen, and notes the current script element and starts recording its
  // content for processing showads snippet in EndElementImpl().
  if (element->keyword() == HtmlName::kScript) {
    const char* src_attribute = element->EscapedAttributeValue(HtmlName::kSrc);
    if (src_attribute != NULL && ads_util::IsAdsByGoogleJsSrc(src_attribute)) {
      has_ads_by_google_js_ = true;
    }
    DCHECK(NULL == current_script_element_);
    if (current_script_element_ == NULL) {
      current_script_element_ = element;
    }
  }
}

void MakeShowAdsAsyncFilter::EndElementImpl(HtmlElement* element) {
  // If 'element' is the end of a showads <script> element, convert it
  // to an adsbygoogle <ins>.
  // If we are waiting for a <script> that calls showads API and 'element' is
  // such an element, replace it with a <script> element that calls adsbygoogle
  // API.
  if (element == current_script_element_) {
    if (driver()->IsRewritable(element)) {
      // TODO(morlovich): We don't actually need this to be rewritable,
      // we could just leave the old one in place if it crosses the flush
      // window!
      ShowAdsSnippetParser::AttributeMap parsed_attributes;
      if (IsApplicableShowAds(current_script_element_contents_,
                              &parsed_attributes)) {
        ReplaceShowAdsWithAdsByGoogleElement(parsed_attributes, element);
      } else {
        if (num_pending_show_ads_api_call_replacements_ > 0) {
          const char* src_attribute = element->EscapedAttributeValue(
              HtmlName::kSrc);
          if (src_attribute != NULL &&
              ads_util::IsShowAdsApiCallJsSrc(src_attribute)) {
            ReplaceShowAdsApiCallWithAdsByGoogleApiCall(element);
            --num_pending_show_ads_api_call_replacements_;
          }
        }
      }
    } else {
       LOG(DFATAL) << "Scripts should never be split";
    }
  }

  if (current_script_element_ == element) {
    current_script_element_ = NULL;
    current_script_element_contents_.clear();
  }
}

void MakeShowAdsAsyncFilter::Characters(HtmlCharactersNode* characters) {
  if (current_script_element_ != NULL) {
    current_script_element_contents_ += characters->contents();
  }
}

bool MakeShowAdsAsyncFilter::IsApplicableShowAds(
    const GoogleString& content,
    ShowAdsSnippetParser::AttributeMap* parsed_attributes) const {
  if (!show_ads_snippet_parser_.ParseStrict(
          content, server_context()->js_tokenizer_patterns(),
          parsed_attributes)) {
    return false;
  }

  // Returns false if required attributes are missing.
  if (parsed_attributes->find(ads_attribute::kGoogleAdClient) ==
      parsed_attributes->end()) {
    return false;
  }

  // TODO(morlovich): Double-check if we really need width/height.
  int result;
  ShowAdsSnippetParser::AttributeMap::const_iterator iter =
      parsed_attributes->find(ads_attribute::kGoogleAdWidth);
  if (iter == parsed_attributes->end()) {
    return false;
  }
  if (!StringToInt(iter->second, &result)) {
    return false;
  }

  iter = parsed_attributes->find(ads_attribute::kGoogleAdHeight);
  if (iter == parsed_attributes->end()) {
    return false;
  }
  if (!StringToInt(iter->second, &result)) {
    return false;
  }

  // adsbygoogle.js only understands the html format.
  iter = parsed_attributes->find(ads_attribute::kGoogleAdOutput);
  if (iter != parsed_attributes->end() && iter->second != "html") {
    return false;
  }
  return true;
}

void MakeShowAdsAsyncFilter::ReplaceShowAdsWithAdsByGoogleElement(
    const ShowAdsSnippetParser::AttributeMap& parsed_attributes,
    HtmlElement* show_ads_element) {
  // Note container_element will be NULL if the script is at the
  // top level in the DOM; this is OK (see test
  // MakeShowAdsAsyncFilterTest.ShowAdsNoParent)
  HtmlElement* container_element = show_ads_element->parent();
  if (!driver()->IsRewritable(show_ads_element)) {
    LOG(DFATAL) << "show_ads_element is not rewriteable: " <<
        show_ads_element->ToString();
    return;
  }

  // If no script with src pointing to adsbygoogle.js has been seen, creates one
  // and inserts it before 'show_ads_element'.
  if (!has_ads_by_google_js_) {
    HtmlElement* script_element = driver()->NewElement(
        container_element, HtmlName::kScript);
    script_element->set_style(HtmlElement::EXPLICIT_CLOSE);
    driver()->AddAttribute(script_element, HtmlName::kAsync, StringPiece());
    driver()->AddAttribute(script_element,
                           HtmlName::kSrc,
                           ads_util::kAdsByGoogleJavascriptSrc);
    driver()->InsertNodeBeforeNode(show_ads_element, script_element);
    has_ads_by_google_js_ = true;
  }

  // We convert dimensions info into CSS.
  ShowAdsSnippetParser::AttributeMap::const_iterator width_iter =
      parsed_attributes.find(ads_attribute::kGoogleAdWidth);
  ShowAdsSnippetParser::AttributeMap::const_iterator height_iter =
      parsed_attributes.find(ads_attribute::kGoogleAdHeight);
  DCHECK(width_iter != parsed_attributes.end());
  DCHECK(height_iter != parsed_attributes.end());
  GoogleString style = StrCat("display:inline-block;",
                              "width:", width_iter->second, "px;",
                              "height:", height_iter->second, "px");

  // Creates an <ins> element with attributes computed from 'parsed_attributes'
  // and inserts it before 'show_ads_element'.
  HtmlElement* ads_by_google_element = driver()->NewElement(
      container_element, HtmlName::kIns);
  ads_by_google_element->set_style(HtmlElement::EXPLICIT_CLOSE);
  driver()->AddAttribute(ads_by_google_element,
                         HtmlName::kClass,
                         ads_util::kAdsbyGoogleClass);
  driver()->AddAttribute(ads_by_google_element, HtmlName::kStyle, style);
  ShowAdsSnippetParser::AttributeMap::const_iterator iter;
  for (iter = parsed_attributes.begin();
       iter != parsed_attributes.end();
       ++iter) {
    // Skip-over width & height, since they're in style= already.
    if (iter->first == ads_attribute::kGoogleAdWidth ||
        iter->first == ads_attribute::kGoogleAdHeight) {
      continue;
    }
    const GoogleString& ads_by_google_property_name =
        ads_attribute::LookupAdsByGoogleAttributeName(iter->first);
    const GoogleString name = ads_by_google_property_name.empty() ?
        iter->first : ads_by_google_property_name;
    driver()->AddAttribute(ads_by_google_element, name, iter->second);
  }
  driver()->InsertNodeBeforeNode(show_ads_element, ads_by_google_element);

  ++num_pending_show_ads_api_call_replacements_;
  driver()->DeleteNode(show_ads_element);
  show_ads_snippets_converted_count_->Add(1);
}

void MakeShowAdsAsyncFilter::ReplaceShowAdsApiCallWithAdsByGoogleApiCall(
    HtmlElement* show_ads_api_call_element) {
  // Creates a script element that calls adsbygoogle JS API, and uses it to
  // replace show_ads_api_call_element.
  HtmlElement* ads_by_google_api_call_element = driver()->NewElement(
      show_ads_api_call_element->parent(), HtmlName::kScript);
  driver()->InsertNodeBeforeNode(show_ads_api_call_element,
                                 ads_by_google_api_call_element);
  HtmlNode* snippet = driver()->NewCharactersNode(
      ads_by_google_api_call_element, ads_util::kAdsByGoogleApiCallJavascript);
  driver()->AppendChild(ads_by_google_api_call_element, snippet);
  driver()->DeleteNode(show_ads_api_call_element);
  show_ads_api_replaced_for_async_->Add(1);
}

}  // namespace net_instaweb
