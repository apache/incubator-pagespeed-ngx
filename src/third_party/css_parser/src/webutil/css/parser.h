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

// Copyright 2006 Google Inc. All Rights Reserved.
// Author: dpeng@google.com (Daniel Peng)

#ifndef WEBUTIL_CSS_PARSER_H__
#define WEBUTIL_CSS_PARSER_H__

#include <string>
#include <vector>
#include "base/scoped_ptr.h"
#include "strings/stringpiece.h"
#include "testing/production_stub/public/gunit_prod.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/property.h"  // while these CSS includes can be
#include "webutil/css/selector.h"  // forward-declared, who is really
#include "webutil/css/string.h"
#include "webutil/css/value.h"    // going to want parser.h but not values.h?
#include "webutil/html/htmlcolor.h"

namespace Css {

// These are defined below Parser.
struct Import;
class Declaration;
class Declarations;
class Stylesheet;
class Ruleset;

// Recursive descent parser for CSS.
//   Based on: http://www.w3.org/TR/CSS21/syndata.html
//             http://www.w3.org/TR/CSS21/grammar.html
//
// Say you want to parse a fragment of CSS.  Then construct a new
// Parser object (this is very lightweight) and pass in the fragment to parse.
// Then, call the top-level ParseXXX() function for what you want to parse.
// This parses the fragment and returns a pointer to the abstract syntax tree.
// You own this pointer and must delete it when you're done.
//
// The data structures comprising the abstract syntax tree are described in
// cssvalue.h, cssparser-inl.h, csscondition.h, and cssproperty.h.
//
// Essentially, each stylesheet is a collection of rulesets.

// Each ruleset has some selectors to describe what HTML elements it
// applies to and some declarations to describe how the HTML elements
// should be styled.
// The ruleset may apply to multiple comma-separated selectors, which
// means they apply to an element matching any of the selectors.
// Ex: h1, h2 > p, h3 { color: red; }
//
// Each selector consists of a chain of simple selectors, separated by
// combinators.
// Ex: h2 > p selects a P element that is a child of an H2 element.
//
// Each simple selector may have conditions which impose further
// restrictions, such as [foo], #id, .class, or :hover.  We represent
// these as a list, which are semantically AND-ed together.
//
// Each declaration consists of a property and a list of values.
//
// Say, for example, you wish to parse a CSS declaration:
//    Parser a("background: white; color: #333; line-height: 1.3;
//                 text-align: justify; font-family: \"Gill Sans MT\",
//                 \"Gill Sans\", GillSans, Arial, Helvetica, sans-serif");
//    scoped_ptr<Declarations> t(a.ParseDeclarations());
// See the 'declarations' unit test case for more details.
//
// We've made most of the recursive-descent ParseXXX functions private
// to shrink the size of the public interface.  We expose
// ParseStylesheet() and ParseDeclarations() because those are the
// top-level functions necessary to parse stylesheets in HTML
// documents. And ParseSelectors is exposed to parse selectors.  If it's
// useful to expose more of the functions, please just send a CL for approval,
// so we know what people depend on.
//
// The CSS parser runs in either quirks mode (the default) and standard
// compliant mode. The latter is stricter in many aspects. Currently, it
// affects color parsing (see below for details). Please refer to:
//   http://developer.mozilla.org/en/docs/Mozilla_Quirks_Mode_Behavior
// for the difference in Mozilla browsers.
class Parser {
 public:
  Parser(const char* utf8text, const char* textend);
  explicit Parser(const char* utf8text);
  explicit Parser(StringPiece s);

  // ParseRawSytlesheet and ParseStylesheet consume the entire document and
  // return a Stylesheet* containing all the imports and rulesets that it
  // found.  You must delete the return pointer.

  // ParseRawStylesheet simply parses the document into an abstract syntax tree.
  Stylesheet* ParseRawStylesheet();
  // ParseStylesheet also runs a second pass to convert shorthand
  // declarations such as background, font and font-family into sets of
  // declarations that they represent.
  Stylesheet* ParseStylesheet();

  // ParserRawDeclarations and ParseDeclarations parse declarations like
  // "background: white; color: #333; line-height: 1.3;", consuming until
  // (but not including) the closing '}' or EOF.  You must delete the return
  // pointer.

  // ParseRawDeclarations simply parses the declarations into an AST.
  Declarations* ParseRawDeclarations();

