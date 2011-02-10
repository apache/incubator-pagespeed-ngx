/*
 * Copyright 2010 Google Inc.
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

#include "net/instaweb/rewriter/public/strip_scripts_filter.h"
#include "net/instaweb/htmlparse/public/html_parse.h"

namespace net_instaweb {

StripScriptsFilter::StripScriptsFilter(HtmlParse* html_parse)
    : html_parse_(html_parse) {
}

void StripScriptsFilter::EndElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kScript) {
    html_parse_->DeleteElement(element);
  }
}

}  // namespace net_instaweb
