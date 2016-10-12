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

#ifndef PAGESPEED_KERNEL_HTML_HTML_ELEMENT_H_
#define PAGESPEED_KERNEL_HTML_HTML_ELEMENT_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/inline_slist.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"

namespace net_instaweb {

// Represents an HTML tag, including all its attributes.  These are never
// constructed independently, but are managed by class HtmlParse.  They
// are constructed when parsing an HTML document, and they can also be
// synthesized via methods in HtmlParse::NewElement.
//
// Note that HtmlElement* saved during filter execution are valid only until
// a Flush occurs.  HtmlElement* can still be fully accessed during a Flush, but
// after that, to save memory, the contents of the HtmlElement* are cleared.
// After that, the only method it's legal to do is to call is
// HtmlParse::IsRewriteable(), which will return false.
class HtmlElement : public HtmlNode {
 public:
  // Tags can be closed in three ways: implicitly (e.g. <img ..>),
  // briefly (e.g. <br/>), or explicitly (<a...>...</a>).  The
  // Lexer will always record the way it parsed a tag, but synthesized
  // elements will have AUTO_CLOSE, and rewritten elements may
  // no longer qualify for the closing style with which they were
  // parsed.
  enum Style {
    AUTO_CLOSE,      // synthesized tag, or not yet closed in source
    IMPLICIT_CLOSE,  // E.g. <img...> <meta...> <link...> <br...> <input...>
    EXPLICIT_CLOSE,  // E.g. <a href=...>anchor</a>
    BRIEF_CLOSE,     // E.g. <head/>
    UNCLOSED,        // Was never closed in source, so don't serialize close-tag
    INVISIBLE,       // Programatically hidden element
  };

  // Various ways things can be quoted (or not)
  enum QuoteStyle {
    NO_QUOTE,
    SINGLE_QUOTE,
    DOUBLE_QUOTE
  };

  class Attribute : public InlineSListElement<Attribute> {
   public:
    // A large quantity of HTML in the wild has attributes that are
    // improperly escaped.  Browsers are generally tolerant of this.
    // But we want to avoid corrupting pages we do not understand.

    // The result of DecodedValueOrNull() and escaped_value() is still
    // owned by this, and will be invalidated by a subsequent call to
    // SetValue() or SetUnescapedValue

    // Returns the attribute name, which is not guaranteed to be case-folded.
    // Compare keyword() to the Keyword constant found in html_name.h for
    // fast attribute comparisons.
    StringPiece name_str() const { return name_.value(); }

    // Returns the HTML keyword enum.  If this attribute name is not
    // recognized, returns HtmlName::kNotAKeyword, and you can examine
    // name_str().
    HtmlName::Keyword keyword() const { return name_.keyword(); }

    HtmlName name() const { return name_; }
    void set_name(const HtmlName& name) { name_ = name; }

    // Returns the value in its original directly from the HTML source.
    // This may have HTML escapes in it, such as "&amp;".
    const char* escaped_value() const { return escaped_value_.get(); }

    // The result of DecodedValueOrNull() is still owned by this, and
    // will be invalidated by a subsequent call to SetValue().
    //
    // The result will be a NUL-terminated string containing the value of the
    // attribute, or NULL if the attribute has no value at all (this is
    // distinct from having the empty string for a value), or there is
    // a decoding error.  E.g.
    //    <tag a="val">              --> "val"
    //    <tag a="&amp;">            --> "&"
    //    <tag a="">                 --> ""
    //    <tag a>                    --> NULL
    //    <tag a="muñecos">          --> NULL  (decoding_error()==true)
    //
    // Returns the unescaped value, suitable for directly operating on
    // in filters as URLs or other data.  Note that decoding_error() is
    // true if the parsed value from HTML could not be decoded.  This
    // might occur if:
    //    - the charset is not known
    //    - the charset is not supported.  Currently none are supported and
    //      only values that fall in 7-bit ascii can be interpreted.
    //    - the charset is known & supported but the value does not appear to be
    //      legal.
    //
    // The decoded value uses 8-bit characters to represent any unicode
    // code-point less than 256.
    const char* DecodedValueOrNull() const {
      if (!decoded_value_computed_) {
        ComputeDecodedValue();
      }
      return decoded_value_.get();
    }

    void set_decoding_error(bool x) { decoding_error_ = x; }
    bool decoding_error() const {
      if (!decoded_value_computed_) {
        ComputeDecodedValue();
      }
      return decoding_error_;
    }

    // See comment about quote on constructor for Attribute.
    // Returns the quotation mark associated with this URL.
    QuoteStyle quote_style() const { return quote_style_; }

    // Textual form of quote for printing.
    const char* quote_str() const;

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

    void set_quote_style(QuoteStyle new_quote_style) {
      quote_style_ = new_quote_style;
    }

    friend class HtmlElement;