  // ParseDeclarations also runs a second pass to convert *some* syntactic
  // sugar declarations such as background, font and font-family.
  // Currently, both the expanded properties (such as background-color) and the
  // original property (background) are stored because the impl. is incomplete.
  // For details, see parser.cc.
  Declarations* ParseDeclarations();

  // Expand the values of shorthand declarations. Currently expands background
  // and font. Clears (but does not delete) input orig_declarartions in the
  // process. orig_declarations should be a std::vector of NULLs on exit.
  Declarations* ExpandDeclarations(Declarations* orig_declarations);

  // Starting at the first simple selector or whitespace, ParseSelectors parses
  // a sequence of selectors. Return NULL if the parsing fails. The parser would
  // consume anything up to the declaration starting '{' or the end of document.
  Selectors* ParseSelectors();

  // Parse the document as a single @import statement. If it's not exactly
  // one of these, or there's a syntax error, NULL is returned. Added for
  // mod_pagespeed's conversion to a link of this inside a style element.
  Import* ParseAsSingleImport();

  // Extract the leading @charset from the document. The return value is
  // valid iff it is not empty -and- errors_seen_mask() is zero. Added so
  // that mod_pagespeed can determine the charset of a CSS file without
  // duplicating a ton of our code.
  UnicodeText ExtractCharset();

  // current position in the parse.
  const char* getpos() const { return in_; }

  // Done with the parse?
  bool Done() const { return in_ == end_; }

  // Whether quirks mode (the default) is used in parsing. Standard compliant
  // (non-quirks) mode is stricter in color parsing, where a form of "rrgbbb"
  // without a leading # is not allowed.
  bool quirks_mode() const { return quirks_mode_; }
  void set_quirks_mode(bool quirks_mode) { quirks_mode_ = quirks_mode; }

  // In preservation mode (default off) we attempt to parse and store as much
  // info as possible from the stylesheet. We avoid value validation and allow
  // all parseable values. In addition for some constructs that cannot be
  // parsed, we store verbatim bytes which can be re-serialized back out.
  bool preservation_mode() const { return preservation_mode_; }
  void set_preservation_mode(bool x) { preservation_mode_ = x; }

  // This is a bitmask of errors seen during the parse.  This is decidedly
  // incomplete --- there are definitely many errors that are not reported here.
  static const uint64 kNoError          = 0;
  static const uint64 kUtf8Error        = 1ULL << 0; // 1
  static const uint64 kDeclarationError = 1ULL << 1; // 2
  static const uint64 kSelectorError    = 1ULL << 2; // 4
  static const uint64 kFunctionError    = 1ULL << 3; // 8
  static const uint64 kMediaError       = 1ULL << 4; // 16
  static const uint64 kCounterError     = 1ULL << 5; // 32
  static const uint64 kHtmlCommentError = 1ULL << 6; // 64
  static const uint64 kValueError       = 1ULL << 7; // 128
  static const uint64 kRulesetError     = 1ULL << 8; // 256
  static const uint64 kSkippedTokenError = 1ULL << 9; // 512
  static const uint64 kCharsetError     = 1ULL << 10; // 1024
  static const uint64 kBlockError       = 1ULL << 11; // 2048
  static const uint64 kNumberError      = 1ULL << 12; // 4096
  static const uint64 kImportError      = 1ULL << 13; // 8192
  static const uint64 kAtRuleError      = 1ULL << 14; // 16384
  uint64 errors_seen_mask() const { return errors_seen_mask_; }
  uint64 unparseable_sections_seen_mask() const {
    return unparseable_sections_seen_mask_;
  }

  static const int kMaxErrorsRemembered = 16;
  struct ErrorInfo {
    int error_num;
    int byte_offset;
    string message;
  };
  // A vector of first kNumErrorsRemembered errors seen.
  const std::vector<ErrorInfo> errors_seen() const { return errors_seen_; }

  // Returns the error number based on the error flag.
  //   Ex: ErrorNumber(kUtf8Error) == 0,
  //       ErrorNumber(kDeclarationError) == 1, etc.
  static int ErrorNumber(uint64 error_flag);

  friend class ParserTest;  // we need to unit test private Parse functions.

 private:
  //
  // Syntactic methods
  //

  // SkipSpace() skips whitespace ([ \t\r\n\f]) and comments
  // (/* .... */) until we reach a non-whitespace, non-comment
  // character, or the end of the document.
  void SkipSpace();

