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

#include "net/instaweb/rewriter/public/js_outline_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;

const char JsOutlineFilter::kFilterId[] = "jo";

JsOutlineFilter::JsOutlineFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      inline_element_(NULL),
      server_context_(driver->server_context()),
      size_threshold_bytes_(driver->options()->js_outline_min_bytes()),
      script_tag_scanner_(driver_) { }

JsOutlineFilter::~JsOutlineFilter() {}

void JsOutlineFilter::StartDocumentImpl() {
  inline_element_ = NULL;
  buffer_.clear();
}

void JsOutlineFilter::StartElementImpl(HtmlElement* element) {
  // No tags allowed inside script element.
  if (inline_element_ != NULL) {
    // TODO(sligocki): Add negative unit tests to hit these errors.
    driver_->ErrorHere("Tag '%s' found inside script.", element->name_str());
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

void JsOutlineFilter::EndElementImpl(HtmlElement* element) {
  if (inline_element_ != NULL) {
    if (element != inline_element_) {
      // No other tags allowed inside script element.
      driver_->ErrorHere("Tag '%s' found inside script.", element->name_str());
    } else if (buffer_.size() >= size_threshold_bytes_) {
      OutlineScript(inline_element_, buffer_);
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
    driver_->ErrorHere("Comment found inside script.");
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
}

void JsOutlineFilter::Cdata(HtmlCdataNode* cdata) {
  if (inline_element_ != NULL) {
    driver_->ErrorHere("CDATA found inside script.");
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
}

void JsOutlineFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  if (inline_element_ != NULL) {
    driver_->ErrorHere("IE Directive found inside script.");
    inline_element_ = NULL;  // Don't outline what we don't understand.
    buffer_.clear();
  }
}

// Try to write content and possibly header to resource.
bool JsOutlineFilter::WriteResource(const GoogleString& content,
                                    OutputResource* resource,
                                    MessageHandler* handler) {
  // We don't provide charset here since in generally we can just inherit
  // from the page.
  // TODO(morlovich) check for proper behavior in case of embedded BOM.
  return server_context_->Write(
      ResourceVector(), content, &kContentTypeJavascript, StringPiece(),
      resource, handler);
}

// Create file with script content and remove that element from DOM.
// TODO(sligocki): We probably will break any relative URL references here.
void JsOutlineFilter::OutlineScript(HtmlElement* inline_element,
                                    const GoogleString& content) {
  if (driver_->IsRewritable(inline_element)) {
    // Create script file from content.
    MessageHandler* handler = driver_->message_handler();
    // Create outline resource at the document location, not base URL location
    OutputResourcePtr resource(
        driver_->CreateOutputResourceWithUnmappedPath(
            driver_->google_url().AllExceptLeaf(), kFilterId, "_",
            kOutlinedResource));
    if (resource.get() != NULL &&
        WriteResource(content, resource.get(), handler)) {
      HtmlElement* outline_element = driver_->CloneElement(inline_element);
      driver_->AddAttribute(outline_element, HtmlName::kSrc,
                            resource->url());
      // Add <script src=...> element to DOM.
      driver_->InsertElementBeforeElement(inline_element,
                                          outline_element);
      // Remove original script element from DOM.
      if (!driver_->DeleteElement(inline_element)) {
        driver_->FatalErrorHere("Failed to delete inline script element");
      }
    } else {
      driver_->ErrorHere("Failed to write outlined script resource.");
    }
  }
}

}  // namespace net_instaweb
