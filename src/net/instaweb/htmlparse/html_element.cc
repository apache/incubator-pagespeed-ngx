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

#include "net/instaweb/htmlparse/public/html_element.h"

#include <cstdio>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/html_event.h"
#include "net/instaweb/htmlparse/public/html_keywords.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_parser_types.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

HtmlElement::HtmlElement(HtmlElement* parent, const HtmlName& name,
    const HtmlEventListIterator& begin, const HtmlEventListIterator& end)
    : HtmlNode(parent),
      data_(new Data(name, begin, end)) {
}

HtmlElement::~HtmlElement() {
}

HtmlElement::Data::Data(const HtmlName& name,
                        const HtmlEventListIterator& begin,
                        const HtmlEventListIterator& end)
    : begin_line_number_(0),
      live_(1),
      end_line_number_(0),
      close_style_(AUTO_CLOSE),
      name_(name),
      begin_(begin),
      end_(end) {
}

HtmlElement::Data::~Data() {
}

void HtmlElement::MarkAsDead(const HtmlEventListIterator& end) {
  if (data_.get() != NULL) {
    data_->live_ = false;
    set_begin(end);
    set_end(end);
  }
}

void HtmlElement::SynthesizeEvents(const HtmlEventListIterator& iter,
                                   HtmlEventList* queue) {
  // We use -1 as a bogus line number, since these events are synthetic.
  HtmlEvent* start_tag = new HtmlStartElementEvent(this, -1);
  set_begin(queue->insert(iter, start_tag));
  HtmlEvent* end_tag = new HtmlEndElementEvent(this, -1);
  set_end(queue->insert(iter, end_tag));
}

bool HtmlElement::DeleteAttribute(HtmlName::Keyword keyword) {
  AttributeList* attrs = mutable_attributes();
  for (AttributeIterator iter(attrs->begin()); iter != attrs->end(); ++iter) {
    if (iter->keyword() == keyword) {
      attrs->Erase(&iter);
      return true;
    }
  }
  return false;
}

const HtmlElement::Attribute* HtmlElement::FindAttribute(
    HtmlName::Keyword keyword) const {
  const Attribute* ret = NULL;

  for (AttributeConstIterator iter = attributes().begin();
       iter != attributes().end(); ++iter) {
    const Attribute* attribute = iter.Get();
    if (attribute->keyword() == keyword) {
      ret = attribute;
      break;
    }
  }
  return ret;
}

void HtmlElement::ToString(GoogleString* buf) const {
  *buf += "<";
  *buf += data_->name_.c_str();

  for (AttributeConstIterator iter = attributes().begin();
       iter != attributes().end(); ++iter) {
    const Attribute& attribute = *iter;
    *buf += ' ';
    *buf += attribute.name_str();
    const char* value = attribute.DecodedValueOrNull();
    if (attribute.decoding_error()) {
      // This is a debug method; not used in serialization.
      *buf += "<DECODING ERROR>";
    } else if (value != NULL) {
      *buf += "=";
      const char* quote = attribute.quote_str();
      *buf += quote;
      *buf += value;
      *buf += quote;
    }
  }
  switch (data_->close_style_) {
    case AUTO_CLOSE:       *buf += "> (not yet closed)"; break;
    case IMPLICIT_CLOSE:   *buf += ">";  break;
    case EXPLICIT_CLOSE:   *buf += "></";
                           *buf += data_->name_.c_str();
                           *buf += ">";
                           break;
    case BRIEF_CLOSE:      *buf += "/>"; break;
    case UNCLOSED:         *buf += "> (unclosed)"; break;
  }
  if ((data_->begin_line_number_ != -1) || (data_->end_line_number_ != -1)) {
    *buf += " ";
    if (data_->begin_line_number_ != -1) {
      *buf += IntegerToString(data_->begin_line_number_);
    }
    *buf += "...";
    if (data_->end_line_number_ != -1) {
      *buf += IntegerToString(data_->end_line_number_);
    }
  }
}

void HtmlElement::DebugPrint() const {
  GoogleString buf;
  ToString(&buf);
  fprintf(stdout, "%s\n", buf.c_str());
}