  // Starting at /*, SkipComment() skips past the matching */ or to
  // the end of the document.
  void SkipComment();

  // Skips following characters until delimiter delim or end is seen, delim is
  // consumed if found. Be smart enough not to stop at character delim in
  // comments.  Returns whether delim is actually seen.
  bool SkipPastDelimiter(char delim);

  // Skip until next "any" token (value which can be parsed by ParseAny).
  //
  // Skips whitespace, comments, blocks ({..}), and @tokens, and returns true
  // unless we are at the end of the document or the next character is a token
  // ending delimiter ([;}!]).
  bool SkipToNextAny();

  //
  // Parse functions.
  //
  // When the comment reads 'starting at foo', it's a dchecked runtime
  // error to call the function if the input does not start with
  // 'foo'.
  //
  // If a ParseXXX method returns a pointer, you own it and must
  // delete it.

  //
  // 'leaves' of the parse tree: strings, urls, identifiers, numbers,
  // etc
  //

  // ParseIdent() consumes the identifier and returns its unescaped
  // representation.  If we are at the end of the document, or if no
  // identifier is found, ParseIdent() returns the empty string.
  //
  // In CSS2, identifiers (including element names, classes, and IDs in
  // selectors) can contain only the characters [A-Za-z0-9] and ISO
  // 10646 characters 161 and higher, plus the hyphen (-); they cannot
  // start with a hyphen or a digit. They can also contain escaped
  // characters and any ISO 10646 character as a numeric code (see next
  // item). For instance, the identifier "B&W?" may be written as
  // "B\&W\?" or "B\26 W\3F".
  // http://www.w3.org/TR/REC-CSS2/syndata.html#value-def-identifier
  //
  // We're a little more forgiving than the standard and permit hyphens
  // and digits to start identifiers.
  // This method does not skip spaces like most other methods do, because it
  // may be used to identify things like "import" in "@import", which is
  // different from "@ import".
  //
  // You may supply a string of additional allowed_chars. For example,
  // there are many IE proprietary declarations whose values contain ':'
  // Ex: filter:progid:DXImageTransform.Microsoft.Alpha(Opacity=80)
  UnicodeText ParseIdent(const StringPiece& allowed_chars = "");

  // Starting at \, parse the escape and return the corresponding
  // unicode codepoint.  If the \ is the last character in the
  // document, we return '\'; there is no other malformed input.  This
  // implements the second and third types of character escapes at
  // http://www.w3.org/TR/REC-CSS2/syndata.html#escaped-characters
  //
  // 2) It cancels the meaning of special CSS characters. Any
  // character (except a hexadecimal digit) can be escaped with a
  // backslash to remove its special meaning.  For example,
  // ParseEscape() returns 0x6240 for \所 and 71 for \G (but \C is a
  // hex escape, treated below:)
  //
  // 3) Backslash escapes allow authors to refer to characters
  // they can't easily put in a document. In this case, the backslash
  // is followed by at most six hexadecimal digits (0..9A..Fa..f), which
  // stand for the ISO 10646 ([ISO10646]) character with that
  // number. If a digit or letter follows the hexadecimal number, the
  // end of the number needs to be made clear. There are two ways to
  // do that:
  //    1. with a space (or other whitespace character): "\26 B" ("&B")
  //    2. by providing exactly 6 hexadecimal digits: "\000026B" ("&B")
  //
  // So, if the escape sequence is a hex escape and the character following
  // the last hex digit is a space, then ParseEscape() consumes it.
  //
  // Only interchange valid Unicode characters will be returned.
  // all other characters will be replaced with space (" ") and
  // a kUtf8Error will be recorded in errors_seen_mask_.
  char32 ParseEscape();  // return the codepoint for the current escape \12a76f

  // Starting at delim, ParseString<char delim>() consumes the string,
  // including the matching end-delim, and returns its unescaped
  // representation, without the delimiters.  If we fail to find the
  // matching delimiter, we consume the rest of the document and
  // return it.
  //
  // Strings can either be written with double quotes or with single
  // quotes. Double quotes cannot occur inside double quotes, unless
  // escaped (as '\"' or as '\22'). Analogously for single quotes
  // ("\'" or "\27").  A string cannot directly contain a newline,
  // unless hex-escaped as "\A".
  //
  // It is possible to break strings over several lines, for aesthetic
  // or other reasons, but in such a case the newline itself has to be
  // escaped with a backslash (\). For instance, the following two
  // selectors are exactly the same:
  // http://www.w3.org/TR/REC-CSS2/syndata.html#strings
  template<char delim> UnicodeText ParseString();

