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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/css_outline_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

class MessageHandler;

const char kStylesheet[] = "stylesheet";

const char CssOutlineFilter::kFilterId[] = "co";

CssOutlineFilter::CssOutlineFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      inline_element_(NULL),
      size_threshold_bytes_(driver->options()->css_outline_min_bytes()) {
}

CssOutlineFilter::~CssOutlineFilter() {}

void CssOutlineFilter::StartDocumentImpl() {
  inline_element_ = NULL;
  buffer_.clear();
}

void CssOutlineFilter::StartElementImpl(HtmlElement* element) {
  // No tags allowed inside style element.
  if (inline_element_ != NULL) {
    // TODO(sligocki): Add negative unit tests to hit these errors.
    driver_->ErrorHere("Tag '%s' found inside style.",
                           element->name_str());
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
  if (element->keyword() == HtmlName::kStyle) {
    inline_element_ = element;
    buffer_.clear();
  }
}

void CssOutlineFilter::EndElementImpl(HtmlElement* element) {
  if (inline_element_ != NULL) {
    if (element != inline_element_) {
      // No other tags allowed inside style element.
      driver_->ErrorHere("Tag '%s' found inside style.",
                                 element->name_str());

    } else if (buffer_.size() >= size_threshold_bytes_) {
      OutlineStyle(inline_element_, buffer_);
    } else {
      driver_->InfoHere("Inline element not outlined because its size %d, "
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
    driver_->ErrorHere("Comment found inside style.");
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
}

void CssOutlineFilter::Cdata(HtmlCdataNode* cdata) {
  if (inline_element_ != NULL) {
    driver_->ErrorHere("CDATA found inside style.");
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
}

void CssOutlineFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  if (inline_element_ != NULL) {
    driver_->ErrorHere("IE Directive found inside style.");
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
}

// Try to write content and possibly header to resource.
bool CssOutlineFilter::WriteResource(const StringPiece& content,
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
                                    const GoogleString& content_str) {
  StringPiece content(content_str);
  if (driver_->IsRewritable(style_element)) {
    // Create style file from content.
    const char* type = style_element->AttributeValue(HtmlName::kType);
    // We only deal with CSS styles.  If no type specified, CSS is assumed.
    // See http://www.w3.org/TR/html5/semantics.html#the-style-element
    if (type == NULL || strcmp(type, kContentTypeCss.mime_type()) == 0) {
      MessageHandler* handler = driver_->message_handler();
      // Create outline resource at the document location,
      // not base URL location.
      OutputResourcePtr output_resource(
          driver_->CreateOutputResourceWithUnmappedPath(
              driver_->google_url().AllExceptLeaf(), kFilterId, "_",
              &kContentTypeCss, kOutlinedResource, true /*async*/));

      if (output_resource.get() != NULL) {
        // Rewrite URLs in content.
        GoogleString transformed_content;
        StringWriter writer(&transformed_content);
        bool content_valid = true;
        switch (driver_->ResolveCssUrls(base_url(),
                                        output_resource->resolved_base(),
                                        content,
                                        &writer, handler)) {
          case RewriteDriver::kNoResolutionNeeded:
            break;
          case RewriteDriver::kWriteFailed:
            content_valid = false;
            break;
          case RewriteDriver::kSuccess:
            content = transformed_content;
            break;
        }
        if (content_valid &&
            WriteResource(content, output_resource.get(), handler)) {
          HtmlElement* link_element = driver_->NewElement(
              style_element->parent(), HtmlName::kLink);
          driver_->AddAttribute(link_element, HtmlName::kRel, kStylesheet);
          driver_->AddAttribute(link_element, HtmlName::kHref,
                                output_resource->url());
          // Add all style atrributes to link.
          for (int i = 0; i < style_element->attribute_size(); ++i) {
            const HtmlElement::Attribute& attr = style_element->attribute(i);
            link_element->AddAttribute(attr);
          }
          // Add link to DOM.
          driver_->InsertElementAfterElement(style_element, link_element);
          // Remove style element from DOM.
          if (!driver_->DeleteElement(style_element)) {
            driver_->FatalErrorHere("Failed to delete inline sytle element");
          }
        }
      }
    } else {
      GoogleString element_string;
      style_element->ToString(&element_string);
      driver_->InfoHere("Cannot outline non-css stylesheet %s",
                        element_string.c_str());
    }
  }
}

}  // namespace net_instaweb
