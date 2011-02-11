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

#include "net/instaweb/htmlparse/public/html_writer_filter.h"

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/message_handler.h"
#include <string>
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

static const int kDefaultMaxColumn = -1;

HtmlWriterFilter::HtmlWriterFilter(HtmlParse* html_parse)
    : html_parse_(html_parse),
      writer_(NULL),
      max_column_(kDefaultMaxColumn),
      case_fold_(false) {
  Clear();
}

HtmlWriterFilter::~HtmlWriterFilter() {
}

void HtmlWriterFilter::Clear() {
  lazy_close_element_ = NULL;
  column_ = 0;
  write_errors_ = 0;
}

void HtmlWriterFilter::EmitBytes(const StringPiece& str) {
  if (lazy_close_element_ != NULL) {
    lazy_close_element_ = NULL;
    if (!writer_->Write(">", html_parse_->message_handler())) {
      ++write_errors_;
    }
    ++column_;
  }

  // Search backward from the end for the last occurrence of a newline.
  column_ += str.size();  // if there are no newlines, bump up column counter.
  for (int i = str.size() - 1; i >= 0; --i) {
    if (str[i] == '\n') {
      column_ = str.size() - i - 1;  // found a newline; so reset the column.
      break;
    }
  }
  if (!writer_->Write(str, html_parse_->message_handler())) {
    ++write_errors_;
  }
}

void HtmlWriterFilter::EmitName(const HtmlName& name) {
  if (case_fold_) {
    case_fold_buffer_ = name.c_str();
    LowerString(&case_fold_buffer_);
    EmitBytes(case_fold_buffer_);
  } else {
    EmitBytes(name.c_str());
  }
}

void HtmlWriterFilter::StartElement(HtmlElement* element) {
  EmitBytes("<");
  EmitName(element->name());

  for (int i = 0; i < element->attribute_size(); ++i) {
    const HtmlElement::Attribute& attribute = element->attribute(i);
    // If the column has grown too large, insert a newline.  It's always safe
    // to insert whitespace in the middle of tag parameters.
    int attr_length = 1 + strlen(attribute.name_str());
    if (max_column_ > 0) {
      if (attribute.escaped_value() != NULL) {
        attr_length += 1 + strlen(attribute.escaped_value());
      }
      if ((column_ + attr_length) > max_column_) {
        EmitBytes("\n");
      }
    }
    EmitBytes(" ");
    EmitName(attribute.name());
    if (attribute.escaped_value() != NULL) {
      EmitBytes("=");
      EmitBytes(attribute.quote());
      EmitBytes(attribute.escaped_value());
      EmitBytes(attribute.quote());
    }
  }

  // Attempt to briefly terminate any legal tag that was explicitly terminated
  // in the input.  Note that a rewrite pass might have injected events
  // between the begin/end of an element that was closed briefly in the input
  // html.  In that case it cannot be closed briefly.  It is up to this
  // code to validate BRIEF_CLOSE on each element.
  //
  // TODO(jmarantz): Add a rewrite pass that morphs EXPLICIT_CLOSE into 'brief'
  // when legal.  Such a change will introduce textual diffs between
  // input and output html that would cause htmlparse unit tests to require
  // a regold.  But the changes could be validated with the normalizer.
  if (GetCloseStyle(element) == HtmlElement::BRIEF_CLOSE) {
    lazy_close_element_ = element;
  } else {
    EmitBytes(">");
  }
}

// Compute the tag-closing style for an element. If the style was specified
// on construction, then we use that.  If the element was synthesized by
// a rewrite pass, then it's stored as AUTO_CLOSE, and we can determine
// whether the element is briefly closable or implicitly closed.
HtmlElement::CloseStyle HtmlWriterFilter::GetCloseStyle(HtmlElement* element) {
  HtmlElement::CloseStyle style = element->close_style();
  if (style == HtmlElement::AUTO_CLOSE) {
    HtmlName::Keyword keyword = element->keyword();
    if (html_parse_->IsImplicitlyClosedTag(keyword)) {
      style = HtmlElement::IMPLICIT_CLOSE;
    } else if (html_parse_->TagAllowsBriefTermination(keyword)) {
      style = HtmlElement::BRIEF_CLOSE;
    } else {
      style = HtmlElement::EXPLICIT_CLOSE;
    }
  }
  return style;
}

void HtmlWriterFilter::EndElement(HtmlElement* element) {
  HtmlElement::CloseStyle style = GetCloseStyle(element);
  switch (style) {
    case HtmlElement::AUTO_CLOSE:
      // This cannot happen because GetCloseStyle prevents won't
      // return AUTO_CLOSE.
      html_parse_->message_handler()->FatalError(
          __FILE__, __LINE__, "GetCloseStyle should never return AUTO_CLOSE.");
      break;
    case HtmlElement::IMPLICIT_CLOSE:
      // Nothing new to write; the ">" was written in StartElement
      break;
    case HtmlElement::BRIEF_CLOSE:
      // even if the element is briefly closeable, if more text
      // got written after the element open, then we must
      // explicitly close it, so we fall through.
      if (lazy_close_element_ == element) {
        lazy_close_element_ = NULL;

        // If this attribute was unquoted, or lacked a value, then we'll need
        // to add a space here to ensure that HTML parsers don't interpret the
        // '/' in the '/>' as part of the attribute.
        if (element->attribute_size() != 0) {
          const HtmlElement::Attribute& attribute = element->attribute(
              element->attribute_size() - 1);
          if ((attribute.escaped_value() == NULL) ||
              (attribute.quote()[0] == '\0')) {
            EmitBytes(" ");
          }
        }
        EmitBytes("/>");
        break;
      }
      // fall through
    case HtmlElement::EXPLICIT_CLOSE:
      EmitBytes("</");
      EmitName(element->name());
      EmitBytes(">");
      break;
    case HtmlElement::UNCLOSED:
      // Nothing new to write; the ">" was written in StartElement
      break;
  }
}

void HtmlWriterFilter::Characters(HtmlCharactersNode* chars) {
  EmitBytes(chars->contents());
}

void HtmlWriterFilter::Cdata(HtmlCdataNode* cdata) {
  EmitBytes("<![CDATA[");
  EmitBytes(cdata->contents());
  EmitBytes("]]>");
}

void HtmlWriterFilter::Comment(HtmlCommentNode* comment) {
  EmitBytes("<!--");
  EmitBytes(comment->contents());
  EmitBytes("-->");
}

void HtmlWriterFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  EmitBytes("<!--");
  EmitBytes(directive->contents());
  EmitBytes("-->");
}

void HtmlWriterFilter::Directive(HtmlDirectiveNode* directive) {
  EmitBytes("<!");
  EmitBytes(directive->contents());
  EmitBytes(">");
}

void HtmlWriterFilter::StartDocument() {
  Clear();
}

void HtmlWriterFilter::EndDocument() {
}

void HtmlWriterFilter::Flush() {
  if (!writer_->Flush(html_parse_->message_handler())) {
    ++write_errors_;
  }
}

}  // namespace net_instaweb
