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

#include "public/html_element.h"

#include <stdio.h>

#include "net/instaweb/htmlparse/html_event.h"
#include "net/instaweb/htmlparse/public/html_keywords.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

HtmlElement::HtmlElement(HtmlElement* parent, const HtmlName& name,
    const HtmlEventListIterator& begin, const HtmlEventListIterator& end)
    : HtmlNode(parent),
      sequence_(-1),
      name_(name),
      begin_(begin),
      end_(end),
      close_style_(AUTO_CLOSE),
      begin_line_number_(-1),
      end_line_number_(-1) {
}

HtmlElement::~HtmlElement() {
  for (int i = 0, n = attribute_size(); i < n; ++i) {
    delete attributes_[i];
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

void HtmlElement::InvalidateIterators(const HtmlEventListIterator& end) {
  set_begin(end);
  set_end(end);
}

void HtmlElement::DeleteAttribute(int i) {
  std::vector<Attribute*>::iterator iter = attributes_.begin() + i;
  delete *iter;
  attributes_.erase(iter);
}

bool HtmlElement::DeleteAttribute(HtmlName::Keyword keyword) {
  for (int i = 0; i < attribute_size(); ++i) {
    const Attribute* attribute = attributes_[i];
    if (attribute->keyword() == keyword) {
      DeleteAttribute(i);
      return true;
    }
  }
  return false;
}

const HtmlElement::Attribute* HtmlElement::FindAttribute(
    HtmlName::Keyword keyword) const {
  const Attribute* ret = NULL;
  for (int i = 0; i < attribute_size(); ++i) {
    const Attribute* attribute = attributes_[i];
    if (attribute->keyword() == keyword) {
      ret = attribute;
      break;
    }
  }
  return ret;
}

void HtmlElement::ToString(GoogleString* buf) const {
  *buf += "<";
  *buf += name_.c_str();
  for (int i = 0; i < attribute_size(); ++i) {
    const Attribute& attribute = *attributes_[i];
    *buf += ' ';
    *buf += attribute.name_str();
    if (attribute.value() != NULL) {
      *buf += "=";
      const char* quote = (attribute.quote() != NULL) ? attribute.quote() : "?";
      *buf += quote;
      *buf += attribute.value();
      *buf += quote;
    }
  }
  switch (close_style_) {
    case AUTO_CLOSE:       *buf += "> (not yet closed)"; break;
    case IMPLICIT_CLOSE:   *buf += ">";  break;
    case EXPLICIT_CLOSE:   *buf += "></";
                           *buf += name_.c_str();
                           *buf += ">";
                           break;
    case BRIEF_CLOSE:      *buf += "/>"; break;
    case UNCLOSED:         *buf += "> (unclosed)"; break;
  }
  if ((begin_line_number_ != -1) || (end_line_number_ != -1)) {
    *buf += " ";
    if (begin_line_number_ != -1) {
      *buf += IntegerToString(begin_line_number_);
    }
    *buf += "...";
    if (end_line_number_ != -1) {
      *buf += IntegerToString(end_line_number_);
    }
  }
}

void HtmlElement::DebugPrint() const {
  GoogleString buf;
  ToString(&buf);
  fprintf(stdout, "%s\n", buf.c_str());
}

void HtmlElement::AddAttribute(const Attribute& src_attr) {
  Attribute* attr = new Attribute(src_attr.name(), src_attr.value(),
                                  src_attr.escaped_value(), src_attr.quote());
  attributes_.push_back(attr);
}

void HtmlElement::AddAttribute(const HtmlName& name, const StringPiece& value,
                               const char* quote) {
  GoogleString buf;
  Attribute* attr = new Attribute(name, value,
                                  HtmlKeywords::Escape(value, &buf), quote);
  attributes_.push_back(attr);
}

void HtmlElement::AddEscapedAttribute(const HtmlName& name,
                                      const StringPiece& escaped_value,
                                      const char* quote) {
  GoogleString buf;
  Attribute* attr = new Attribute(name,
                                  HtmlKeywords::Unescape(escaped_value, &buf),
                                  escaped_value, quote);
  attributes_.push_back(attr);
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
                                  const StringPiece& value,
                                  const StringPiece& escaped_value,
                                  const char* quote)
    : name_(name), quote_(quote) {
  CopyValue(value, &value_);
  CopyValue(escaped_value, &escaped_value_);
}

// Modify value of attribute (eg to rewrite dest of src or href).
// As with the constructor, copies the string in, so caller retains
// ownership of value.
void HtmlElement::Attribute::SetValue(const StringPiece& value) {
  GoogleString buf;
  // Note that we execute the lines in this order in case value
  // is a substring of value_.  This copies the value just prior
  // to deallocation of the old value_.
  const char* escaped_chars = escaped_value_.get();
  DCHECK(value.data() + value.size() < escaped_chars ||
         escaped_chars + strlen(escaped_chars) < value.data())
      << "Setting unescaped value from substring of escaped value.";
  CopyValue(HtmlKeywords::Escape(value, &buf), &escaped_value_);
  CopyValue(value, &value_);
}

void HtmlElement::Attribute::SetEscapedValue(const StringPiece& escaped_value) {
  GoogleString buf;
  // Note that we execute the lines in this order in case value
  // is a substring of value_.  This copies the value just prior
  // to deallocation of the old value_.
  const char* value_chars = value_.get();
  DCHECK(value_chars + strlen(value_chars) < escaped_value.data() ||
         escaped_value.data() + escaped_value.size() < value_chars)
      << "Setting escaped value from substring of unescaped value.";
  CopyValue(HtmlKeywords::Unescape(escaped_value, &buf), &value_);
  CopyValue(escaped_value, &escaped_value_);
}

}  // namespace net_instaweb
