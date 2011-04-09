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

// Author: mdsteele@google.com (Matthew D. Steele)

#include "net/instaweb/rewriter/public/collapse_whitespace_filter.h"

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

namespace {

// Tags within which we should never try to collapse whitespace (note that this
// is not _quite_ the same thing as kLiteralTags in html_lexer.cc):
const HtmlName::Keyword kSensitiveTags[] = {
  HtmlName::kPre, HtmlName::kScript, HtmlName::kStyle, HtmlName::kTextarea
};

bool IsSensitiveKeyword(HtmlName::Keyword keyword) {
  const HtmlName::Keyword* end = kSensitiveTags + arraysize(kSensitiveTags);
  return std::binary_search(kSensitiveTags, end, keyword);
}

}  // namespace

CollapseWhitespaceFilter::CollapseWhitespaceFilter(HtmlParse* html_parse)
    : html_parse_(html_parse) {
  for (size_t i = 1; i < arraysize(kSensitiveTags); ++i) {
    DCHECK(kSensitiveTags[i - 1] < kSensitiveTags[i]);
  }
}

void CollapseWhitespaceFilter::StartDocument() {
  keyword_stack_.clear();
}

void CollapseWhitespaceFilter::StartElement(HtmlElement* element) {
  HtmlName::Keyword keyword = element->keyword();
  if (IsSensitiveKeyword(keyword)) {
    keyword_stack_.push_back(keyword);
  }
}

void CollapseWhitespaceFilter::EndElement(HtmlElement* element) {
  HtmlName::Keyword keyword = element->keyword();
  if (!keyword_stack_.empty() && (keyword == keyword_stack_.back())) {
    keyword_stack_.pop_back();
  } else {
    DCHECK(!IsSensitiveKeyword(keyword));
  }
}

void CollapseWhitespaceFilter::Characters(HtmlCharactersNode* characters) {
  if (keyword_stack_.empty()) {
    // Mutate the contents-string in-place for speed.
    GoogleString* contents = characters->mutable_contents();
    // It is safe to directly mutate the bytes in the string because
    // we are only going to shrink it, never grow it.
    char* read_ptr = &(*contents)[0];
    char* write_ptr = read_ptr;
    char* end = read_ptr + contents->size();
    int in_whitespace = 0;  // Used for pointer-subtraction so newlines dominate
    for (; read_ptr != end; ++read_ptr) {
      char ch = *read_ptr;
      switch (ch) {
        // See http://www.w3.org/TR/html401/struct/text.html#h-9.1
        case ' ':
        case '\t':
        case '\r':
        case '\f':
          // Add whitespace if the previous character was not already
          // whitespace.  Note that the whitespace may be overwritten
          // by a newline.  This extra branch could be avoided if we folded
          // the current whitespace-state into the switch via an OR.
          if (in_whitespace == 0) {
            *write_ptr++ = ch;
            in_whitespace = 1;
          }
          break;
        case '\n':
          // If the previous character was a whitespace, then back up
          // so that the 'write' in the default case will overwrite the
          // previous whitespace with a newline.  Avoid branches.
          write_ptr -= in_whitespace;
          in_whitespace = 1;
          *write_ptr++ = ch;
          break;
        default:
          in_whitespace = 0;
          *write_ptr++ = ch;
          break;
      }
    }
    contents->resize(write_ptr - contents->data());
  }
}

}  // namespace net_instaweb
