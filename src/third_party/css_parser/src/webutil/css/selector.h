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

#ifndef WEBUTIL_CSS_SELECTOR_H__
#define WEBUTIL_CSS_SELECTOR_H__

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/string.h"
#include "webutil/html/htmltagenum.h"
#include "webutil/html/htmltagindex.h"

namespace Css {

// -------------
// Overview:
//
// We adopt CSS3's naming conventions:
// http://www.w3.org/TR/css3-selectors/#selector-syntax
//
// A selector is a chain of one or more sequences of simple selectors
// separated by combinators.  We represent this in the Selector
// class.
// Ex: div[align=center] > h1
//
// A sequence of simple selectors is a chain of simple selectors that
// are not separated by a combinator. It always begins with a type
// selector or a universal selector. No other type selector or
// universal selector is allowed in the sequence.  We represent this
// in the SimpleSelectors class as just a list of simple selectors;
// the simple selectors are logically AND-ed together.
// Ex: div[align=center] is a sequence of two simple selectors, 'div'
// and '[align=center]'
//
// A simple selector is either a type selector, universal selector,
// attribute selector, class selector, ID selector, content selector,
// or pseudo-class. One pseudo-element may be appended to the last
// sequence of simple selectors.  We represent this in the
// SimpleSelector class.
// Ex: [align=center]
//
// ------------

// ------------
// SimpleSelector:
//
// A SimpleSelector represents a simple selector.  We currently
// don't distinguish pseudo-elements (:first-line) from pseudo-classes
// (:hover).
//
// For example, 'div' is a simple selector of type TYPE_SELECTOR,
// matching HTML div tags.  '[align=center]' is simple selector
// of type EXACT_ATTRIBUTE, matching tags with attribute 'align' equal
// to 'center'.  If you put them together as div[align=center], you
// get a chain of simple selectors matching, e.g., <div align=center
// foo=bar>.
//
// There are several different types of simple selectors, so you can
// think of a SimpleSelector as a tagged union of various values.
// The tag is set by the factory method and accessed with type().  The
// values are also set by the factory and accessed with the various
// accessors.  Each accessor is valid with certain types.
// ------------
class SimpleSelector {
 public:
  enum Type {
    // An element type selector matches the HTML element type (e.g., h1, h2, h3)
    ELEMENT_TYPE,

    // This type of simple selector matches anything ('*').
    UNIVERSAL,

    // These types check HTML element attributes in various ways.
    EXIST_ATTRIBUTE,   // [attr]: element sets the "attr" attribute
    EXACT_ATTRIBUTE,   // [attr=val]: element's "attr" attribute value is "val".
    ONE_OF_ATTRIBUTE,  // [attr~=val]: element's "attr" attribute value is a
                       // space-separated list of words, one of which
                       // is exactly "val".
    BEGIN_HYPHEN_ATTRIBUTE,  // [attr|=val] element's "attr" attribute value
                             // is a hypen-separated list that begins
                             // with "val-".

    // New in CSS3, but well supported.
    BEGIN_WITH_ATTRIBUTE,  // [attr^=val]: attribute value starts with "val".
    END_WITH_ATTRIBUTE,    // [attr$=val]: attribute value ends with "val".
    SUBSTRING_ATTRIBUTE,   // [attr*=val]: attribute value contains "val".

    // CLASS and ID are equivalent to ONE_OF_ATTRIBUTE and
    // EXACT_ATTRIBUTE, but CSS provides special syntax for them, and
    // they're very common.  GetLocalName() returns "class" and "id"
    // for these:
    CLASS,             // .class: element's class attribute is "val"
    ID,                // #id:    the element's id attribute is "id"

    // Miscellaneous conditions:
    PSEUDOCLASS,  // a:hover matches <a href=blah> when mouse is hovering
    LANG,          // :lang(en) matches if the element is in English.

    // We don't implement these (yet).
    // AND, OR, NOT, ONLY_CHILD, ONLY_TYPE, CONTENT, POSITIONAL
    //    ROOT, TEXT, PSEUDOELEMENT, PROCESSING_INSTRUCTION,
    //    NEGATIVE, COMMENT, CDATA_SECTION,
  };

  // Factory methods to generate SimpleSelectors of various types.
  static SimpleSelector* NewElementType(const UnicodeText& name);
  static SimpleSelector* NewUniversal();
  static SimpleSelector* NewExistAttribute(const UnicodeText& attribute);
  // *_ATTRIBUTE.
  static SimpleSelector* NewBinaryAttribute(Type type,
                                            const UnicodeText& attribute,
                                            const UnicodeText& value);
  static SimpleSelector* NewClass(const UnicodeText& classname);
  static SimpleSelector* NewId(const UnicodeText& id);
  static SimpleSelector* NewPseudoclass(const UnicodeText& pseudoclass);
  static SimpleSelector* NewLang(const UnicodeText& lang);

  // oper is '=' for EXACT_ATTRIBUTE, or the first character of the attribute
  // selector operator, i.e. '~', '|', etc.
  static Type AttributeTypeFromOperator(char oper) {
    switch (oper) {
      case '=':
        return EXACT_ATTRIBUTE;
      case '~':
        return ONE_OF_ATTRIBUTE;
      case '|':
        return BEGIN_HYPHEN_ATTRIBUTE;
      case '^':
        return BEGIN_WITH_ATTRIBUTE;
      case '$':
        return END_WITH_ATTRIBUTE;
      case '*':
        return SUBSTRING_ATTRIBUTE;
      default:
        LOG(FATAL) << "Invalid attribute operator " << oper;
    }
  }

