/**
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
#include <string>

namespace {

// Tags within which we should never try to collapse whitespace (note that this
// is not _quite_ the same thing as kLiteralTags in html_lexer.cc):
const char* const kSensitiveTags[] = {"pre", "script", "style", "textarea"};

bool IsHtmlWhiteSpace(char ch) {
  // See http://www.w3.org/TR/html401/struct/text.html#h-9.1
  return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t' || ch == '\f';
}

// Sentinel value for use in the CollapseWhitespace function:
const char kNotInWhitespace = '\0';

// Append the input to the output with whitespace collapsed.  Specifically,
// each contiguous sequence of whitespace is replaced with the first
// (whitespace) character in the sequence, except that any sequence containing
// a newline is collapsed to a newline.
void CollapseWhitespace(const std::string& input, std::string* output) {
  // This variable stores the first whitespace character in each whitespace
  // sequence, or kNotInWhitespace.
  char whitespace = kNotInWhitespace;
  for (std::string::const_iterator iter = input.begin(), end = input.end();
       iter != end; ++iter) {
    const char ch = *iter;
    if (IsHtmlWhiteSpace(ch)) {
      // We let newlines take precedence over other kinds of whitespace, for
      // aesthetic reasons.
      if (whitespace == kNotInWhitespace || ch == '\n') {
        whitespace = ch;
      }
    } else {
      if (whitespace != kNotInWhitespace) {
        *output += whitespace;
        whitespace = kNotInWhitespace;
      }
      *output += ch;
    }
  }
  if (whitespace != kNotInWhitespace) {
    *output += whitespace;
  }
}

}  // namespace

namespace net_instaweb {

CollapseWhitespaceFilter::CollapseWhitespaceFilter(HtmlParse* html_parse)
    : html_parse_(html_parse) {
  for (size_t i = 0; i < arraysize(kSensitiveTags); ++i) {
    sensitive_tags_.insert(html_parse->Intern(kSensitiveTags[i]));
  }
}

void CollapseWhitespaceFilter::StartDocument() {
  atom_stack_.clear();
}

void CollapseWhitespaceFilter::StartElement(HtmlElement* element) {
  const Atom tag = element->tag();
  if (sensitive_tags_.count(tag) > 0) {
    atom_stack_.push_back(tag);
  }
}

void CollapseWhitespaceFilter::EndElement(HtmlElement* element) {
  const Atom tag = element->tag();
  if (!atom_stack_.empty() && tag == atom_stack_.back()) {
    atom_stack_.pop_back();
  } else {
    DCHECK(sensitive_tags_.count(tag) == 0);
  }
}

void CollapseWhitespaceFilter::Characters(HtmlCharactersNode* characters) {
  if (atom_stack_.empty()) {
    std::string minified;
    CollapseWhitespace(characters->contents(), &minified);
    html_parse_->ReplaceNode(
        characters,
        html_parse_->NewCharactersNode(characters->parent(), minified));
  }
}

}  // namespace net_instaweb
