/*
 * Copyright 2017 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)
//
// This provides basic parsing and evaluation of a (subset of)
// Content-Security-Policy that's relevant for PageSpeed Automatic

#include "net/instaweb/rewriter/public/csp.h"

#include "net/instaweb/rewriter/public/csp_directive.h"

namespace net_instaweb {

namespace {

void TrimCspWhitespace(StringPiece* input) {
  // AKA RWS in HTTP spec, which of course isn't the HTML notion of whitespace
  // that TrimWhitespace uses.
  while (!input->empty() && ((*input)[0] == ' ' || (*input)[0] == '\t')) {
    input->remove_prefix(1);
  }

  while (input->ends_with(" ") || input->ends_with("\t")) {
    input->remove_suffix(1);
  }
}

char Last(StringPiece input) {
  DCHECK(!input.empty());
  return input[input.size() - 1];
}

inline bool IsAsciiAlpha(char ch) {
  return (((ch >= 'a') && (ch <= 'z')) ||
          ((ch >= 'A') && (ch <= 'Z')));
}

}  // namespace

CspSourceExpression CspSourceExpression::Parse(StringPiece input) {
  TrimCspWhitespace(&input);
  if (input.empty()) {
    return CspSourceExpression(kUnknown);
  }

  if (input.size() > 2 && input[0] == '\'' && Last(input) == '\'') {
    return ParseQuoted(input.substr(1, input.size() - 2));
  }

  // Check for scheme-source.
  if (input.size() >= 2 && Last(input) == ':') {
    // scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
    bool is_scheme = true;
    if (IsAsciiAlpha(input[0])) {
      for (size_t i = 1; i < (input.size() - 1); ++i) {
        char c = input[i];
        if (!IsAsciiAlphaNumeric(c) && (c != '+') && (c != '-') && (c != '.')) {
          is_scheme = false;
          break;
        }
      }
    } else {
      is_scheme = false;
    }

    if (is_scheme) {
      return CspSourceExpression(kSchemeSource, input);
    }
  }

  // Assume host-source. It might make sense to split this down further here,
  // that will become clear once the actual URL matching algorithm is
  // implemented.
  return CspSourceExpression(kHostSource, input);
}

CspSourceExpression CspSourceExpression::ParseQuoted(StringPiece input) {
  CHECK(!input.empty());

  if (input[0] == 'u' || input[0] == 'U') {
    if (StringCaseEqual(input, "unsafe-inline")) {
      return CspSourceExpression(kUnsafeInline);
    }
    if (StringCaseEqual(input, "unsafe-eval")) {
      return CspSourceExpression(kUnsafeEval);
    }
    if (StringCaseEqual(input, "unsafe-hashed-attributes")) {
      return CspSourceExpression(kUnsafeHashedAttributes);
    }
  }

  if (input[0] == 's' || input[0] == 'S') {
    if (StringCaseEqual(input, "self")) {
      return CspSourceExpression(kSelf);
    }
    if (StringCaseEqual(input, "strict-dynamic")) {
      return CspSourceExpression(kStrictDynamic);
    }
  }
  return CspSourceExpression(kUnknown);
}

std::unique_ptr<CspSourceList> CspSourceList::Parse(StringPiece input) {
  std::unique_ptr<CspSourceList> result(new CspSourceList);

  TrimCspWhitespace(&input);
  StringPieceVector tokens;
  SplitStringPieceToVector(input, " ", &tokens, true);
  for (StringPiece token : tokens) {
    TrimCspWhitespace(&token);
    CspSourceExpression expr = CspSourceExpression::Parse(token);
    if (expr.kind() != CspSourceExpression::kUnknown) {
      result->expressions_.push_back(expr);
    }
  }

  return result;
}

CspPolicy::CspPolicy() {
  policies_.resize(static_cast<size_t>(CspDirective::kNumSourceListDirectives));
}

std::unique_ptr<CspPolicy> CspPolicy::Parse(StringPiece input) {
  std::unique_ptr<CspPolicy> policy;

  TrimCspWhitespace(&input);

  StringPieceVector tokens;
  SplitStringPieceToVector(input, ";", &tokens, true);

  // TODO(morlovich): This will need some extra-careful testing.
  // Essentially the spec has a notion of a policy with an empty directive set,
  // and it basically gets ignored; but is a policy like
  // tasty-chocolate-src: * an empty one, or not? This is particularly
  // relevant since we may not want to parse worker-src or whatever.
  if (tokens.empty()) {
    return policy;
  }

  policy.reset(new CspPolicy);
  for (StringPiece token : tokens) {
    TrimCspWhitespace(&token);
    StringPiece::size_type pos = token.find(' ');
    if (pos != StringPiece::npos) {
      StringPiece name = token.substr(0, pos);
      StringPiece value = token.substr(pos + 1);
      CspDirective dir_name = LookupCspDirective(name);
      if (dir_name != CspDirective::kNumSourceListDirectives &&
          policy->policies_[static_cast<int>(dir_name)] == nullptr) {
        // Note: repeated directives are ignored per the "Parse a serialized
        // CSP as disposition" algorith,
        policy->policies_[static_cast<int>(dir_name)]
            = CspSourceList::Parse(value);
      }
    }
  }

  return policy;
}

}  // namespace net_instaweb