  // Accessors.
  //
  Type type() const { return type_; }  // The type of selector

  // ELEMENT_TYPE accessor
  // element_type() returns kHtmlTagUnknown if we don't recognize the tag.
  HtmlTagEnum element_type() const { return element_type_; }
  // element_text() returns the element text.  We preserve the original case.
  const UnicodeText& element_text() const { return element_text_; }

  // *_ATTRIBUTE, CLASS, ID accessors
  const UnicodeText& namespace_uri();  // Not implemented.
  const UnicodeText& attribute() const {
    DCHECK(IsAttributeCondition());
    return attribute_;
  }
  const UnicodeText& value() const {
    DCHECK(IsAttributeCondition());
    return value_;
  }
  // IsAttributeCondition indicates whether this is an attribute
  // selector, with valid attribute() and value() fields.
  bool IsAttributeCondition() const {
    return (EXIST_ATTRIBUTE == type_ || EXACT_ATTRIBUTE == type_
            || ONE_OF_ATTRIBUTE == type_ || BEGIN_HYPHEN_ATTRIBUTE == type_
            || BEGIN_WITH_ATTRIBUTE == type_ || END_WITH_ATTRIBUTE == type_
            || SUBSTRING_ATTRIBUTE == type_ || CLASS == type_ || ID == type_);
  }

  // PSEUDOCLASS accessor:
  const UnicodeText& pseudoclass() const {
    DCHECK_EQ(PSEUDOCLASS, type_);
    return value_;
  }

  // lang accessor
  const UnicodeText& lang() const {
    DCHECK_EQ(LANG, type_);
    return value_;
  }

  string ToString() const;
 private:
  Type type_;

  HtmlTagEnum element_type_;  // ELEMENT_TYPE
  UnicodeText element_text_;  // ELEMENT_TYPE
  static const HtmlTagIndex tagindex_;  // Look up HTML tags.  Thread-safe.

  UnicodeText attribute_;  // Attribute name, valid for *_ATTRIBUTE, CLASS, ID
  UnicodeText value_;    // Valid for *_ATTRIBUTE, CLASS, ID, PSEUDOCLASS, LANG

  // Private constructors, for use by factory methods
  SimpleSelector(Type type,
                    const UnicodeText& attribute, const UnicodeText& value)
      : type_(type), attribute_(attribute), value_(value) { }
  SimpleSelector(HtmlTagEnum element_type, const UnicodeText& element_text)
      : type_(ELEMENT_TYPE),
        element_type_(element_type), element_text_(element_text) { }

  DISALLOW_IMPLICIT_CONSTRUCTORS(SimpleSelector);
};

// ------------
// SimpleSelectors:
//
// SimpleSelectors is a vector of SimpleSelector*, which we own and
// will delete upon destruction.  If you remove elements from
// SimpleSelectors, you are responsible for deleting them.
//
// Semantically, SimpleSelectors is the AND of each of its constituent
// simple selectors.  Although SAC permits other logical connectives like OR
// and NOT, it appears that CSS3 will not, so we favor the simplicity
// of this list representation over the complexity of a full logical tree
// (and its pie-in-the-sky possibilities).  CSS3's :not pseudoclass
// takes an entire simple selector as negation, not just a condition.
//
// SimpleSelectors also has the combinator() field, which describes
// how it relates to the previous SimpleSelectors in the
// Selector chain.  For example, consider E > F + G.  E's
// combinator() is NONE, F's combinator is CHILD, and G's combinator
// is SIBLING.
// ------------
class SimpleSelectors : public std::vector<SimpleSelector*> {
 public:
  enum Combinator {
    NONE,         // first one in the chain
    DESCENDANT,   // this one is a descendant of the previous one
    CHILD,        // this one is a child (direct descendant) of the previous one
    SIBLING       // this one is an adjacent sibling of the previous
                  // one, non-element nodes (such as text nodes and
                  // comments) excluded.
  };

  SimpleSelectors(Combinator c)
      : std::vector<SimpleSelector*>(), combinator_(c) { }
  ~SimpleSelectors();

  Combinator combinator() const { return combinator_; }
  const SimpleSelector* get(int i) const { return (*this)[i]; }  // sugar.

  string ToString() const;
 private:
  const Combinator combinator_;
  DISALLOW_COPY_AND_ASSIGN(SimpleSelectors);
};

// ------------
// Selector:
//
// A selector is a chain of sequences of simple selectors separated by
// combinators.  Each SimpleSelectors stores the combinator between
// it and the previous one in the chain.
// ------------
class Selector: public std::vector<SimpleSelectors*> {
 public:
  Selector() { }
  ~Selector();
  // We provide syntactic sugar for accessing elements.
  // conditions->get(i) looks better than (*conditions)[i])
  const SimpleSelectors* get(int i) const { return (*this)[i]; }

  string ToString() const;
 private:
  DISALLOW_COPY_AND_ASSIGN(Selector);
};

// ------------
// Selectors:
//
// When several selectors share the same declarations, they may be
// grouped into a comma-separated list:
// ------------
class Selectors: public std::vector<Selector*> {
 public:
  Selectors() { }
  ~Selectors();
  const Selector* get(int i) const { return (*this)[i]; }

  string ToString() const;
 private:
  DISALLOW_COPY_AND_ASSIGN(Selectors);
};

}  // namespace

#endif  // WEBUTIL_CSS_SELECTOR_H__
