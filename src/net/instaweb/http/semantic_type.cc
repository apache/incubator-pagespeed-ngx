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

// Author: jefftk@google.com (Jeff Kaufman)

#include "net/instaweb/http/public/semantic_type.h"

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
namespace semantic_type {

bool ParseCategory(const StringPiece& category_str, Category* category) {
  if (StringCaseEqual("Script", category_str)) {
    *category = kScript;
  } else if (StringCaseEqual("Image", category_str)) {
    *category = kImage;
  } else if (StringCaseEqual("Stylesheet", category_str)) {
    *category = kStylesheet;
  } else if (StringCaseEqual("OtherResource", category_str)) {
    *category = kOtherResource;
  } else if (StringCaseEqual("Hyperlink", category_str)) {
    *category = kHyperlink;
  } else if (StringCaseEqual("Prefetch", category_str)) {
    *category = kPrefetch;
  } else {
    *category = kUndefined;
  }
  return *category != kUndefined;
}

}  // namespace semantic_type
}  // namespace net_instaweb
