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

#include "net/instaweb/rewriter/public/js_outline_filter.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/http/public/response_headers.h"
#include <string>
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

const char JsOutlineFilter::kFilterId[] = "jo";

JsOutlineFilter::JsOutlineFilter(RewriteDriver* driver)
    : inline_element_(NULL),
      html_parse_(driver->html_parse()),
      resource_manager_(driver->resource_manager()),
      size_threshold_bytes_(driver->options()->js_outline_min_bytes()),
      s_script_(html_parse_->Intern("script")),
      s_src_(html_parse_->Intern("src")),
      s_type_(html_parse_->Intern("type")),
      script_tag_scanner_(html_parse_) { }

void JsOutlineFilter::StartDocument() {
  inline_element_ = NULL;
  buffer_.clear();
}

void JsOutlineFilter::StartElement(HtmlElement* element) {
  // No tags allowed inside script element.
  if (inline_element_ != NULL) {
    // TODO(sligocki): Add negative unit tests to hit these errors.
    html_parse_->ErrorHere("Tag '%s' found inside script.",
                           element->tag().c_str());
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }

  HtmlElement::Attribute* src;
  // We only deal with Javascript
  if (script_tag_scanner_.ParseScriptElement(element, &src) ==
      ScriptTagScanner::kJavaScript) {
    inline_element_ = element;
    buffer_.clear();
    // script elements which already have a src should not be outlined.
    if (src != NULL) {
      inline_element_ = NULL;
    }
  }
}

void JsOutlineFilter::EndElement(HtmlElement* element) {
  if (inline_element_ != NULL) {
    if (element != inline_element_) {
      // No other tags allowed inside script element.
      html_parse_->ErrorHere("Tag '%s' found inside script.",
                             element->tag().c_str());

    } else if (buffer_.size() >= size_threshold_bytes_) {
      OutlineScript(inline_element_, buffer_);
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

void JsOutlineFilter::Flush() {
  // If we were flushed in a script element, we cannot outline it.
  inline_element_ = NULL;
  buffer_.clear();
}

void JsOutlineFilter::Characters(HtmlCharactersNode* characters) {
  if (inline_element_ != NULL) {
    buffer_ += characters->contents();
  }
}

void JsOutlineFilter::Comment(HtmlCommentNode* comment) {
  if (inline_element_ != NULL) {
    html_parse_->ErrorHere("Comment found inside script.");
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
}

void JsOutlineFilter::Cdata(HtmlCdataNode* cdata) {
  if (inline_element_ != NULL) {
    html_parse_->ErrorHere("CDATA found inside script.");
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
}

void JsOutlineFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  if (inline_element_ != NULL) {
    html_parse_->ErrorHere("IE Directive found inside script.");
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
}

// Try to write content and possibly header to resource.
bool JsOutlineFilter::WriteResource(const std::string& content,
                                    OutputResource* resource,
                                    MessageHandler* handler) {
  // We set the TTL of the origin->hashed_name map to 0 because this is
  // derived from the inlined HTML.
  int64 origin_expire_time_ms = 0;
  return resource_manager_->Write(HttpStatus::kOK, content, resource,
                                  origin_expire_time_ms, handler);
}

// Create file with script content and remove that element from DOM.
// TODO(sligocki): We probably will break any relative URL references here.
void JsOutlineFilter::OutlineScript(HtmlElement* inline_element,
                                    const std::string& content) {
  if (html_parse_->IsRewritable(inline_element)) {
    // Create script file from content.
    MessageHandler* handler = html_parse_->message_handler();
    // Create outline resource at the document location, not base URL location
    scoped_ptr<OutputResource> resource(
        resource_manager_->CreateOutputResourceWithPath(
            GoogleUrl::AllExceptLeaf(html_parse_->gurl()), kFilterId, "_",
            &kContentTypeJavascript, handler));
    if (WriteResource(content, resource.get(), handler)) {
      HtmlElement* outline_element = html_parse_->NewElement(
          inline_element->parent(), s_script_);
      outline_element->AddAttribute(s_src_, resource->url(), "'");
      // Add all atrributes from old script element to new script src element.
      for (int i = 0; i < inline_element->attribute_size(); ++i) {
        const HtmlElement::Attribute& attr = inline_element->attribute(i);
        outline_element->AddAttribute(attr);
      }
      // Add <script src=...> element to DOM.
      html_parse_->InsertElementBeforeElement(inline_element,
                                              outline_element);
      // Remove original script element from DOM.
      if (!html_parse_->DeleteElement(inline_element)) {
        html_parse_->FatalErrorHere("Failed to delete inline script element");
      }
    } else {
      html_parse_->ErrorHere("Failed to write outlined script resource.");
    }
  }
}

}  // namespace net_instaweb
