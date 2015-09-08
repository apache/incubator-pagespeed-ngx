// Copyright 2010 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_KERNEL_JS_JS_MINIFY_H_
#define PAGESPEED_KERNEL_JS_JS_MINIFY_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/source_map.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/js/js_keywords.h"
#include "pagespeed/kernel/js/js_tokenizer.h"

namespace pagespeed {

namespace js {

// Represents the kind of whitespace between two tokens:
//   kNoWhitespace means that there is no whitespace between the tokens.
//   kSpace means there's been at least one space/tab, but no linebreaks.
//   kLinebreak means there's been at least one linebreak.
enum JsWhitespace { kNoWhitespace, kSpace, kLinebreak };

// This works like JsTokenizer, except that it only emits whitespace and
// comment tokens that are deemed necessary for the script to work.  IE
// conditional compilation comments are kept; other comments are removed.
// Whitespace tokens are only emitted if they are necessary to separate other
// tokens or for semicolon insertion, and any that are emitted will be
// collapsed to a single whitespace character.
class JsMinifyingTokenizer {
 public:
  // Creates a tokenizer that will tokenize the given input string (which must
  // outlive the JsMinifyingTokenizer object).
  JsMinifyingTokenizer(const JsTokenizerPatterns* patterns, StringPiece input);

  // Version that sets source mappings as well.
  // Note: Source Maps are only correct for ASCII text. Line and column numbers
  // will be incorrect if there are multi-byte chars in input.
  // TODO(sligocki): Fix this.
  JsMinifyingTokenizer(
      const JsTokenizerPatterns* patterns, StringPiece input,
      net_instaweb::source_map::MappingVector* mappings);

  ~JsMinifyingTokenizer();

  // Gets the next token type from the input,
  JsKeywords::Type NextToken(StringPiece* token_out);

  // True if an error has been encountered.  All future calls to NextToken()
  // will return JsKeywords::kEndOfInput with an empty token string.
  bool has_error() const { return tokenizer_.has_error(); }

 private:
  JsKeywords::Type NextTokenHelper(
      StringPiece* token_out,
      net_instaweb::source_map::Mapping* token_out_position);

  // Determines whether we need to include whitespace to separate the given
  // token from the previous token.
  bool WhitespaceNeededBefore(JsKeywords::Type type, StringPiece token);

  JsTokenizer tokenizer_;
  JsWhitespace whitespace_;  // Whitespace since the previous token.
  JsKeywords::Type prev_type_;
  StringPiece prev_token_;
  JsKeywords::Type next_type_;
  StringPiece next_token_;
  net_instaweb::source_map::MappingVector* mappings_;
  net_instaweb::source_map::Mapping current_position_;
  net_instaweb::source_map::Mapping next_position_;

  DISALLOW_COPY_AND_ASSIGN(JsMinifyingTokenizer);
};

// Minifies the given UTF8-encoded JavaScript code; returns true if the code
// parsed successfully, or false if a syntax error prevented complete
// minification.  Even if this function returns false, the output string will
// still be fully populated from the input; the portion of the input up to the
// parse error will be minified, and the remainder will be passed through
// unmodified.
//
// The input should be UTF8-encoded (or plain ASCII); the minifier does have
// some limited capability to tolerate invalid UTF8 bytes, so Latin1-encoded
// input will often work, but no guarantees are made.
bool MinifyUtf8Js(const JsTokenizerPatterns* patterns,
                  StringPiece input, GoogleString* output);

// Minify JS and returns a source mapping.  The input should be UTF8-encoded
// (or plain ASCII); the minifier does have some limited capability to tolerate
// invalid UTF8 bytes, so Latin1-encoded input will often work, but no
// guarantees are made.
bool MinifyUtf8JsWithSourceMap(
    const JsTokenizerPatterns* patterns,
    StringPiece input, GoogleString* output,
    net_instaweb::source_map::MappingVector* mappings);

///////////////////////////////////////////////////////////////////////////////
// Below is the old JsMinify implementation.  It has several known issues that
// the newer implementation above fixes, but for now is still more
// battle-tested.
//
// TODO(mdsteele): Deprecate these functions once we're more confident in the
// new implementation, and remove them once all clients are migrated.
///////////////////////////////////////////////////////////////////////////////

// Returns true if minification was successful, false otherwise.
bool MinifyJs(const StringPiece& input, GoogleString* out);

// Returns true if minification was successful, false otherwise.
bool GetMinifiedJsSize(const StringPiece& input, int* minimized_size);

// Returns true if minification and collapsing string was successful, false
// otherwise. This functin is a special use of js_minify. It minifies the JS
// and removes all the string literals. Example:
//   before : var x = 'asd \' lse'
//   after  : var x=''
bool MinifyJsAndCollapseStrings(const StringPiece& input, GoogleString* output);
bool GetMinifiedStringCollapsedJsSize(const StringPiece& input,
                                      int* minimized_size);

}  // namespace js

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_JS_JS_MINIFY_H_
