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

// Author: morlovich@google.com (Maksim Orlovich)
//
// This class lets one do simple token-match replacements of RHS of
// JS field assignments of form a.b = "foo".

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_JS_REPLACER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_JS_REPLACER_H_

#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/js/js_tokenizer.h"

namespace net_instaweb {

class JsReplacer {
 public:
  typedef Callback1<GoogleString*> StringRewriter;

  // Does not take ownership of patterns.
  explicit JsReplacer(const pagespeed::js::JsTokenizerPatterns* patterns)
      : js_tokenizer_patterns_(patterns) {}

  ~JsReplacer();

  // Whenever a pattern of object.field = "literal" is seen,
  // 'rewriter' will get called to change the value of literal.
  // (This also includes something like otherobject.object.field = "literal")
  // Does not take ownership of rewriter.
  //
  // If there are multiple additions of patterns with same object and field,
  // the first one gets invoked.
  //
  // Note that this may not work right if the pattern uses a reserved keyword
  // (e.g. things like 'class').
  void AddPattern(const GoogleString& object,
                  const GoogleString& field,
                  StringRewriter* rewriter);

  // Transforms the JS by applying any patterns added by calls to
  // AddPattern, and writes the result to *out. Quoting style of string
  // literals is preserved, and the StringRewriter is responsible for proper
  // escaping inside its output.
  //
  // Returns true if the input was lexed successfully, and false if there were
  // errors --- in which case *out may not be complete.
  bool Transform(StringPiece in, GoogleString* out);

 private:
  struct Pattern {
    Pattern() : rewriter(NULL) {}
    Pattern(const GoogleString& object, const GoogleString& field,
            StringRewriter* rewriter)
        : object(object), field(field), rewriter(rewriter) {}

    GoogleString object;
    GoogleString field;
    StringRewriter* rewriter;
  };

  // Checks to see if there is a pattern matching assignment of value to
  // object.field, and if so applies its callback to set *out and return true;
  // otherwise returns false.
  bool HandleCandidate(const GoogleString& object, const GoogleString& field,
                       StringPiece value, GoogleString* out);

  std::vector<Pattern> patterns_;
  const pagespeed::js::JsTokenizerPatterns* js_tokenizer_patterns_;

  DISALLOW_COPY_AND_ASSIGN(JsReplacer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JS_REPLACER_H_
