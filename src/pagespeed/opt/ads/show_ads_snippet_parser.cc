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

#include "pagespeed/opt/ads/show_ads_snippet_parser.h"

#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/js/js_keywords.h"
#include "pagespeed/kernel/js/js_tokenizer.h"
#include "pagespeed/kernel/util/re2.h"
#include "pagespeed/opt/ads/ads_attribute.h"

namespace net_instaweb {
namespace {

const char kAttributeNamePatternItem[] =
    "google_"
    "([a-zA-Z0-9]*)_"
    "([a-zA-Z0-9_]*)";

const char kAdFormatPattern[] = "([0-9]*)x([0-9]*)";

bool IsValidAttributeName(StringPiece name) {
  return RE2::FullMatch(StringPieceToRe2(name), kAttributeNamePatternItem);
}

bool IsValid(StringPiece attribute_name, StringPiece attribute_value) {
  // TODO(chenyu): check if we can support "_as" suffix.
  if (attribute_name == net_instaweb::ads_attribute::kGoogleAdFormat) {
    TrimWhitespace(&attribute_value);
    // For now we only support ad format in the form of widhtxheight.
    return RE2::FullMatch(StringPieceToRe2(attribute_value), kAdFormatPattern);
  }
  return true;
}

// Removes the enclosing comment tag for JS 'input'.
StringPiece StripAnyEnclosingCommentTag(StringPiece input) {
  if (input.starts_with("<!--") && input.ends_with("//-->")) {
    return input.substr(4, input.length() - 9);
  }
  return input;
}

// Removes enclosing quotes for string 'input'.
StringPiece StripAnyEnclosingQuotes(StringPiece input) {
  if ((input.starts_with("\"") && input.starts_with( "\"")) ||
      (input.starts_with("\'") && input.starts_with("\'"))) {
    return input.substr(1, input.length() - 2);
  }
  return input;
}

// Advances to the next token, skipping whitespaces and comments. The state of
// token and type is updated when moving to the next token.
void AdvanceToNextNoneWhiteSpaceCommentToken(
    pagespeed::js::JsTokenizer* tokenizer,
    StringPiece* token,
    pagespeed::JsKeywords::Type* type) {
  do {
    (*type) = tokenizer->NextToken(token);
  } while (*type == pagespeed::JsKeywords::kWhitespace ||
           *type == pagespeed::JsKeywords::kComment ||
           *type == pagespeed::JsKeywords::kLineSeparator);
}

}  // namespace

bool ShowAdsSnippetParser::ParseStrict(
    const GoogleString& content, AttributeMap* parsed_attributes) const {
  StringPiece stripped_content(content);
  TrimWhitespace(&stripped_content);
  StringPiece snippet = StripAnyEnclosingCommentTag(stripped_content);
  pagespeed::js::JsTokenizer tokenizer(&tokenizer_patterns_, snippet);

  StringPiece token;
  pagespeed::JsKeywords::Type type;

  // 'snippet' is required to repeated the format:
  // identifer = value [; \n or EOF]
  // where 'value' is either a literal string or a number.
  while (true) {
    AdvanceToNextNoneWhiteSpaceCommentToken(&tokenizer, &token, &type);

    if (type == pagespeed::JsKeywords::kIdentifier) {
      if (!IsValidAttributeName(token)) {
        return false;
      }
      GoogleString attribute_name(token.as_string());
      // Returns false if this attribute is already present.
      if (parsed_attributes->find(attribute_name) != parsed_attributes->end()) {
        return false;
      }

      AdvanceToNextNoneWhiteSpaceCommentToken(&tokenizer, &token, &type);
      if (type != pagespeed::JsKeywords::kOperator || token != "=") {
        return false;
      }

      AdvanceToNextNoneWhiteSpaceCommentToken(&tokenizer, &token, &type);
      if (type != pagespeed::JsKeywords::kStringLiteral &&
          type != pagespeed::JsKeywords::kNumber) {
        return false;
      }
      GoogleString attribute_value = StripAnyEnclosingQuotes(token).as_string();

      if (!IsValid(attribute_name, attribute_value)) {
        return false;
      }
      (*parsed_attributes)[attribute_name] = attribute_value;
      AdvanceToNextNoneWhiteSpaceCommentToken(&tokenizer, &token, &type);
      if (type == pagespeed::JsKeywords::kEndOfInput) {
        // At end of input; return successfully.
        return true;
      }
      if ((type == pagespeed::JsKeywords::kOperator && token == ";") ||
          type == pagespeed::JsKeywords::kSemiInsert) {
        // At end of an assignment, continue.
        continue;
      } else {
        return false;
      }
    } else if (type == pagespeed::JsKeywords::kEndOfInput) {
      return true;
    } else if (type == pagespeed::JsKeywords::kOperator && token == ";") {
      // Ignore an empty statement comprising just a semi-colon.
      continue;
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace net_instaweb
