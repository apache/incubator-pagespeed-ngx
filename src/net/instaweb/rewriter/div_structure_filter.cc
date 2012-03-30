/*
 * Copyright 2011 Google Inc.
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

// Author: jhoch@google.com (Jason R. Hoch)

#include "net/instaweb/rewriter/public/div_structure_filter.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/shared_mem_referer_statistics.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

DivStructureFilter::DivStructureFilter() {}

DivStructureFilter::~DivStructureFilter() {}

void DivStructureFilter::StartDocument() {
  div_count_stack_.push_back(0);
}

// As we parse we maintain a stack of div locations, and at each link we
// add an encoded version of the stack as a query param.  Note that the stack
// is incremented immediately after its encoded value is added as a query param.
//
// Example HTML:                                | Stack as we go:
//                                              |
// <html>                                       | 0
//   <head>                                     | 0
//   </head>                                    | 0
//   <body>                                     | 0
//     <div>                                    | 0, 0
//       <p>Toolbar link 1.</p>                 | 0, 0
//       <a href="http://a.com/b/c.html?p=q">   | 0, 1 ("0.0" added to URL)
//       <p>Toolbar link 1.</p>                 | 0, 1
//       <a href="http://a.com/b/c.html?p=q">   | 0, 2 ("0.1" added to URL)
//     </div>                                   | 1
//     <div>                                    | 1, 0
//       <div>                                  | 1, 0, 0
//         <p>Main page link.</p>               | 1, 0, 0
//         <a href="http://a.com/b/c.html?p=q"> | 1, 0, 1 ("1.0.0" added to URL)
//       </div>                                 | 1, 1
//       <p>Secondary link.</p>                 | 1, 1
//       <a href="http://a.com/b/c.html?p=q">   | 1, 2 ("1.1" added to URL)
//     </div>                                   | 2
//   </body>                                    | 2
// </html>                                      | 2
void DivStructureFilter::StartElement(HtmlElement* element) {
  HtmlName::Keyword keyword = element->keyword();
  if (keyword == HtmlName::kDiv) {
    div_count_stack_.push_back(0);
  } else if (keyword == HtmlName::kA) {
    HtmlElement::Attribute* href = element->FindAttribute(HtmlName::kHref);
    if (href != NULL) {
      const char* url = href->DecodedValueOrNull();
      if (url != NULL) {
        GoogleUrl google_url(url);
        if (google_url.is_valid()) {
          GoogleString param_value = GetDivCountStackEncoding(div_count_stack_);
          scoped_ptr<GoogleUrl> new_url(google_url.CopyAndAddQueryParam(
              SharedMemRefererStatistics::kParamName, param_value));
          // new url should be valid, so we can use Spec()
          href->SetValue(new_url->Spec());
          div_count_stack_.back()++;
        }
      }
    }
  }
}

void DivStructureFilter::EndElement(HtmlElement* element) {
  HtmlName::Keyword keyword = element->keyword();
  if (keyword == HtmlName::kDiv) {
    div_count_stack_.pop_back();
    div_count_stack_.back()++;
  }
}

// TODO(jhoch): produce shorter encodings (but keep them unique of course)
GoogleString DivStructureFilter::GetDivCountStackEncoding
    (const std::vector<int>& div_count_stack) {
  GoogleString result = "";
  for (int i = 0, size = div_count_stack.size(); i < size; i++) {
    result.append(IntegerToString(div_count_stack[i]));
    if (i != size - 1) result.append(".");
  }
  return result;
}

}  // namespace net_instaweb
