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

// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/rewriter/public/html_attribute_quote_removal.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

// Explicit about signedness because we are
// loading a 0-indexed lookup table.
const unsigned char kNoQuoteChars[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789-._:";

}  // namespace

// TODO(jmaessen): Make adjustable.
const bool kLogQuoteRemoval = false;

namespace net_instaweb {
// Remove quotes; see description in .h file.

HtmlAttributeQuoteRemoval::HtmlAttributeQuoteRemoval(HtmlParse* html_parse)
    : total_quotes_removed_(0),
      html_parse_(html_parse) {
  // In pidgin Python:
  //    needs_no_quotes[:] = false
  //    needs_no_quotes[kNoQuoteChars] = true
  memset(&needs_no_quotes_, 0, sizeof(needs_no_quotes_));
  for (int i = 0; kNoQuoteChars[i] != '\0'; ++i) {
    needs_no_quotes_[kNoQuoteChars[i]] = true;
  }
}

bool HtmlAttributeQuoteRemoval::NeedsQuotes(const char *val) {
  bool needs_quotes = false;
  int i = 0;
  if (val != NULL) {
    for (; val[i] != '\0'; ++i) {
      // Explicit cast to unsigned char ensures that our offset
      // into needs_no_quotes_ is positive.
      needs_quotes = !needs_no_quotes_[static_cast<unsigned char>(val[i])];
      if (needs_quotes) {
        break;
      }
    }
  }
  // Note that due to inconsistencies in empty attribute parsing between Firefox
  // and Chrome (Chrome seems to parse the next thing it sees after whitespace
  // as the attribute value) we leave empty attributes intact.
  return needs_quotes || i == 0;
}

void HtmlAttributeQuoteRemoval::StartElement(HtmlElement* element) {
  int rewritten = 0;
  for (int i = 0; i < element->attribute_size(); ++i) {
    HtmlElement::Attribute& attr = element->attribute(i);
    if (attr.quote() != NULL && attr.quote()[0] != '\0' &&
        !NeedsQuotes(attr.value())) {
      attr.set_quote("");
      rewritten++;
    }
  }
  if (rewritten > 0) {
    total_quotes_removed_ += rewritten;
    if (kLogQuoteRemoval) {
      const char* plural = (rewritten == 1) ? "" : "s";
      html_parse_->InfoHere("Scrubbed quotes from %d attribute%s",
                            rewritten, plural);
    }
  }
}

}  // namespace net_instaweb
