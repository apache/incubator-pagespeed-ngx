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

#ifndef NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_ELEMENT_H_
#define NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_ELEMENT_H_

#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parser_types.h"
#include "net/instaweb/util/public/atom.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/symbol_table.h"

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
    // TODO(sligocki): check sanity of values (ex: no stray quotes).
    //
    // TODO(jmarantz): arg 'quote' must be a static string, or NULL,
    // if quoting is not yet known (e.g. this is a synthesized attribute.
    // This is hard-to-describe and we should probably use an Atom for
    // the quote, and decide how to handle NULL.
    Attribute(Atom name, const StringPiece& value, const char* quote)
        : name_(name), quote_(quote) {
      set_value(value);
    }

    Atom name() const { return name_; }
    void set_name(Atom name) { name_ = name; }

    // The result of value() is still owned by this, and will be invalidated by
    // a subsequent call to set_value().
    //
    // The result will be a NUL-terminated string containing the value of the
    // attribute, or NULL if the attribute has no value at all (this is
    // distinct from having the empty string for a value).
    const char* value() const { return value_.get(); }

    // See comment about quote on constructor for Attribute.
    const char* quote() const { return quote_; }

    // Modify value of attribute (eg to rewrite dest of src or href).
    // As with the constructor, copies the string in, so caller retains
    // ownership of value.
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
    void set_value(const StringPiece& value) {
      if (value.data() == NULL) {
        value_.reset(NULL);
      } else {
        char* buf = new char[value.size() + 1];
        memcpy(buf, value.data(), value.size());
        buf[value.size()] = '\0';
        value_.reset(buf);
      }
    }

    // See comment about quote on constructor for Attribute.
    void set_quote(const char *quote) {
      quote_ = quote;
    }

   private:
    Atom name_;
    scoped_array<char> value_;
    const char* quote_;
  };

  virtual ~HtmlElement();

  // Unconditionally add attribute, copying value.
  // Quote is assumed to be a static const char *.
  // Doesn't check for attribute duplication (which is illegal in html).
  void AddAttribute(Atom name, const StringPiece& value, const char* quote) {
    attributes_.push_back(new Attribute(name, value, quote));
  }

  // Removes the attribute at the given index, shifting higher-indexed
  // attributes down.  Note that this operation is linear in the number of
  // attributes.
  void DeleteAttribute(int i);

  // Look up attribute by name.  NULL if no attribute exists.
  // Use this for attributes whose value you might want to change
  // after lookup.
  const Attribute* FindAttribute(Atom name) const;
  Attribute* FindAttribute(Atom name) {
    const HtmlElement* const_this = this;
    const Attribute* result = const_this->FindAttribute(name);
    return const_cast<Attribute*>(result);
  }

  // Look up attribute value by name.  NULL if no attribute exists.
  // Use this only if you don't intend to change the attribute value;
  // if you might change the attribute value, use FindAttribute instead
  // (this avoids a double lookup).
  const char* AttributeValue(Atom name) const {
    const Attribute* attribute = FindAttribute(name);
    if (attribute != NULL) {
      return attribute->value();
    }
    return NULL;
  }

  // Look up attribute value by name.  false if no attribute exists,
  // or attribute value cannot be converted to int.  Otherwise
  // sets *value.
  bool IntAttributeValue(Atom name, int* value) const {
    const Attribute* attribute = FindAttribute(name);
    if (attribute != NULL) {
      return StringToInt(attribute->value(), value);
    }
    return false;
  }

  // Small integer uniquely identifying the HTML element, primarily
  // for debugging.
  void set_sequence(int sequence) { sequence_ = sequence; }


  Atom tag() const {return tag_;}

  // Changing that tag of an element should only occur if the caller knows
  // that the old attributes make sense for the new tag.  E.g. a div could
  // be changed to a span.
  void set_tag(Atom new_tag) { tag_ = new_tag; }

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
  Atom tag_;
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