  // If the current character is a string-delimiter (' or "),
  // ParseStringOrIdent() parses a string and returns the contents.
  // Otherwise, it tries to parse an identifier.  We must not be at
  // the end of the document.
  UnicodeText ParseStringOrIdent();

  // ParseNumber parses a number and an optional unit, consuming to
  // the end of the number or unit and returning a Value*.
  // Real numbers and integers are specified in decimal notation
  // only. An <integer> consists of one or more digits "0" to "9". A
  // <number> can either be an <integer>, or it can be zero or more
  // digits followed by a dot (.) followed by one or more digits. Both
  // integers and real numbers may be preceded by a "-" or "+" to
  // indicate the sign.
  //
  // If no number is found, ParseNumber returns NULL.
  Value* ParseNumber();

  // ParseColor parses several different representations of colors:
  // 1) rgb
  // 2) #rgb
  // 3) rrggbb
  // 4) #rrggbb
  // 5) The 16 HTML4 color names (aqua, black, blue,
  //    fuchsia, gray, green, lime, maroon, navy, olive, purple, red,
  //    silver, teal, white, and yellow), with or without quotes (' or ").
  // It's designed to handle all the ill-formed CSS color values out there.
  // It consumes the color if it finds a valid color.  Otherwise, it returns
  // an undefined HtmlColor (HtmlColor::IsDefined()) and does not consume
  // anything.
  //
  // However, if quirks_mode_ is false (standard compliant mode), forms 1 and 3
  // (without #) would not be accepted.
  HtmlColor ParseColor();                          // parse a hex or named
                                                   // color like #fff, #bcdefa
                                                   // or black

  //
  // FUNCTIONS and FUNCTION-like objects: rgb(), url(), rect()
  //

  // Parse a generic list of function parameters.
  //
  // Specifically, starting after the opening '(', repeatedly ParseAny() as
  // values either comma or space separated until we reach the closing ')'.
  //
  // ParseFunction() does not consume closing ')' and returns a vector of
  // values if successful, and NULL if the contents were mal-formed.
  FunctionParameters* ParseFunction();

  // Converts a Value number or percentage to an RGB value.
  static unsigned char ValueToRGB(Value* v);

  // ParseRgbColor parsers the part between the parentheses of rgb( )
  // according to http://www.w3.org/TR/REC-CSS2/syndata.html#color-units .
  //
  //  The format of an RGB value in the functional notation is 'rgb('
  //  followed by a comma-separated list of three numerical values
  //  (either three integer values or three percentage values)
  //  followed by ')'. The integer value 255 corresponds to 100%, and
  //  to F or FF in the hexadecimal notation: rgb(255,255,255) =
  //  rgb(100%,100%,100%) = #FFF. Whitespace characters are allowed
  //  around the numerical values.
  //
  // Starting just past 'rgb(', ParseRgbColor() consumes up to (but not
  // including) the closing ) and returns the color it finds.
  // Returns NULL if mal-formed.
  Value* ParseRgbColor();   // parse an rgbcolor like 125, 25, 12
                            // or 12%, 57%, 89%

  // ParseUrl parses the part between the parentheses of url( )
  // according to http://www.w3.org/TR/REC-CSS2/syndata.html#uri .
  //
  //  The format of a URI value is 'url(' followed by optional
  //  whitespace followed by an optional single quote (') or double
  //  quote (") character followed by the URI itself, followed by an
  //  optional single quote (') or double quote (") character followed
  //  by optional whitespace followed by ')'. The two quote characters
  //  must be the same.
  //
  // Starting just past 'url(', ParseUrl() consumes the url as well as
  // the optional whitespace.  If the url is well-formed, the next
  // character must be ')'.
  // Returns NULL for mal-formed URLs.
  Value* ParseUrl();      // parse a url like yellow.png or 'blah.png'

  //
  // Value and Values
  //

  // Parses a value which is expected to be color values. It can be
  // different from ParseAny, for example, for black or ccddff, both
  // are translated into color values here but are returned as idents
  // in the latter case.  We call this instead of ParseAny() after
  // color, background-color, and background properties to accomodate bad CSS.
  // If no value is found, ParseAnyExpectingColor returns NULL.
  Value* ParseAnyExpectingColor(const StringPiece& allowed_chars = "");

