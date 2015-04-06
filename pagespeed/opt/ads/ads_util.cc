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
// Author: chenyu@google.com (Yu Chen)

#include "pagespeed/opt/ads/ads_util.h"

#include <cstddef>

#include "pagespeed/opt/ads/ads_attribute.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"

// Separator used in publisher code.
namespace { const char kAdsPublisherCodeSeparator[] = "-"; }

namespace net_instaweb {

namespace ads_util {

const char kAdsByGoogleJavascriptSrc[] =
    "//pagead2.googlesyndication.com/pagead/js/adsbygoogle.js";
const char kAdsByGoogleApiCallJavascript[] =
    "(adsbygoogle = window.adsbygoogle || []).push({})";
const char kAdsbyGoogleClass[] = "adsbygoogle";

const char kShowAdsApiCallJSSrc[] =
    "//pagead2.googlesyndication.com/pagead/show_ads.js";

// TODO(chenyu): add unittests for methods defined in this file.
StringPiece GetPublisherIdWithoutProductPrefix(StringPiece publisher_code) {
  // TODO(chenyu): Modify this search to support Direct publishers
  // ("partner-aol-in" is a valid Direct AFS property code).
  stringpiece_ssize_type last_separator_pos = publisher_code.find_last_of(
      kAdsPublisherCodeSeparator);
  if (last_separator_pos != StringPiece::npos) {
    publisher_code.remove_prefix(last_separator_pos + 1);
  }
  return publisher_code;
}

bool IsValidAdsByGoogle(
    const HtmlElement& element, StringPiece publisher_id) {
  // An adsbygoogle element must be an <Ins> element.
  if (element.keyword() != HtmlName::kIns) {
    return false;
  }

  // An adsbygoogle element must have class name 'kAdsbyGoogleClass'.
  if (StringPiece(element.AttributeValue(HtmlName::kClass)) !=
      kAdsbyGoogleClass) {
    return false;
  }

  // An adsbygoogle element for publisher with 'publisher_id' must have
  // 'kDataAdClient' attribute containing 'publisher_id'.
  const HtmlElement::Attribute* ad_client_attribute =
      element.FindAttribute(ads_attribute::kDataAdClient);
  if (ad_client_attribute == NULL ||
      FindIgnoreCase(
          ad_client_attribute->DecodedValueOrNull(),
          publisher_id) == StringPiece::npos) {
    return false;
  }

  // An adsbygoogle element must have 'kDataAdSlot' attribute.
  const HtmlElement::Attribute* ad_slot_attribute =
      element.FindAttribute(ads_attribute::kDataAdSlot);
  if (ad_slot_attribute == NULL ||
      ad_slot_attribute->DecodedValueOrNull() == NULL) {
    return false;
  }

  return true;
}

bool IsShowAdsApiCallJsSrc(StringPiece src) {
  return src.find(kShowAdsApiCallJSSrc) != StringPiece::npos;
}

bool IsAdsByGoogleJsSrc(StringPiece src) {
  return src.find(kAdsByGoogleJavascriptSrc) != StringPiece::npos;
}

}  // namespace ads_util
}  // namespace net_instaweb
