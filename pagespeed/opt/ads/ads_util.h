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

#ifndef PAGESPEED_KERNEL_ADS_ADS_UTIL_H_
#define PAGESPEED_KERNEL_ADS_ADS_UTIL_H_

#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class HtmlElement;

namespace ads_util {

// Src of adsbygoogle javascripts.
extern const char kAdsByGoogleJavascriptSrc[];
// Javascript snippet that calls adsbygoogle API.
extern const char kAdsByGoogleApiCallJavascript[];
// Element class name used in adsbygoogle snippets.
extern const char kAdsbyGoogleClass[];

// Returns the unique publisher id in publisher_code without product prefix.
// publisher_code is a string concatenated from a list of prefixes and a unique
// id with a separator. This method returns the unique id.
StringPiece GetPublisherIdWithoutProductPrefix(StringPiece publisher_code);

// Returns if 'element' is a valid adsbygooogle snippet for 'publisher_id'.
bool IsValidAdsByGoogle(const HtmlElement& element, StringPiece publisher_id);

// Returns if 'src' is pointing to a JS used by showads to call ads JS API.
bool IsShowAdsApiCallJsSrc(StringPiece src);

// Returns if 'src' is pointing to the JS required by adsbygoogle snippets.
bool IsAdsByGoogleJsSrc(StringPiece src);

}  // namespace ads_util
}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_ADS_ADS_UTIL_H_