  // ParseAny() parses a css value and consumes it.  It does not skip
  // leading or trailing whitespace.
  // If no value is found, ParseAny returns NULL and make sure at least one
  // character is consumed (to make progress).
  Value* ParseAny(const StringPiece& allowed_chars = "");

  // Parse a list of values for the given property.
  // We parse until we see a !, ;, or } delimiter. However, if there are any
  // malformed values, stop parsing and return NULL immediately.
  // For special shortcut properties, use the following specialized methods
  // instead.
  Values* ParseValues(Property::Prop prop);

  // Expand a background property into all the sub-properties (background-color,
  // background-image, etc.). Return false on malformed original_declaration.
  static bool ExpandBackground(const Declaration& original_declaration,
                               Declarations* new_declarations);

  // Parses FONT. Returnss NULL if malformed. Otherwise, the output is a tuple
  // in the following order
  //   "font-style font-variant font-weight font-size line-height font-family+"
  Values* ParseFont();

  // Parses FONT-FAMILY and the tailing part in FONT and appends the results in
  // values. Returns false if there are any malformed values.
  // This interface is different from the others because it is also used by
  // ParseFont(), where family names are appended to other CSS values.
  bool ParseFontFamily(Values* values);

  //
  // Selectors and Rulesets
  //

  // ParseAttributeSelector() starts at [ and parses an attribute
  // selector like [ foo ~= bar], consuming the final ].  Returns NULL
  // on error but still consumes to the matching ].
  // This method does not skip spaces like most other methods do.
  // Whitespace is syntactically significant here, because a sequence of simple
  // selectors contains no whitespace.  'div[align=center]' is a sequence of
  // simple selectors, but 'div [align=center]' is a syntax error (though we
  // will parse it as a selector, i.e., two simple selector sequences separated
  // by a whitespace combinator).
  SimpleSelector* ParseAttributeSelector();

  // ParseSimpleSelector() parses one simple sector.  Starts from
  // anything and returns NULL if no simple selector found or parse error.
  // This method does not skip spaces like most other methods do.
  // See comment above.
  SimpleSelector* ParseSimpleSelector();

  // Checks if the parser stops at a character (or characters) that will
  // legally terminate a SimpleSelectors. The checked characters are not eaten.
  // Valid terminators are whitespaces, comments, combinators ('>', '+'), ','
  // and '{'. A stop at the end is also considered valid.
  bool AtValidSimpleSelectorsTerminator() const;

  // Starting at whitespace, a combinator, or the first simple
  // selector, ParseSimpleSelectors parses a sequence of simple
  // selectors, i.e., a chain of simple selectors that are not
  // separated by a combinator.  The chain itself may be preceeded by
  // a combinator, in which case you should pass true for
  // expecting_combinator, and we will parse the combinator.
  // Typically, when you're parsing a selector (i.e., a chain of
  // sequences of simple selectors separated by combinators), you pass
  // false on the first simple selector and true on the subsequent
  // ones.
  SimpleSelectors* ParseSimpleSelectors(bool expecting_combinator);

  // ParseRuleset() starts from the first character of the first
  // selector (note: it does not skip whitespace) and consumes the
  // ruleset, including the closing '}'. Return NULL if the parsing fails.
  // However, the parser would consume anything up to the closing '}', if any,
  // even if it fails somehow in the middle, per CSS spec.
  //
  // Note: In preservation mode, a ruleset may be returned even if selectors
  // could not be parsed. If this happens the selectors.is_dummy() will be true.
  Ruleset* ParseRuleset();

  //
  // Miscellaneous
  //

  // Starting at whitespace or the first medium, ParseMediumList
  // parses a medium list separated by commas and whitespace, stopping
  // at (without consuming) ; or {.  It adds each medium to the back of media.
  void ParseMediumList(std::vector<UnicodeText>* media);

  // ParseImport starts just after @import and consumes the import
  // declaration, including the closing ;.  It returns a Import*
  // containing the imported name and the media.
  Import* ParseImport();

  // Starting at @, ParseAtRule parses @import, @charset, and @medium
  // declarations and adds the information to the stylesheet.
  //
  // For other (unsupported) at-keywords (like @font-face or @keyframes),
  // we set an error and skip over and ignore the entire at-rule.
  // TODO(sligocki): In preservation mode, we should save a dummy at-rule
  // type for passing through verbatim bytes.
  //
  // Consumes the @-rule, including the closing ';' or '}'.  Does not
  // consume trailing whitespace.
  void ParseAtRule(Stylesheet* stylesheet);  // parse @ rules.

