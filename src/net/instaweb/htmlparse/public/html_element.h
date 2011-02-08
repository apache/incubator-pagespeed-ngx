/*
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

#ifndef NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_ELEMENT_H_
#define NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_ELEMENT_H_

#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parser_types.h"
#include "net/instaweb/util/public/atom.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/symbol_table.h"

#define LOWER_CASE_DURING_LEXER 1

namespace net_instaweb {

class HtmlElement : public HtmlNode {
 public:
  // Tags can be closed in three ways: implicitly (e.g. <img ..>),
  // briefly (e.g. <br/>), or explicitly (<a...>...</a>).  The
  // Lexer will always record the way it parsed a tag, but synthesized
  // elements will have AUTO_CLOSE, and rewritten elements may
  // no longer qualify for the closing style with which they were
  // parsed.
  enum CloseStyle {
    AUTO_CLOSE,      // synthesized tag, or not yet closed in source
    IMPLICIT_CLOSE,  // E.g. <img...> <meta...> <link...> <br...> <input...>
    EXPLICIT_CLOSE,  // E.g. <a href=...>anchor</a>
    BRIEF_CLOSE,     // E.g. <head/>
    UNCLOSED         // Was never closed in source
  };

  class Attribute {
   public:
    // A large quantity of HTML in the wild has attributes that are
    // improperly escaped.  Browsers are generally tolerant of this.
    // But we want to avoid corrupting pages we do not understand.

    // The result of value() and escaped_value() is still owned by this, and
    // will be invalidated by a subsequent call to SetValue() or
    // SetUnescapedValue

    // Returns the attribute name, which is not guaranteed to be case-folded.
    // Compare keyword() to the Keyword constant found in html_name.h for
    // fast attribute comparisons.
    const char* name_str() const { return name_.atom().c_str(); }

    // Returns the HTML keyword enum.  If this attribute name is not
    // recognized, returns HtmlName::kNotAKeyword, and you can examine
    // name_str().
    HtmlName::Keyword keyword() const { return name_.keyword(); }

#if LOWER_CASE_DURING_LEXER
    // Currently for incremental CL checkins we must have this....
    Atom name() const { return name_.atom(); }
    HtmlName html_name() const { return name_; }
    void set_name(Atom atom) { name_ = HtmlName(atom); }
#else
    // ...but we would like to migrate to this
    HtmlName name() const { return name_; }
#endif
    void set_name(const HtmlName& name) { name_ = name; }

    // Returns the value in its original directly from the HTML source.
    // This may have HTML escapes in it, such as "&amp;".
    const char* escaped_value() const { return escaped_value_.get(); }

    // The result of value() is still owned by this, and will be invalidated by
    // a subsequent call to set_value().
    //
    // The result will be a NUL-terminated string containing the value of the
    // attribute, or NULL if the attribute has no value at all (this is
    // distinct from having the empty string for a value).

    // Returns the unescaped value, suitable for directly operating on
    // in filters as URLs or other data.
    const char* value() const { return value_.get(); }

    // See comment about quote on constructor for Attribute.
    // Returns the quotation mark associated with this URL, typically
    // ", ', or an empty string.
    const char* quote() const { return quote_; }

    // Two related methods to modify the value of attribute (eg to rewrite
    // dest of src or href). As  with the constructor, copies the string in,
    // so caller retains ownership of value.
    //
    // A StringPiece pointing to an empty string (that is, a char array {'\0'})
    // indicates that the attribute value is the empty string (e.g. <foo
    // bar="">); however, a StringPiece with a data() pointer of NULL indicates
    // that the attribute has no value at all (e.g. <foo bar>).  This is an
    // important distinction.
    //
    // Note that passing a value containing NULs in the middle will cause
    // breakage, but this isn't currently checked for.
    // TODO(mdsteele): Perhaps we should check for this?

    // Sets the value of the attribute.  No HTML escaping is expected.
    // This call causes the HTML-escaped value to be automatically computed
    // by scanning the value and escaping any characters required in HTML
    // attributes.
    void SetValue(const StringPiece& value);

    // Sets the escaped value.  This is intended to be called from the HTML
    // Lexer, and results in the Value being computed automatically by
    // scanning the value for escape sequences.
    void SetEscapedValue(const StringPiece& value);

    // See comment about quote on constructor for Attribute.
    void set_quote(const char *quote) {
      quote_ = quote;
    }

    friend class HtmlElement;

   private:
    // TODO(jmarantz): arg 'quote' must be a static string, or NULL,
    // if quoting is not yet known (e.g. this is a synthesized attribute.
    // This is hard-to-describe and we should probably use an Atom for
    // the quote, and decide how to handle NULL.
    //
    // This should only be called from AddAttribute
    //
    // TODO(jmarantz): remove any of these that are not needed once the
    // atoms are hidden.
    Attribute(Atom name, const StringPiece& value,
              const StringPiece& escaped_value, const char* quote);
    Attribute(const HtmlName& name, const StringPiece& value,
              const StringPiece& escaped_value, const char* quote);

    static inline void CopyValue(const StringPiece& src,
                                 scoped_array<char>* dst);

    HtmlName name_;
    scoped_array<char> escaped_value_;
    scoped_array<char> value_;
    const char* quote_;

    DISALLOW_COPY_AND_ASSIGN(Attribute);
  };

  virtual ~HtmlElement();

  // Add a copy of an attribute to this element.  The attribute may come
  // from this element, or another one.
  void AddAttribute(const Attribute& attr);

  // Unconditionally add attribute, copying value.
  // Quote is assumed to be a static const char *.
  // Doesn't check for attribute duplication (which is illegal in html).
  //
  // The value, if non-null, is assumed to be unescaped.  See also
  // AddEscapedAttribute.
  void AddAttribute(Atom name, const StringPiece& value, const char* quote);
  // As AddAttribute, but assumes value has been escaped for html output.
  void AddEscapedAttribute(Atom name, const StringPiece& escaped_value,
                           const char* quote);

  // Removes the attribute at the given index, shifting higher-indexed
  // attributes down.  Note that this operation is linear in the number of
  // attributes.
  void DeleteAttribute(int i);

  // Remove the attribute with the given name.  Return true if the attribute
  // was deleted, false if it wasn't there to begin with.
  bool DeleteAttribute(HtmlName::Keyword keyword);

  // Look up attribute by name.  NULL if no attribute exists.
  // Use this for attributes whose value you might want to change
  // after lookup.
  const Attribute* FindAttribute(HtmlName::Keyword keyword) const;
  Attribute* FindAttribute(HtmlName::Keyword keyword) {
    const HtmlElement* const_this = this;
    const Attribute* result = const_this->FindAttribute(keyword);
    return const_cast<Attribute*>(result);
  }

  // Look up attribute value by name.  NULL if no attribute exists.
  // Use this only if you don't intend to change the attribute value;
  // if you might change the attribute value, use FindAttribute instead
  // (this avoids a double lookup).
  const char* AttributeValue(HtmlName::Keyword name) const {
    const Attribute* attribute = FindAttribute(name);
    if (attribute != NULL) {
      return attribute->value();
    }
    return NULL;
  }

  // Look up attribute value by name.  false if no attribute exists,
  // or attribute value cannot be converted to int.  Otherwise
  // sets *value.
  bool IntAttributeValue(HtmlName::Keyword name, int* value) const {
    const Attribute* attribute = FindAttribute(name);
    if (attribute != NULL) {
      return StringToInt(attribute->value(), value);
    }
    return false;
  }

  // Small integer uniquely identifying the HTML element, primarily
  // for debugging.
  void set_sequence(int sequence) { sequence_ = sequence; }

  // Compatibility methods that will be eliminated once CLs 19351656,
  // 19351667, and 19355391 are submitted.
#if LOWER_CASE_DURING_LEXER
  Atom tag() const { return name_.atom(); }
  void set_tag(Atom atom) { name_ = HtmlName(atom); }
  HtmlName::Keyword Keyword(Atom atom) const {
    HtmlName name(atom);
    return name.keyword();
  }

  const char* AttributeValue(Atom atom) const {
    return AttributeValue(Keyword(atom));
  }

  bool IntAttributeValue(Atom atom, int* value) const {
    return IntAttributeValue(Keyword(atom), value);
  }

  Attribute* FindAttribute(Atom atom) {
    return FindAttribute(Keyword(atom));
  }

  const Attribute* FindAttribute(Atom atom) const {
    return FindAttribute(Keyword(atom));
  }

  bool DeleteAttribute(Atom atom) {
    return DeleteAttribute(Keyword(atom));
  }

  void AddAttribute(Atom name, int value);
#endif

  // Returns the element tag name, which is not guaranteed to be
  // case-folded.  Compare keyword() to the Keyword constant found in
  // html_name.h for fast tag name comparisons.
  const char* name_str() const { return name_.atom().c_str(); }

  // Returns the HTML keyword enum.  If this tag name is not
  // recognized, returns HtmlName::kNotAKeyword, and you can
  // examine name_str().
  HtmlName::Keyword keyword() const { return name_.keyword(); }

  const HtmlName& name() const { return name_; }

  // Changing that tag of an element should only occur if the caller knows
  // that the old attributes make sense for the new tag.  E.g. a div could
  // be changed to a span.
  void set_name(const HtmlName& new_tag) { name_ = new_tag; }

  int attribute_size() const {return attributes_.size(); }
  const Attribute& attribute(int i) const { return *attributes_[i]; }
  Attribute& attribute(int i) { return *attributes_[i]; }

  friend class HtmlParse;
  friend class HtmlLexer;

  CloseStyle close_style() const { return close_style_; }
  void set_close_style(CloseStyle style) { close_style_ = style; }

  // Render an element as a string for debugging.  This is not
  // intended as a fully legal serialization.
  void ToString(std::string* buf) const;
  void DebugPrint() const;

  int begin_line_number() const { return begin_line_number_; }
  int end_line_number() const { return end_line_number_; }

 protected:
  virtual void SynthesizeEvents(const HtmlEventListIterator& iter,
                                HtmlEventList* queue);
  virtual void InvalidateIterators(const HtmlEventListIterator& end);

  virtual HtmlEventListIterator begin() const { return begin_; }
  virtual HtmlEventListIterator end() const { return end_; }

 private:
  // Begin/end event iterators are used by HtmlParse to keep track
  // of the span of events underneath an element.  This is primarily to
  // help delete the element.  Events are not public.
  void set_begin(const HtmlEventListIterator& begin) { begin_ = begin; }
  void set_end(const HtmlEventListIterator& end) { end_ = end; }

  void set_begin_line_number(int line) { begin_line_number_ = line; }
  void set_end_line_number(int line) { end_line_number_ = line; }

  // construct via HtmlParse::NewElement
  HtmlElement(HtmlElement* parent, Atom tag, const HtmlEventListIterator& begin,
      const HtmlEventListIterator& end);

  int sequence_;
  HtmlName name_;
  std::vector<Attribute*> attributes_;
  HtmlEventListIterator begin_;
  HtmlEventListIterator end_;
  CloseStyle close_style_;
  int begin_line_number_;
  int end_line_number_;

  DISALLOW_COPY_AND_ASSIGN(HtmlElement);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_ELEMENT_H_