void HtmlElement::AddAttribute(const Attribute& src_attr) {
  Attribute* attr = new Attribute(src_attr.name(),
                                  src_attr.escaped_value(),
                                  src_attr.quote_style());
  if (src_attr.decoded_value_computed_) {
    attr->decoded_value_computed_ = true;
    attr->decoding_error_ = src_attr.decoding_error_;
    Attribute::CopyValue(src_attr.decoded_value_.get(), &attr->decoded_value_);
  }
  data_->attributes_.Append(attr);
}

void HtmlElement::AddAttribute(const HtmlName& name,
                               const StringPiece& decoded_value,
                               QuoteStyle quote_style) {
  GoogleString buf;
  Attribute* attr = new Attribute(name,
                                  HtmlKeywords::Escape(decoded_value, &buf),
                                  quote_style);
  attr->decoded_value_computed_ = true;
  attr->decoding_error_ = false;
  Attribute::CopyValue(decoded_value, &attr->decoded_value_);
  data_->attributes_.Append(attr);
}

void HtmlElement::AddEscapedAttribute(const HtmlName& name,
                                      const StringPiece& escaped_value,
                                      QuoteStyle quote_style) {
  Attribute* attr = new Attribute(name, escaped_value, quote_style);
  data_->attributes_.Append(attr);
}

void HtmlElement::Attribute::CopyValue(const StringPiece& src,
                                       scoped_array<char>* dst) {
  if (src.data() == NULL) {
    // This case indicates attribute without value <tag attr>, as opposed
    // to data()=="", which implies an empty value <tag attr=>.
    dst->reset(NULL);
  } else {
    char* buf = new char[src.size() + 1];
    memcpy(buf, src.data(), src.size());
    buf[src.size()] = '\0';
    dst->reset(buf);
  }
}

HtmlElement::Attribute::Attribute(const HtmlName& name,
                                  const StringPiece& escaped_value,
                                  QuoteStyle quote_style)
    : name_(name),
      quote_style_(quote_style),
      decoding_error_(false),
      decoded_value_computed_(false) {
  CopyValue(escaped_value, &escaped_value_);
}

// Modify value of attribute (eg to rewrite dest of src or href).
// As with the constructor, copies the string in, so caller retains
// ownership of value.
void HtmlElement::Attribute::SetValue(const StringPiece& decoded_value) {
  GoogleString buf;
  // Note that we execute the lines in this order in case value
  // is a substring of value_.  This copies the value just prior
  // to deallocation of the old value_.
  const char* escaped_chars = escaped_value_.get();
  DCHECK(decoded_value.data() + decoded_value.size() < escaped_chars ||
         escaped_chars + strlen(escaped_chars) < decoded_value.data())
      << "Setting unescaped value from substring of escaped value.";
  CopyValue(HtmlKeywords::Escape(decoded_value, &buf), &escaped_value_);
  CopyValue(decoded_value, &decoded_value_);
}

void HtmlElement::Attribute::SetEscapedValue(const StringPiece& escaped_value) {
  GoogleString buf;
  // Note that we execute the lines in this order in case value
  // is a substring of value_.  This copies the value just prior
  // to deallocation of the old value_.
  const char* value_chars = decoded_value_.get();
  if (value_chars != NULL) {
    DCHECK(value_chars + strlen(value_chars) < escaped_value.data() ||
           escaped_value.data() + escaped_value.size() < value_chars)
        << "Setting escaped value from substring of unescaped value.";
  }

  decoded_value_.reset(NULL);
  decoding_error_ = false;
  decoded_value_computed_ = false;

  CopyValue(escaped_value, &escaped_value_);
}

const char* HtmlElement::Attribute::quote_str() const {
  switch (quote_style_) {
    case NO_QUOTE:
      return "";
    case SINGLE_QUOTE:
      return "'";
    case DOUBLE_QUOTE:
    default:
      return "\"";
  }
}

void HtmlElement::Attribute::ComputeDecodedValue() const {
  GoogleString buf;
  StringPiece unescaped_value = HtmlKeywords::Unescape(
      escaped_value_.get(), &buf, &decoding_error_);
  CopyValue(unescaped_value, &decoded_value_);
  decoded_value_computed_ = true;
}

}  // namespace net_instaweb