  // Parse the charset after an @charset rule.
  UnicodeText ParseCharset();

  // Skip until the end of the at-rule. Used for at-rules that we do not
  // recognize.
  //
  // From http://www.w3.org/TR/CSS2/syndata.html#parsing-errors:
  //
  //   At-rules with unknown at-keywords. User agents must ignore an invalid
  //   at-keyword together with everything following it, up to the end of the
  //   block that contains the invalid at-keyword, or up to and including the
  //   next semicolon (;), or up to and including the next block ({...}),
  //   whichever comes first.
  void SkipToAtRuleEnd();

  // Starting at '{', SkipBlock consumes to the matching '}', respecting
  // nested blocks.  We discard the result.
  void SkipBlock();

  // Current position in document (bytes from beginning).
  int CurrentOffset() const { return in_ - begin_; }

  static const int kErrorContext;

  // error_flag should be one of the static const k*Error's above.
  void ReportParsingError(uint64 error_flag, const StringPiece& message);

  const char *begin_;  // The beginning of the doc (used to report offset).
  const char *in_;     // The current point in the parse.
  const char *end_;    // The end of the document to parse.

  bool quirks_mode_;  // Whether we are in quirks mode.
  // In preservation mode, we attempt to save all information from the
  // stylesheet (including unparseable constructs such as proprietary CSS
  // and CSS hacks) so that they can be re-serialized precisely.
  bool preservation_mode_;
  // errors_seen_mask_ is non-zero iff we failed to parse part of the CSS
  // and could not recover and so we have lost information.
  uint64 errors_seen_mask_;
  // Only set in preservation_mode_. unparseable_sections_seen_mask_ is non-zero
  // iff we failed to parse a section of CSS, but saved the text verbatim or
  // in some other way preserved the information from the original document.
  uint64 unparseable_sections_seen_mask_;
  // Vector of all errors { error_type_number, location, message }.
  std::vector<ErrorInfo> errors_seen_;

  FRIEND_TEST(ParserTest, color);
  FRIEND_TEST(ParserTest, url);
  FRIEND_TEST(ParserTest, rect);
  FRIEND_TEST(ParserTest, background);
  FRIEND_TEST(ParserTest, font_family);
  friend void ParseFontFamily(Parser* parser);
  FRIEND_TEST(ParserTest, ParseBlock);
  FRIEND_TEST(ParserTest, font);
  FRIEND_TEST(ParserTest, values);
  FRIEND_TEST(ParserTest, declarations);
  FRIEND_TEST(ParserTest, universalselector);
  FRIEND_TEST(ParserTest, universalselectorcondition);
  FRIEND_TEST(ParserTest, comment_breaking_descendant_combinator);
  FRIEND_TEST(ParserTest, comment_breaking_child_combinator);
  FRIEND_TEST(ParserTest, simple_selectors);
  FRIEND_TEST(ParserTest, bad_simple_selectors);
  FRIEND_TEST(ParserTest, rulesets);
  FRIEND_TEST(ParserTest, ruleset_starts_with_combinator);
  FRIEND_TEST(ParserTest, atrules);
  FRIEND_TEST(ParserTest, percentage_colors);
  FRIEND_TEST(ParserTest, ValueError);
  FRIEND_TEST(ParserTest, SkippedTokenError);
  DISALLOW_COPY_AND_ASSIGN(Parser);
};

// Definitions of various data structures returned by the parser.
// More in selector.h and value.h.

// A single declaration such as font: 12pt Arial.
// A declaration consists of a property name (Property) and a list
// of values (Values*).
// It could also be important (font: 12pt Arial !important).
class Declaration {
 public:
  // constructor.  We take ownership of v.
  Declaration(Property p, Values* v, bool important)
      : property_(p), values_(v), important_(important) {}
  // constructor with a single Value. We make a copy of the value.
  Declaration(Property p, const Value& v, bool important)
      : property_(p), values_(new Values), important_(important) {
    values_->push_back(new Value(v));
  }
  // Constructor for dummy declaration used to pass through unparseable
  // declaration text.
  explicit Declaration(const StringPiece& bytes_in_original_buffer)
      : property_(Property::UNPARSEABLE), important_(false),
        bytes_in_original_buffer_(bytes_in_original_buffer.data(),
                                  bytes_in_original_buffer.length()) {}

