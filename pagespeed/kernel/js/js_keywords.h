// Copyright 2011 Google Inc. All Rights Reserved.
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
//
// Author: jmarantz@google.com
//
// This is based on third_party/libpagespeed/src/pagespeed/js/js_minify.cc by
// mdsteele@google.com

#ifndef PAGESPEED_KERNEL_JS_JS_KEYWORDS_H_
#define PAGESPEED_KERNEL_JS_JS_KEYWORDS_H_

#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {
  class JsLexer;
}

namespace pagespeed {

class JsKeywords {
 public:
  enum Type {
    // literals
    kNull,
    kTrue,
    kFalse,

    // keywords
    kBreak,
    kCase,
    kCatch,
    kConst,
    kDefault,
    kFinally,
    kFor,
    kInstanceof,
    kNew,
    kVar,
    kContinue,
    kFunction,
    kReturn,
    kVoid,
    kDelete,
    kIf,
    kThis,
    kDo,
    kWhile,
    kElse,
    kIn,
    kSwitch,
    kThrow,
    kTry,
    kTypeof,
    kWith,
    kDebugger,

    // reserved for future use
    kClass,
    kEnum,
    kExport,
    kExtends,
    kImport,
    kSuper,

    // reserved for future use in strict code
    kImplements,
    kInterface,
    kLet,
    kPackage,
    kPrivate,
    kProtected,
    kPublic,
    kStatic,
    kYield,

    // Sentinel value for gperf.
    kNotAKeyword,

    // Other types of lexical tokens; returned by lexer, but not gperf.
    kComment,        // A block or line comment (not including the linebreak).
    kWhitespace,     // Whitespace not containing any linebreaks.
    kLineSeparator,  // Whitespace with linebreaks, but no semicolon insertion.
    kSemiInsert,     // Whitespace that triggers semicolon insertion.
    kRegex,          // A regex literal, such as /foo/i or /a+b*/
    kStringLiteral,  // A string literal, such as 'foo' or "bar"
    kNumber,         // A numeric literal, such as 3.5 or 017 or .2e+10
    kOperator,       // An operator or symbol, such as && or <<= or (
    kIdentifier,     // An identifier (variable name, label, etc).
    kEndOfInput,     // End of input was reached without errors.
    kError           // A syntax error occurred.
  };

  static bool IsAKeyword(Type type) { return type < kNotAKeyword; }

  // Returns true if name is a javascript keyword that can precede a regular
  // expression. Keywords such as 'return' and 'throw' can precede a regex '/'
  // but keywords such as 'while' cannot.
  static bool CanKeywordPrecedeRegEx(const StringPiece& name);

  enum Flag {
    kNone,
    kIsValue,
    kIsReservedNonStrict,
    kIsReservedStrict,
    kCanPrecedeRegEx  // keywords that can be placed directly before a regex
  };

  // Finds a Keyword based on a keyword string.  If not found, returns
  // kNotAKeyword.  Otherwise, this always returns a Type for which
  // IsAKeyword is true.
  static Type Lookup(const StringPiece& name, Flag* flag);

  // Limited iterator (not an STL iterator).  Example usage:
  //    for (JsKeywords::Iterator iter; !iter.AtEnd(); iter.Next()) {
  //      use(iter.keyword(), iter.name());
  //    }
  class Iterator {
   public:
    Iterator() : index_(-1) { Next(); }
    bool AtEnd() const;
    void Next();
    Type keyword() const;
    const char* name() const;

   private:
    int index_;

    // Implicit copy and assign ok.  The members can be safely copied by bits.
  };

 private:
  // TODO(jkarlin): Get rid of the net_instaweb namespace once JsLexer is
  // moved into kernel/js.
  friend class net_instaweb::JsLexer;

  // Returns the number of keywords recognized by the Lookup function.  This is
  // used by the Lexer to size the keyword-sring array prior to iterating over
  // the keywords to populate it.
  static int num_keywords();
};

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_JS_JS_KEYWORDS_H_