   private:
    void ComputeDecodedValue() const;

    // This should only be called from AddAttribute
    Attribute(const HtmlName& name, const StringPiece& escaped_value,
              QuoteStyle quote_style);

    static inline void CopyValue(const StringPiece& src,
                                 scoped_array<char>* dst);

    HtmlName name_;
    QuoteStyle quote_style_ : 8;
    mutable bool decoding_error_;
    mutable bool decoded_value_computed_;

    // Attribute value represented as ascii and
    // HTML-escape-sequences, typically parsed directly from an HTML
    // file.  This is the canonical representation, and it can handle
    // any arbitrary multi-byte characters.
    //
    // Note that it is acceptable to have 8-bit characters in escape
    // sequences (typically iso8859).  However we will not be able to
    // decode such attributes.
    scoped_array<char> escaped_value_;

    // An 8-bit representation of the escaped_value.  Escape sequences
    // that contain character-codes >= 256 are not decoded, and will
    // result in decoding_error_==true.  Also note that a literal 8-bit
    // code in escaped_value_ cannot be decoded either.
    //
    // We can get fewer decoding errors if we are careful to track the
    // character-encoding for the document, and implement some of the
    // popular ones, e.g. utf8, gb2312 and iso8859.  Note that failing
    // to decode an attribute value does not impact our ability to
    // parse and reserialize the document.  It just prevents us from
    // looking at the decoded value, which is a requirement primarily
    // for tags referencing URLs, e.g. <img src=...>.
    //
    // Note that we do not decode non-ASCII characters but we can
    // represent them in escaped_value_.  We can get 8-bit characters
    // into decoded_value_ via &#129; etc.
    mutable scoped_array<char> decoded_value_;

    DISALLOW_COPY_AND_ASSIGN(Attribute);
  };

  typedef InlineSList<Attribute> AttributeList;
  typedef InlineSList<Attribute>::Iterator AttributeIterator;
  typedef InlineSList<Attribute>::ConstIterator AttributeConstIterator;

  virtual ~HtmlElement();

  // Determines whether this node is still accessible via API.  Note that
  // when a FLUSH occurs after an open-element, the element will be live()
  // but will not be rewritable.  Specifically, node->live() can be true when
  // html_parse->IsRewritable(node) is false.  Once a node is closed, a FLUSH
  // will cause the node's data to be freed, which triggers this method
  // returning false.
  virtual bool live() const { return (data_.get() != NULL) && data_->live_; }

  virtual void MarkAsDead(const HtmlEventListIterator& end);

  // Add a copy of an attribute to this element.  The attribute may come
  // from this element, or another one.
  void AddAttribute(const Attribute& attr);

  // Unconditionally add attribute, copying value.
  // For binary attributes (those without values) use value=NULL.
  // TODO(sligocki): StringPiece(NULL) seems fragile because what it is or
  // how it's treated is not documented.
  //
  // Doesn't check for attribute duplication (which is illegal in html).
  //
  // The value, if non-null, is assumed to be unescaped.  See also
  // AddEscapedAttribute.
  void AddAttribute(const HtmlName& name,
                    const StringPiece& decoded_value,
                    QuoteStyle quote_style);
  // As AddAttribute, but assumes value has been escaped for html output.
  void AddEscapedAttribute(const HtmlName& name,
                           const StringPiece& escaped_value,
                           QuoteStyle quote_style);

  // Remove the attribute with the given name.  Return true if the attribute
  // was deleted, false if it wasn't there to begin with.
  bool DeleteAttribute(HtmlName::Keyword keyword);
  bool DeleteAttribute(StringPiece name);

  // Look up attribute by name.  NULL if no attribute exists.
  // Use this for attributes whose value you might want to change
  // after lookup.
  const Attribute* FindAttribute(HtmlName::Keyword keyword) const;
  Attribute* FindAttribute(HtmlName::Keyword keyword) {
    const HtmlElement* const_this = this;
    const Attribute* result = const_this->FindAttribute(keyword);
    return const_cast<Attribute*>(result);
  }

  const Attribute* FindAttribute(StringPiece name) const;
  Attribute* FindAttribute(StringPiece name) {
    const HtmlElement* const_this = this;
    const Attribute* result = const_this->FindAttribute(name);
    return const_cast<Attribute*>(result);
  }

  bool HasAttribute(HtmlName::Keyword keyword) const {
    const Attribute* attribute = FindAttribute(keyword);
    return attribute != nullptr;
  }

  // Look up decoded attribute value by name.
  // Returns NULL if:
  //    1. no attribute exists
  //    2. the attribute has no value.
  //    3. the attribute has a value, but it cannot currently be safely decoded.
  // If you care about this distinction, call FindAttribute.
  // Use this only if you don't intend to change the attribute value;
  // if you might change the attribute value, use FindAttribute instead
  // (this avoids a double lookup).
  const char* AttributeValue(HtmlName::Keyword name) const {
    const Attribute* attribute = FindAttribute(name);
    if (attribute != NULL) {
      return attribute->DecodedValueOrNull();
    }
    return NULL;
  }

