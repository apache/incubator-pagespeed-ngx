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
#ifndef PAGESPEED_KERNEL_ADS_ADS_ATTRIBUTE_H_
#define PAGESPEED_KERNEL_ADS_ADS_ATTRIBUTE_H_

#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {
namespace ads_attribute {

// Names of attributes used in adsbygoogle snippets.
extern const char kDataAdClient[];
extern const char kDataAdChannel[];
extern const char kDataAdSlot[];
extern const char kDataAdFormat[];

// Names of attributes used in showads snippets.
extern const char kGoogleAdClient[];
extern const char kGoogleAdChannel[];
extern const char kGoogleAdSlot[];
extern const char kGoogleAdFormat[];
extern const char kGoogleAdWidth[];
extern const char kGoogleAdHeight[];
extern const char kGoogleAdOutput[];

// Returns the name of the adsbygoogle attribute that corresponds to the showads
// attribute name; returns an empty string if there is no such adsbygoogle
// attribute.
GoogleString LookupAdsByGoogleAttributeName(
    StringPiece show_ads_attribute_name);

}  // namespace ads_attribute
}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_ADS_ADS_ATTRIBUTE_H_
