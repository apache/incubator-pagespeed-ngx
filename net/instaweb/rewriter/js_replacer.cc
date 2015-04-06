// Copyright 2014 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/rewriter/public/js_replacer.h"

#include "base/logging.h"
#include "pagespeed/kernel/js/js_keywords.h"

using pagespeed::JsKeywords;
using pagespeed::js::JsTokenizer;

namespace net_instaweb {

namespace {

enum State {
  kStart,
  kSawIdent,
  kSawIdentDot,
  kSawIdentDotIdent,
  kSawIdentDotIdentEquals
};

void ResetState(State* state, GoogleString* maybe_object,
                GoogleString* maybe_field) {
  *state = kStart;
  maybe_object->clear();
  maybe_field->clear();
}

}  // namespace.

JsReplacer::~JsReplacer() {}

void JsReplacer::AddPattern(const GoogleString& object,
                            const GoogleString& field,
                            StringRewriter* rewriter) {
  patterns_.push_back(Pattern(object, field, rewriter));
}

bool JsReplacer::Transform(StringPiece in, GoogleString* out) {
  State state = kStart;

  GoogleString maybe_object;
  GoogleString maybe_field;

  pagespeed::js::JsTokenizer tokenizer(js_tokenizer_patterns_, in);
  out->clear();
  while (true) {
    // Note that this may get modified in the switch below.
    StringPiece token;
    GoogleString replacement;  // in case we need to replace token.
    JsKeywords::Type type = tokenizer.NextToken(&token);
    // TODO(morlovich): This only matches object.field, not object['field'].
    switch (type) {
      case JsKeywords::kEndOfInput:
        return true;
      case JsKeywords::kError:
        return false;
      case JsKeywords::kComment:
      case JsKeywords::kWhitespace:
      case JsKeywords::kLineSeparator:
      case JsKeywords::kSemiInsert:
        // Whitespace is just passed through, and doesn't cause state machine
        // transitions.
        break;
      case JsKeywords::kIdentifier:
        switch (state) {
          case kStart:
          case kSawIdent:
          case kSawIdentDotIdent:
          case kSawIdentDotIdentEquals:
            state = kSawIdent;
            token.CopyToString(&maybe_object);
            break;
          case kSawIdentDot:
            state = kSawIdentDotIdent;
            token.CopyToString(&maybe_field);
            break;
        }
        break;
      case JsKeywords::kOperator:
        if (token == ".") {
          switch (state) {
            case kStart:
            case kSawIdentDot:
            case kSawIdentDotIdentEquals:
              // No clue on how some of these could parse.
              ResetState(&state, &maybe_object, &maybe_field);
              break;
            case kSawIdent:
              state = kSawIdentDot;
              break;
            case kSawIdentDotIdent:
              // This is something like a.b. -> so what we thought was a field
              // is now "object".
              state = kSawIdentDot;
              maybe_object = maybe_field;
              maybe_field.clear();
              break;
          }
        } else if (token == "=") {
          switch (state) {
            case kStart:
            case kSawIdent:
            case kSawIdentDot:
            case kSawIdentDotIdentEquals:
              // No clue on how some of these could parse.
              ResetState(&state, &maybe_object, &maybe_field);
              break;

            case kSawIdentDotIdent:
              state = kSawIdentDotIdentEquals;
              break;
          }
        } else {
          // Things other than . and = are uninteresting to us.
          ResetState(&state, &maybe_object, &maybe_field);
        }
        break;
      case JsKeywords::kStringLiteral:
        if (state == kSawIdentDotIdentEquals) {
          if (HandleCandidate(maybe_object, maybe_field, token, &replacement)) {
            token = replacement;
          }
        } else {
          ResetState(&state, &maybe_object, &maybe_field);
        }
        break;
      default:
        // Something unexpected --- reset matching.
        ResetState(&state, &maybe_object, &maybe_field);
    }

    StrAppend(out, token);
  }
}

bool JsReplacer::HandleCandidate(
    const GoogleString& object, const GoogleString& field,
    StringPiece value, GoogleString* out) {
  // Note that the token still has the quotes; we strip them before invoking
  // the callback and then restore them when serializing.
  CHECK_GE(value.length(), 2) << value;
  char quote = value[0];
  CHECK(quote == '\'' || quote == '"');
  CHECK_EQ(quote, value[value.length() - 1]);
  value.remove_prefix(1);
  value.remove_suffix(1);

  // Check patterns.
  for (int i = 0, n = patterns_.size(); i < n; ++i) {
    const Pattern& pat = patterns_[i];
    if (pat.object == object && pat.field == field) {
      GoogleString quote_str(1, quote);
      GoogleString rewriter_inout;
      value.CopyToString(&rewriter_inout);
      pat.rewriter->Run(&rewriter_inout);
      out->clear();
      StrAppend(out, quote_str, rewriter_inout, quote_str);
      return true;
    }
  }

  return false;
}

}  // namespace net_instaweb