  // Look up escaped attribute value by name.
  // Returns NULL if:
  //    1. no attribute exists
  //    2. the attribute has no value.
  // If you care about this distinction, call FindAttribute.
  // Use this only if you don't intend to change the attribute value;
  // if you might change the attribute value, use FindAttribute instead
  // (this avoids a double lookup).
  const char* EscapedAttributeValue(HtmlName::Keyword name) const {
    const Attribute* attribute = FindAttribute(name);
    if (attribute != NULL) {
      return attribute->escaped_value();
    }
    return NULL;
  }

  // Returns the element tag name, which is not guaranteed to be
  // case-folded.  Compare keyword() to the Keyword constant found in
  // html_name.h for fast tag name comparisons.
  StringPiece name_str() const { return data_->name_.value(); }

  // Returns the HTML keyword enum.  If this tag name is not
  // recognized, returns HtmlName::kNotAKeyword, and you can
  // examine name_str().
  HtmlName::Keyword keyword() const { return data_->name_.keyword(); }

  const HtmlName& name() const { return data_->name_; }

  // Changing that tag of an element should only occur if the caller knows
  // that the old attributes make sense for the new tag.  E.g. a div could
  // be changed to a span.
  void set_name(const HtmlName& new_tag) { data_->name_ = new_tag; }

  const AttributeList& attributes() const { return data_->attributes_; }
  AttributeList* mutable_attributes() { return &data_->attributes_; }

  friend class HtmlParse;
  friend class HtmlLexer;

  Style style() const { return data_->style_; }
  void set_style(Style style) { data_->style_ = style; }

  // Render an element as a string for debugging.  This is not
  // intended as a fully legal serialization.
  virtual GoogleString ToString() const;
  void DebugPrint() const;

  int begin_line_number() const { return data_->begin_line_number_; }
  int end_line_number() const { return data_->end_line_number_; }

 protected:
  virtual void SynthesizeEvents(const HtmlEventListIterator& iter,
                                HtmlEventList* queue);

  virtual HtmlEventListIterator begin() const { return data_->begin_; }
  virtual HtmlEventListIterator end() const { return data_->end_; }

 private:
  // All of the data associated with an HtmlElement is indirected through this
  // class, so we can delete it on Flush after a CloseElement event.
  struct Data {
    Data(const HtmlName& name,
         const HtmlEventListIterator& begin,
         const HtmlEventListIterator& end);
    ~Data();

    // Max value for the line numbers below.  Since they are 24-bits,
    // comparing against -1 does not work properly.
    static const unsigned kMaxLineNumber = 0x00ffffff;

    // Pack four fields into 64 bits using bitfields.  Warning: this
    // stuff is quite sensitive to details, so make sure to look at
    // object sizes before changing!  Interleaving the 24-bit and
    // 8-bit member variables gives a total size of 8 bytes for these
    // 4 variables on a gcc 64-bit compile.  But putting the two
    // 24-bit integers together gives a total size of 16 bytes, so
    // we interleave.
    //
    // HtmlParse::DeleteNode will set live_ to false without
    // deleting element->data_.  Flushing an ElementClose deletes
    // data_ but HtmlElement knows that null data_ implies !live().
    unsigned begin_line_number_ : 24;
    unsigned live_ : 8;
    unsigned end_line_number_ : 24;
    Style style_ : 8;

    HtmlName name_;
    AttributeList attributes_;
    HtmlEventListIterator begin_;
    HtmlEventListIterator end_;
  };

  // Begin/end event iterators are used by HtmlParse to keep track
  // of the span of events underneath an element.  This is primarily to
  // help delete the element.  Events are not public.
  void set_begin(const HtmlEventListIterator& begin) { data_->begin_ = begin; }
  void set_end(const HtmlEventListIterator& end) { data_->end_ = end; }

  void set_begin_line_number(int line) { data_->begin_line_number_ = line; }
  void set_end_line_number(int line) { data_->end_line_number_ = line; }

  // construct via HtmlParse::NewElement
  HtmlElement(HtmlElement* parent, const HtmlName& name,
              const HtmlEventListIterator& begin,
              const HtmlEventListIterator& end);

  // HtmlElement data is held in HtmlElement::Data*, which is freed
  // when a CloseElement is Flushed.  The pointers themselves are
  // retained and can correctly answer element->IsRewritable() and
  // element->is_live(), but the rest of the data (attributes etc)
  // is deleted.
  void FreeData() { data_.reset(NULL); }

  scoped_ptr<Data> data_;

  DISALLOW_COPY_AND_ASSIGN(HtmlElement);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTML_HTML_ELEMENT_H_
