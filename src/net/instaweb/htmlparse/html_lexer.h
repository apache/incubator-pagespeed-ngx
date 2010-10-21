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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_HTMLPARSE_HTML_LEXER_H_
#define NET_INSTAWEB_HTMLPARSE_HTML_LEXER_H_

#include <stdarg.h>
#include <set>
#include <vector>
#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/printf_format.h"
#include <string>

namespace net_instaweb {

// Constructs a re-entrant HTML lexer.  This lexer minimally parses tags,
// attributes, and comments.  It is intended to parse the Wild West of the
// Web.  It's designed to be tolerant of syntactic transgressions, merely
// passing through unparseable chunks as Characters.
//
// TODO(jmarantz): refactor this with html_parse, so that this class owns
// the symbol table and the event queue, and no longer needs to mutually
// depend on HtmlParse.  That will make it easier to unit-test.
class HtmlLexer {
 public:
  explicit HtmlLexer(HtmlParse* html_parse);
  ~HtmlLexer();

  // Initialize a new parse session, id is only used for error messages.
  void StartParse(const StringPiece& id);

  // Parse a chunk of text, adding events to the parser by calling
  // html_parse_->AddEvent(...).
  void Parse(const char* text, int size);

  // Completes parse, reporting any leftover text as a final HtmlCharacterEvent.
  void FinishParse();

  // Determines whether a tag should be terminated in HTML.
  bool IsImplicitlyClosedTag(Atom tag) const;

  // Determines whether a tag can be terminated briefly (e.g. <tag/>)
  bool TagAllowsBriefTermination(Atom tag) const;

  // Print element stack to stdout (for debugging).
  void DebugPrintStack();

  // Returns the current lowest-level parent element in the element stack
  HtmlElement* Parent() const;

 private:
  void EvalStart(char c);
  void EvalTag(char c);
  void EvalTagOpen(char c);
  void EvalTagClose(char c);
  void EvalTagCloseTerminate(char c);
  void EvalTagBriefClose(char c);
  void EvalTagBriefCloseAttr(char c);
  void EvalCommentStart1(char c);
  void EvalCommentStart2(char c);
  void EvalCommentBody(char c);
  void EvalCommentEnd1(char c);
  void EvalCommentEnd2(char c);
  void EvalCdataStart1(char c);
  void EvalCdataStart2(char c);
  void EvalCdataStart3(char c);
  void EvalCdataStart4(char c);
  void EvalCdataStart5(char c);
  void EvalCdataStart6(char c);
  void EvalCdataBody(char c);
  void EvalCdataEnd1(char c);
  void EvalCdataEnd2(char c);
  void EvalAttribute(char c);
  void EvalAttrName(char c);
  void EvalAttrEq(char c);
  void EvalAttrVal(char c);
  void EvalAttrValSq(char c);
  void EvalAttrValDq(char c);
  void EvalLiteralTag(char c);
  void EvalDirective(char c);

  void MakeElement();
  void MakeAttribute(bool has_value);
  void FinishAttribute(char c, bool has_value, bool brief_close);

  void EmitCdata();
  void EmitComment();
  void EmitLiteral();
  void EmitTagOpen(bool allow_implicit_close);
  void EmitTagClose(HtmlElement::CloseStyle close_style);
  void EmitTagBriefClose();
  void EmitDirective();

  // Emits an error message.
  void Warning(const char* format, ...) INSTAWEB_PRINTF_FORMAT(2, 3);

  // Takes an interned tag, and tries to find a matching HTML element on
  // the stack.  If it finds it, it pops all the intervening elements off
  // the stack, issuing warnings for each discarded tag, the matching element
  // is also popped off the stack, and returned.
  //
  // If the tag is not matched, then no mutations are done to the stack,
  // and NULL is returned.
  //
  // The tag name should be interned.
  // TODO(jmarantz): use type system
  HtmlElement* PopElementMatchingTag(Atom tag);

  HtmlElement* PopElement();
  void CloseElement(HtmlElement* element, HtmlElement::CloseStyle close_style,
                    int line_nubmer);

  // Minimal i18n analysis.  With utf-8 and gb2312 we can do this
  // context-free, and thus the method can be static.  If we add
  // more encodings we may need to turn this into a non-static method.
  static inline bool IsI18nChar(char c) {return (((c) & 0x80) != 0); }

  // Determines whether a character can be used in a tag name as first char ...
  static inline bool IsLegalTagFirstChar(char c);
  // ... or subsequent char.
  static inline bool IsLegalTagChar(char c);

  // Determines whether a character can be used in an attribute name.
  static inline bool IsLegalAttrNameChar(char c);

  // Determines whether a character can be used in an attribute value.
  static inline bool IsLegalAttrValChar(char c);

  // The lexer is implemented as a pure state machine.  There is
  // no lookahead.  The state is understood primarily in this
  // enum, although there are a few state flavors that are managed
  // by the other member variables, notably: has_attr_value_ and
  // attr_name_.empty().  Those could be eliminated by adding
  // a few more explicit states.
  enum State {
    START,
    TAG,                   // "<"
    TAG_CLOSE,             // "</"
    TAG_CLOSE_TERMINATE,   // "</x "
    TAG_OPEN,              // "<x"
    TAG_BRIEF_CLOSE,       // "<x/"
    TAG_BRIEF_CLOSE_ATTR,  // "<x /" or "<x y/" or "x y=/z" etc
    COMMENT_START1,        // "<!"
    COMMENT_START2,        // "<!-"
    COMMENT_BODY,          // "<!--"
    COMMENT_END1,          // "-"
    COMMENT_END2,          // "--"
    CDATA_START1,          // "<!["
    CDATA_START2,          // "<![C"
    CDATA_START3,          // "<![CD"
    CDATA_START4,          // "<![CDA"
    CDATA_START5,          // "<![CDAT"
    CDATA_START6,          // "<![CDATA"
    CDATA_BODY,            // "<![CDATA["
    CDATA_END1,            // "]"
    CDATA_END2,            // "]]"
    TAG_ATTRIBUTE,         // "<x "
    TAG_ATTR_NAME,         // "<x y"
    TAG_ATTR_NAME_SPACE,   // "<x y "
    TAG_ATTR_EQ,           // "<x y="
    TAG_ATTR_VAL,          // "<x y=x" value terminated by whitespace or >
    TAG_ATTR_VALDQ,        // '<x y="' value terminated by double-quote
    TAG_ATTR_VALSQ,        // "<x y='" value terminated by single-quote
    LITERAL_TAG,           // "<script " or "<iframe "
    DIRECTIVE              // "<!x"
  };

  HtmlParse* html_parse_;
  State state_;
  std::string token_;       // accmulates tag names and comments
  std::string literal_;     // accumulates raw text to pass through
  std::string attr_name_;   // accumulates attribute name
  std::string attr_value_;  // accumulates attribute value
  const char* attr_quote_;  // accumulates quote used to delimit attribute
  bool has_attr_value_;     // distinguishes <a n=> from <a n>
  HtmlElement* element_;    // current element; used to collect attributes
  int line_;
  int tag_start_line_;      // line at which we last transitioned to TAG state
  std::string id_;
  std::string literal_close_;  // specific tag go close, e.g </script>

  AtomSet implicitly_closed_;
  AtomSet non_brief_terminated_tags_;
  AtomSet literal_tags_;
  std::vector<HtmlElement*> element_stack_;

  DISALLOW_COPY_AND_ASSIGN(HtmlLexer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTMLPARSE_HTML_LEXER_H_
