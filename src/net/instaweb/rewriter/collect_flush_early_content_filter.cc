/*
 * Copyright 2012 Google Inc.
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
// Author: nikhilmadan@google.com (Nikhil Madan)

#include "net/instaweb/rewriter/public/collect_flush_early_content_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

CollectFlushEarlyContentFilter::CollectFlushEarlyContentFilter(
    RewriteDriver* driver)
    : HtmlWriterFilter(driver),
      driver_(driver),
      resource_html_writer_(&resource_html_) {
  Clear();
}

void CollectFlushEarlyContentFilter::StartDocument() {
  Clear();
  set_writer(&null_writer_);
}

void CollectFlushEarlyContentFilter::EndDocument() {
  if (driver_->flushing_early()) {
    return;
  }
  if (!resource_html_.empty()) {
    driver_->flush_early_info()->set_resource_html(resource_html_);
  }
}

void CollectFlushEarlyContentFilter::StartElement(HtmlElement* element) {
  if (driver_->flushing_early()) {
    // Do nothing.
  } else if (element->keyword() == HtmlName::kHead) {
    in_head_ = true;
  } else if (element->keyword() == HtmlName::kNoscript) {
    // Don't detect resources inside noscript tags.
    no_script_element_ = element;
    set_writer(&null_writer_);
  } else if (no_script_element_ == NULL && current_element_ == NULL) {
    // Find javascript elements in the head, and css elements in the entire page
    // and write them to resource_html_writer_.
    semantic_type::Category category;
    HtmlElement::Attribute* attr =  resource_tag_scanner::ScanElement(
        element, driver_, &category);
    if (category == semantic_type::kStylesheet ||
        (category == semantic_type::kScript && in_head_)) {
      if (attr != NULL) {
        StringPiece url(attr->DecodedValueOrNull());
        if (!url.empty()) {
          // Absolutify the url before storing its value so that we handle
          // <base> tags correctly.
          GoogleUrl gurl(driver_->base_url(), url);
          if (gurl.is_valid()) {
            attr->SetValue(gurl.Spec());
            set_writer(&resource_html_writer_);
            current_element_ = element;
          }
        }
      }
    }
  }
  HtmlWriterFilter::StartElement(element);
}

void CollectFlushEarlyContentFilter::EndElement(HtmlElement* element) {
  if (driver_->flushing_early()) {
    // Do nothing if we are flushing early.
  } else  if (element == no_script_element_) {
    // Check if we've reached the end of no_script_element_.
    no_script_element_ = NULL;
  } else if (element->keyword() == HtmlName::kHead) {
    // Check if we are exiting a <head> node.
    if (in_head_) {
      in_head_ = false;
    }
  }
  HtmlWriterFilter::EndElement(element);
  if (element == current_element_) {
    // Once we are done with the current_element_, set the writer back to
    // null_writer_.
    current_element_ = NULL;
    set_writer(&null_writer_);
  }
}

void CollectFlushEarlyContentFilter::Clear() {
  in_head_ = false;
  current_element_ = NULL;
  no_script_element_ = NULL;
  resource_html_.clear();
  HtmlWriterFilter::Clear();
}

}  // namespace net_instaweb
