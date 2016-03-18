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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_MAKE_SHOW_ADS_ASYNC_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_MAKE_SHOW_ADS_ASYNC_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_filter.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/opt/ads/show_ads_snippet_parser.h"

namespace net_instaweb {

class Statistics;
class Variable;

// This filter converts from synchronous AdSense snippets (showads.js)
// to async ones (adsbygoogle.js)
class MakeShowAdsAsyncFilter : public CommonFilter {
 public:
  static const char kShowAdsSnippetsConverted[];
  static const char kShowAdsSnippetsNotConverted[];
  static const char kShowAdsApiReplacedForAsync[];

  explicit MakeShowAdsAsyncFilter(RewriteDriver* rewrite_driver);
  virtual ~MakeShowAdsAsyncFilter();

  static void InitStats(Statistics* statistics);

  // Overrides CommonFilter
  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);

  // Overrides HtmlFilter
  virtual const char* Name() const {
    return "MakeShowAdsAsyncFilter";
  }
  virtual void Characters(HtmlCharactersNode* characters);
  ScriptUsage GetScriptUsage() const override { return kWillInjectScripts; }

 private:
  // Parses an element that has 'content'; the parsed attributes are stored
  // in 'parsed_attributes'. It returns true if the element is a showads element
  // that can be changed to be async. 'parsed_attributes' are
  // meaningful only when this method returns true.
  //
  // Note that this is in particular looking for the inline snippets that
  // set various google_ad_ variables, rather than <script src="showads">
  bool IsApplicableShowAds(
      const GoogleString& content,
      ShowAdsSnippetParser::AttributeMap* parsed_attributes) const;

  // Creates elements for an adsbygoogle snippet that is equivalent to element
  // 'show_ads_element', whose attributes are parsed in 'parsed_attributes', and
  // replace the 'show_ads_element' with the created elements.
  void ReplaceShowAdsWithAdsByGoogleElement(
      const ShowAdsSnippetParser::AttributeMap& parsed_attributes,
      HtmlElement* show_ads_elment);
  void ReplaceShowAdsApiCallWithAdsByGoogleApiCall(
      HtmlElement* show_ads_elment);

  // The current element if it is a script element, NULL otherwise.
  HtmlElement* current_script_element_;
  // Contents of 'current_script_element_'.
  GoogleString current_script_element_contents_;

  // These two variables ('has_ads_by_google_js_' and
  // 'num_pending_show_ads_push_replacements_') are used when replacing a
  // showads ad by an adsbygoogle ad. More details are given below.
  //
  // In order to display adsbygoogle ads in an HTML page, the required JS
  // (adsbygoogle.js) must be loaded at least once a page, that is, a <script>
  // with src pointing to adsbygoogle.js should be present in the page.
  // For each ad, there must be
  // - a valid adsbygoogle <ins> element and
  // - a <script> element with a snippet that calls adsbygoogle API.
  // The <script> element that calls adsbygoogle API must be after the <ins>
  // element; it does not need to be immediately after the <ins> element.
  //
  // In order to display a showads ad in an HTML page, there must be
  // - a valid showads data <script> element and
  // - a <script> element with a snippet that calls showads API.
  // The <script> element that calls showads API must be after the showads data
  // <script> element; it does not need to be immediately after the showads data
  // <script> element.
  //
  // In this class, we use 'has_ads_by_google_js_' to track whether a
  // <script> element with src pointing to adsbygoogle.js has been seen.
  //
  // Each time an applicable showads data <script> element is seen, it is
  // replaced with an adsbygoogle <ins> element. And if no <script> element with
  // src pointing to adsbygoogle.js has been seen, we will create one and
  // insert it to the page, and also set 'has_ads_by_google_js_' to true to
  // prevent us from the doing this more than once.
  bool has_ads_by_google_js_;
  // The number of <script> elements with a snippet that calls showads API and
  // - that are expected to be paired with a replaced showads data <script>
  //   element, and
  // - that has not been replaced by a <script> element with a snippet that
  //   calls adsbygoogle API.
  int32 num_pending_show_ads_api_call_replacements_;

  ShowAdsSnippetParser show_ads_snippet_parser_;

  // Statistics variables.
  Variable* show_ads_snippets_converted_count_;
  Variable* show_ads_snippets_not_converted_count_;
  Variable* show_ads_api_replaced_for_async_;

  DISALLOW_COPY_AND_ASSIGN(MakeShowAdsAsyncFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_MAKE_SHOW_ADS_ASYNC_FILTER_H_