  // accessors
  Property property() const { return property_; }
  const Values* values() const { return values_.get(); }
  bool IsImportant() const { return important_; }

  // Note: May be invalid UTF8.
  StringPiece bytes_in_original_buffer() const {
    return bytes_in_original_buffer_;
  }
  void set_bytes_in_original_buffer(const StringPiece& new_bytes) {
    bytes_in_original_buffer_ = string(new_bytes.data(), new_bytes.length());
  }

  // convenience accessors
  Property::Prop prop() const { return property_.prop(); }
  string prop_text() const { return property_.prop_text(); }

  Values* mutable_values() { return values_.get(); }

  void set_property(Property property) { property_ = property; }
  // Takes ownership of values.
  void set_values(Values* values) { values_.reset(values); }
  void set_important(bool important) { important_ = important; }

  string ToString() const;

 private:
  Property property_;
  scoped_ptr<Values> values_;
  bool important_;  // Whether !important is declared on this declaration.

  // Verbatim bytes parsed for the declaration. Currently this is only stored
  // for unparseable declarations (stored with property_ == UNPARSEABLE).
  // TODO(sligocki): We may want to store verbatim text for all declarations
  // to preserve the details of the original text.
  string bytes_in_original_buffer_;

  DISALLOW_COPY_AND_ASSIGN(Declaration);
};

// Declarations is a vector of Declaration*, which we own and
// will delete upon destruction.  If you remove elements from
// Declarations, you are responsible for deleting them.
// Also, be careful --- there's no virtual destructor, so this must be
// deleted as a Declarations.
class Declarations : public std::vector<Declaration*> {
 public:
  Declarations() : std::vector<Declaration*>() { }
  ~Declarations();

  // We provide syntactic sugar for accessing elements.
  // declarations->get(i) looks better than (*declarations)[i])
  const Declaration* get(int i) const { return (*this)[i]; }

  string ToString() const;
 private:
  DISALLOW_COPY_AND_ASSIGN(Declarations);
};

// Unparsed sections of CSS file. For example, unexpected @-rules cannnot be
// parsed, so we simply collect the verbatim bytes from start to finish and
// store them in an UnparsedRegion so that they can be re-emitted in
// preservation mode.
class UnparsedRegion {
 public:
  explicit UnparsedRegion(const StringPiece& bytes_in_original_buffer)
      : bytes_in_original_buffer_(bytes_in_original_buffer.data(),
                                  bytes_in_original_buffer.size()) {}

  StringPiece bytes_in_original_buffer() const {
    return bytes_in_original_buffer_;
  }

  void set_bytes_in_original_buffer(const StringPiece& bytes) {
    bytes.CopyToString(&bytes_in_original_buffer_);
  }

  string ToString() const;

 private:
  string bytes_in_original_buffer_;

  DISALLOW_COPY_AND_ASSIGN(UnparsedRegion);
};

// A ruleset consists of a list of selectors followed by a declaration block.
// It can also optionally include a list of medium description.
//
// Unparsed regions between Rulesets can also be stored here in preservation
// mode. For example, at-rules can be interspersed with Rulesets, for those
// that we don't parse, they are stored in dummy Rulesets.
class Ruleset {
 public:
  // TODO(sligocki): Allow other parsed at-rules, like @font-family.
  enum Type { RULESET, UNPARSED_REGION, };

  Ruleset() : type_(RULESET), selectors_(new Selectors),
              declarations_(new Declarations) { }
  // Takes ownership of selectors and declarations.
  Ruleset(Selectors* selectors, const std::vector<UnicodeText>& media,
          Declarations* declarations)
      : type_(RULESET), selectors_(selectors), media_(media),
        declarations_(declarations) { }
  // Dummy Ruleset. Used for unparsed statements, for example unknown at-rules.
  explicit Ruleset(UnparsedRegion* unparsed_region)
      : type_(UNPARSED_REGION), unparsed_region_(unparsed_region) { }
  ~Ruleset() { }

  // Is this actually a Ruleset or some sort of at-rule? For historical reasons
  // at-rules are also stored as Rulesets.
  Type type() const { return type_; }

