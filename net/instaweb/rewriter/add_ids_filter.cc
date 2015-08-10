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
// Author: jmaessen@google.com (Jan-Willem Maessen)

#include "net/instaweb/rewriter/public/add_ids_filter.h"

#include <algorithm>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"

namespace net_instaweb {

namespace {

// TODO(jmaessen): perhaps this should go somewhere central?  It needs to be a
// subset of the tags considered divlike by mobilize_label_filter at least.
const HtmlName::Keyword kDivLikeTags[] = {
  HtmlName::kArticle,
  HtmlName::kAside,
  HtmlName::kContent,
  HtmlName::kDiv,
  HtmlName::kFooter,
  HtmlName::kForm,
  HtmlName::kHeader,
  HtmlName::kMain,
  HtmlName::kMenu,
  HtmlName::kNav,
  HtmlName::kSection,
  HtmlName::kTable,
  HtmlName::kTr,
  HtmlName::kUl
};

#ifndef NDEBUG
// For invariant-checking the static data above.
void CheckKeywordsSorted(const HtmlName::Keyword* list, int len) {
  for (int i = 1; i < len; ++i) {
    DCHECK_LT(list[i - 1], list[i]);
  }
}
#endif  // #ifndef NDEBUG

bool IsDivLike(HtmlName::Keyword tag) {
  return std::binary_search(
      kDivLikeTags, kDivLikeTags + arraysize(kDivLikeTags), tag);
}

bool NeedsExplicitId(HtmlName::Keyword tag) {
  return IsDivLike(tag);
}

bool IsIgnored(HtmlName::Keyword tag) {
  return (tag == HtmlName::kHtml || tag == HtmlName::kBody);
}

}  // namespace

// We don't want this to conflict with another id name, and length
// also matters (shorter is better).
const char AddIdsFilter::kIdPrefix[] = "PageSpeed";

const int AddIdsFilter::kIsId = -1;

AddIdsFilter::AddIdsFilter(RewriteDriver* driver)
    : driver_(driver) {}

AddIdsFilter::~AddIdsFilter() {}

void AddIdsFilter::StartDocument() {
  // Push an initial top-level count.
  div_count_stack_.clear();
  div_count_stack_.push_back(0);
  id_stack_.clear();
#ifndef NDEBUG
  CheckKeywordsSorted(kDivLikeTags, arraysize(kDivLikeTags));
#endif  // #ifndef NDEBUG
}

// As we parse outside head we maintain a stack of tag locations, and at each
// tag for which TagRequiresId we add an encoded version of the stack as a query
// param.  Note that the stack is incremented immediately after its encoded
// value is added as a query param.
//
// An explicit id adds a kIsId entry to the stack before the entry for that
// tag's children, and pushes the id onto the id_stack_.
//
// Example HTML:                   | Stack as we go:
//                                 |
// <html>                          | 0
//   <head>                        | 0
//   </head>                       | 0
//   <body>                        | 0
//     <div>                       | 0, 0 (id="...-0")
//       <p>Toolbar link 1.</p>    | 0, 0
//       <p>Toolbar link 2.</p>    | 0, 1
//     </div>                      | 1             id stack
//     <div id=foo>                | 1, -1, 0      foo
//       <div>                     | 1, -1, 0, 0   foo (id="...-foo-0")
//         <p>Main page link.</p>  | 1, -1, 0, 0   foo
//       </div>                    | 1, 1
//       <div>Secondary link.      | 1, -1, 1, 0   foo (id="...-foo-1")
//       </div>                    | 1, -1, 2      foo
//     </div>                      | 2
//   </body>                       | 2
// </html>                         | 2
void AddIdsFilter::StartElement(HtmlElement* element) {
  HtmlName::Keyword tag = element->keyword();
  const HtmlElement::Attribute* id =
      element->FindAttribute(HtmlName::kId);
  if (id != NULL) {
    id_stack_.push_back(id);
    div_count_stack_.push_back(kIsId);
  } else if (IsIgnored(tag)) {
    // Don't touch stack in this case.
    return;
  } else if (NeedsExplicitId(tag) ||
             element->FindAttribute(HtmlName::kClass) != NULL) {
    driver_->AddAttribute(element, HtmlName::kId, GetDivCountStackEncoding());
  }
  div_count_stack_.push_back(0);
}

void AddIdsFilter::EndElement(HtmlElement* element) {
  DCHECK(!div_count_stack_.empty());
  DCHECK_NE(kIsId, div_count_stack_.back());
  if (!id_stack_.empty() &&
      id_stack_.back() == element->FindAttribute(HtmlName::kId)) {
    DCHECK_LT(2, div_count_stack_.size());
    // For an element with an id the stack looks like:
    // ... my_count_in_parent kIsId child_count
    // If so, pop both along with the back of id_stack_.
    div_count_stack_.pop_back();
    id_stack_.pop_back();
    // Now stack is ... my_count_in_parent kIsId
  } else if (IsIgnored(element->keyword())) {
    // Again, don't touch the stack in this case.
    return;
  } else {
    // stack is:
    // ... my_count_in_parent child_count
  }
  div_count_stack_.pop_back();
  // Stack is ... my_count_in_parent
  div_count_stack_.back()++;
  // Stack is ... my_count_in_parent+1
  DCHECK(!div_count_stack_.empty());
  DCHECK_NE(kIsId, div_count_stack_.back());
}

GoogleString AddIdsFilter::GetDivCountStackEncoding() {
  DCHECK(!div_count_stack_.empty());
  DCHECK_NE(kIsId, div_count_stack_.back());
  GoogleString result(kIdPrefix);
  if (!id_stack_.empty()) {
    // Note: we make use of StringPiece(NULL) -> "" in this call.
    StrAppend(&result, "-", id_stack_.back()->escaped_value());
  }
  int size = div_count_stack_.size();
  int count_index = size - 1;
  while (count_index > 0 && div_count_stack_[count_index - 1] != kIsId) {
    --count_index;
  }
  for (; count_index < size; ++count_index) {
    StrAppend(&result, "-", IntegerToString(div_count_stack_[count_index]));
  }
  return result;
}

}  // namespace net_instaweb
