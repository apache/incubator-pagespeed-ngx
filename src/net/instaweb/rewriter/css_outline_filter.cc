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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/css_outline_filter.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include <string>
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

const char kStylesheet[] = "stylesheet";

const char CssOutlineFilter::kFilterId[] = "co";

CssOutlineFilter::CssOutlineFilter(HtmlParse* html_parse,
                                   ResourceManager* resource_manager,
                                   size_t size_threshold_bytes)
    : inline_element_(NULL),
      html_parse_(html_parse),
      resource_manager_(resource_manager),
      size_threshold_bytes_(size_threshold_bytes),
      s_link_(html_parse->Intern("link")),
      s_style_(html_parse->Intern("style")),
      s_rel_(html_parse->Intern("rel")),
      s_href_(html_parse->Intern("href")),
      s_type_(html_parse->Intern("type")) { }

void CssOutlineFilter::StartDocument() {
  inline_element_ = NULL;
  buffer_.clear();
}

void CssOutlineFilter::StartElement(HtmlElement* element) {
  // No tags allowed inside style element.
  if (inline_element_ != NULL) {
    // TODO(sligocki): Add negative unit tests to hit these errors.
    html_parse_->ErrorHere("Tag '%s' found inside style.",
                           element->tag().c_str());
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
  if (element->tag() == s_style_) {
    inline_element_ = element;
    buffer_.clear();
  }
}

void CssOutlineFilter::EndElement(HtmlElement* element) {
  if (inline_element_ != NULL) {
    if (element != inline_element_) {
      // No other tags allowed inside style element.
      html_parse_->ErrorHere("Tag '%s' found inside style.",
                             element->tag().c_str());

    } else if (buffer_.size() >= size_threshold_bytes_) {
      OutlineStyle(inline_element_, buffer_);
    } else {
      html_parse_->InfoHere("Inline element not outlined because its size %d, "
                            "is below threshold %d",
                            static_cast<int>(buffer_.size()),
                            static_cast<int>(size_threshold_bytes_));
    }
    inline_element_ = NULL;
    buffer_.clear();
  }
}

void CssOutlineFilter::Flush() {
  // If we were flushed in a style element, we cannot outline it.
  inline_element_ = NULL;
  buffer_.clear();
}

void CssOutlineFilter::Characters(HtmlCharactersNode* characters) {
  if (inline_element_ != NULL) {
    buffer_ += characters->contents();
  }
}

void CssOutlineFilter::Comment(HtmlCommentNode* comment) {
  if (inline_element_ != NULL) {
    html_parse_->ErrorHere("Comment found inside style.");
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
}

void CssOutlineFilter::Cdata(HtmlCdataNode* cdata) {
  if (inline_element_ != NULL) {
    html_parse_->ErrorHere("CDATA found inside style.");
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
}

void CssOutlineFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  if (inline_element_ != NULL) {
    html_parse_->ErrorHere("IE Directive found inside style.");
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
}

// Try to write content and possibly header to resource.
bool CssOutlineFilter::WriteResource(const std::string& content,
                                     OutputResource* resource,
                                     MessageHandler* handler) {
  // We set the TTL of the origin->hashed_name map to 0 because this is
  // derived from the inlined HTML.
  int64 origin_expire_time_ms = 0;
  return resource_manager_->Write(HttpStatus::kOK, content, resource,
                                  origin_expire_time_ms, handler);
}

// Create file with style content and remove that element from DOM.
void CssOutlineFilter::OutlineStyle(HtmlElement* style_element,
                                    const std::string& content) {
  if (html_parse_->IsRewritable(style_element)) {
    // Create style file from content.
    const char* type = style_element->AttributeValue(s_type_);
    // We only deal with CSS styles.  If no type specified, CSS is assumed.
    // See http://www.w3.org/TR/html5/semantics.html#the-style-element
    if (type == NULL || strcmp(type, kContentTypeCss.mime_type()) == 0) {
      MessageHandler* handler = html_parse_->message_handler();
      scoped_ptr<OutputResource> resource(
          resource_manager_->CreateNamedOutputResourceWithPath(
              GoogleUrl::AllExceptLeaf(html_parse_->gurl()), kFilterId, "_",
              &kContentTypeCss, handler));
      // Absolutify URLs in content.
      std::string absolute_content;
      StringWriter absolute_writer(&absolute_content);
      // TODO(sligocki): Use CssParser instead of CssTagScanner hack.
      // TODO(sligocki): Use settable base URL rather than always HTML's URL.
      if (CssTagScanner::AbsolutifyUrls(content, html_parse_->url(),
                                        &absolute_writer, handler) &&
          WriteResource(absolute_content, resource.get(), handler)) {
        HtmlElement* link_element = html_parse_->NewElement(
            style_element->parent(), s_link_);
        link_element->AddAttribute(s_rel_, kStylesheet, "'");
        link_element->AddAttribute(s_href_, resource->url(), "'");
        // Add all style atrributes to link.
        for (int i = 0; i < style_element->attribute_size(); ++i) {
          const HtmlElement::Attribute& attr = style_element->attribute(i);
          link_element->AddAttribute(attr);
        }
        // Add link to DOM.
        html_parse_->InsertElementAfterElement(style_element, link_element);
        // Remove style element from DOM.
        if (!html_parse_->DeleteElement(style_element)) {
          html_parse_->FatalErrorHere("Failed to delete inline sytle element");
        }
      }
    } else {
      std::string element_string;
      style_element->ToString(&element_string);
      html_parse_->InfoHere("Cannot outline non-css stylesheet %s",
                            element_string.c_str());
    }
  }
}

}  // namespace net_instaweb
