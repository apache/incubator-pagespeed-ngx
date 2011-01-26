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

  // current position in the parse.
  const char* getpos() const { return in_; }

  // Done with the parse?
  bool Done() const { return in_ == end_; }

  // Whether quirks mode (the default) is used in parsing. Standard compliant
  // (non-quirks) mode is stricter in color parsing, where a form of "rrgbbb"
  // without a leading # is not allowed.
  bool quirks_mode() const { return quirks_mode_; }
  void set_quirks_mode(bool quirks_mode) { quirks_mode_ = quirks_mode; }

  // This is a bitmask of errors seen during the parse.  This is decidedly
  // incomplete --- there are definitely many errors that are not reported here.
  static const uint64 kNoError          = 0;
  static const uint64 kUtf8Error        = 1ULL << 0;
  static const uint64 kDeclarationError = 1ULL << 1;
  static const uint64 kSelectorError    = 1ULL << 2;
  static const uint64 kFunctionError    = 1ULL << 3;
  static const uint64 kMediaError       = 1ULL << 4;
  static const uint64 kCounterError     = 1ULL << 5;
  uint64 errors_seen_mask() const { return errors_seen_mask_; }

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

  // Skips whitespace, comments, blocks ({..}), and @tokens, and returns true
  // unless we are at the end of the document or the next character is a token
  // ending delimiter ([;}!]).
  bool SkipToNextToken();

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
  UnicodeText ParseIdent();  // parse an identifier like justify

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
  // FUNCTION-like objects: rgb(), url(), rect()
  //

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
  // Starting just past 'rgb(', ParseColor() consumes up to (but not
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

  // Parses between the parentheses of rect(top, right, bottom, left).
  //
  // The contents should be a comma or space separated list of four numerical
  // values or the keyword "auto". Note that spaces are allowed to separate
  // values for historical reasons.
  //
  // Returns NULL if the contents is not well-formed.
  Value* ParseRect();  // parse rect(top, right, bottom, left)

  //
  // Value and Values
  //

  // Parses a value which is expected to be color values. It can be
  // different from ParseAny, for example, for black or ccddff, both
  // are translated into color values here but are returned as idents
  // in the latter case.  We call this instead of ParseAny() after
  // color, background-color, and background properties to accomodate bad CSS.
  // If no value is found, ParseAnyExpectingColor returns NULL.
  Value* ParseAnyExpectingColor();

  // ParseAny() parses a css value and consumes it.  It does not skip
  // leading or trailing whitespace.
  // If no value is found, ParseAny returns NULL and make sure at least one
  // character is consumed (to make progress).
  Value* ParseAny();  // parse a value, which is pretty much anything.

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

  // Starting at @, ParseAtrule parses @import, @charset, @medium, and
  // @page declarations and adds the information to the stylesheet.
  // Consumes the @-rule, including the closing ';' or '}'.  Does not
  // consume trailing whitespace.
  void ParseAtrule(Stylesheet* stylesheet);  // parse @ rules.

  // Starting at '{', ParseBlock consumes to the matching '}', respecting
  // nested blocks.  We discard the result.
  void ParseBlock();

  const char *in_;   // The current point in the parse.
  const char *end_;  // The end of the document to parse.

  bool quirks_mode_;  // Whether we are in quirks mode.
  uint64 errors_seen_mask_;

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
  Declaration(Property p, Values* v, bool important) :
      property_(p), values_(v), important_(important) { }
  // constructor with a single Value. We make a copy of the value.
  Declaration(Property p, const Value& v, bool important)
      : property_(p), values_(new Values), important_(important) {
    values_->push_back(new Value(v));
  }

  // accessors
  Property property() const { return property_; }
  const Values* values() const { return values_.get(); }
  bool IsImportant() const { return important_; }

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

// A ruleset consists of a list of selectors followed by a declaration block.
// It can also optionally include a list of medium description.
class Ruleset {
 public:
  Ruleset() : selectors_(new Selectors), declarations_(new Declarations) { }
  // Takes ownership of selectors and declarations.
  Ruleset(Selectors* selectors, const std::vector<UnicodeText>& media,
          Declarations* declarations)
      : selectors_(selectors), media_(media), declarations_(declarations) { }
  ~Ruleset() { }

  const Selectors& selectors() const { return *selectors_; }
  const Selector& selector(int i) const { return *selectors_->at(i); }
  const std::vector<UnicodeText>& media() const { return media_; }
  const UnicodeText& medium(int i) const { return media_.at(i); }
  const Declarations& declarations() const { return *declarations_; }
  const Declaration& declaration(int i) const { return *declarations_->at(i); }

  Selectors& mutable_selectors() { return *selectors_; }
  Declarations& mutable_declarations() { return *declarations_; }

  // set_media copies input media.
  void set_media(const std::vector<UnicodeText>& media) {
    media_.assign(media.begin(), media.end());
  }
  // set_selectors and _declarations take ownership of parameters.
  void set_selectors(Selectors* selectors) { selectors_.reset(selectors); }
  void set_declarations(Declarations* decls) { declarations_.reset(decls); }

  string ToString() const;
 private:
  scoped_ptr<Selectors> selectors_;
  std::vector<UnicodeText> media_;
  scoped_ptr<Declarations> declarations_;

  DISALLOW_COPY_AND_ASSIGN(Ruleset);
};

class Rulesets : public std::vector<Css::Ruleset*> {
 public:
  Rulesets() : std::vector<Css::Ruleset*>() { }
  ~Rulesets();
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
  const Imports& imports() const { return imports_; }
  const Rulesets& rulesets() const { return rulesets_; }

  const Import& import(int i) const { return *imports_[i]; }
  const Ruleset& ruleset(int i) const { return *rulesets_[i]; }

  void set_type(StylesheetType type) { type_ = type; }
  Imports& mutable_imports() { return imports_; }
  Rulesets& mutable_rulesets() { return rulesets_; }

  string ToString() const;
 private:
  StylesheetType type_;
  Imports imports_;
  Rulesets rulesets_;

  DISALLOW_COPY_AND_ASSIGN(Stylesheet);
};

}  // namespace

#endif  // WEBUTIL_CSS_PARSER_H__