  // NOTE: Only call these getters if you know that type() == RULESET.
  // type() always == RULESET if Css::Parser::preservation_mode() is false,
  // so getters should all be valid if preservation mode is off (default).
  const Selectors& selectors() const {
    CHECK_EQ(RULESET, type());
    return *selectors_;
  }
  const Selector& selector(int i) const {
    CHECK_EQ(RULESET, type());
    return *selectors_->at(i);
  }
  const std::vector<UnicodeText>& media() const {
    CHECK_EQ(RULESET, type());
    return media_;
  }
  const UnicodeText& medium(int i) const {
    CHECK_EQ(RULESET, type());
    return media_.at(i);
  }
  const Declarations& declarations() const {
    CHECK_EQ(RULESET, type());
    return *declarations_;
  }
  const Declaration& declaration(int i) const {
    CHECK_EQ(RULESET, type());
    return *declarations_->at(i);
  }

  Selectors& mutable_selectors() {
    CHECK_EQ(RULESET, type());
    return *selectors_;
  }
  std::vector<UnicodeText>& mutable_media() {
    CHECK_EQ(RULESET, type());
    return media_;
  }
  Declarations& mutable_declarations() {
    CHECK_EQ(RULESET, type());
    return *declarations_;
  }

  // set_media copies input media.
  void set_media(const std::vector<UnicodeText>& media) {
    CHECK_EQ(RULESET, type());
    media_.assign(media.begin(), media.end());
  }
  // set_selectors and _declarations take ownership of parameters.
  void set_selectors(Selectors* selectors) {
    CHECK_EQ(RULESET, type());
    selectors_.reset(selectors);
  }
  void set_declarations(Declarations* decls) {
    CHECK_EQ(RULESET, type());
    declarations_.reset(decls);
  }

  // If type() == UNPARSED_REGION, this is the link to that region.
  const UnparsedRegion* unparsed_region() const {
    CHECK_EQ(UNPARSED_REGION, type());
    return unparsed_region_.get();
  }
  UnparsedRegion* mutable_unparsed_region() {
    CHECK_EQ(UNPARSED_REGION, type());
    return unparsed_region_.get();
  }

  string ToString() const;
 private:
  Type type_;

  scoped_ptr<Selectors> selectors_;
  std::vector<UnicodeText> media_;
  scoped_ptr<Declarations> declarations_;

  scoped_ptr<UnparsedRegion> unparsed_region_;

  DISALLOW_COPY_AND_ASSIGN(Ruleset);
};

class Rulesets : public std::vector<Css::Ruleset*> {
 public:
  Rulesets() : std::vector<Css::Ruleset*>() { }
  ~Rulesets();
};

class Charsets : public std::vector<UnicodeText> {
 public:
  ~Charsets();

  string ToString() const;
};

struct Import {
  std::vector<UnicodeText> media;
  UnicodeText link;

  string ToString() const;
};

class Imports : public std::vector<Css::Import*> {
 public:
  Imports() : std::vector<Css::Import*>() { }
  ~Imports();
};

// A stylesheet consists of a list of import information and a list of
// rulesets.
class Stylesheet {
 public:
  Stylesheet() : type_(AUTHOR) {}

  // USER is currently unused.
  enum StylesheetType { AUTHOR, USER, SYSTEM };
  StylesheetType type() const { return type_; }
  const Charsets& charsets() const { return charsets_; }
  const Imports& imports() const { return imports_; }
  const Rulesets& rulesets() const { return rulesets_; }

  const UnicodeText& charset(int i) const { return charsets_[i]; }
  const Import& import(int i) const { return *imports_[i]; }
  const Ruleset& ruleset(int i) const { return *rulesets_[i]; }

  void set_type(StylesheetType type) { type_ = type; }
  Charsets& mutable_charsets() { return charsets_; }
  Imports& mutable_imports() { return imports_; }
  Rulesets& mutable_rulesets() { return rulesets_; }

  string ToString() const;
 private:
  StylesheetType type_;
  Charsets charsets_;
  Imports imports_;
  // Note: CSS spec specifies that a stylesheet is a list of statements each
  // of which is either a ruleset or at-rule. Since we want to support the
  // legacy rulesets() interface and most at-rules are not parsed, at-rules
  // are currently being stored as dummy rulesets.
  Rulesets rulesets_;

  DISALLOW_COPY_AND_ASSIGN(Stylesheet);
};

}  // namespace

#endif  // WEBUTIL_CSS_PARSER_H__
