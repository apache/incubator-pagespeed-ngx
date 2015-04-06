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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef PAGESPEED_KERNEL_HTTP_DOMAIN_REGISTRY_H_
#define PAGESPEED_KERNEL_HTTP_DOMAIN_REGISTRY_H_

#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {
namespace domain_registry {

// The public suffix of a hostname is the bit shared between multiple
// organizations.  For example, anyone can register under ".com",
// ".co.uk", or ".appspot.com".  The minimal private suffix goes one
// dotted section further, and is the name you would register when
// getting a domain: "google.com", "google.co.uk",
// "mysite.appspot.com".  See domain_registry_test for more examples.
StringPiece MinimalPrivateSuffix(StringPiece hostname);

// Must be called before calling MinimalPrivateSuffix.
void Init();

}  // namespace domain_registry
}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTTP_DOMAIN_REGISTRY_H_
