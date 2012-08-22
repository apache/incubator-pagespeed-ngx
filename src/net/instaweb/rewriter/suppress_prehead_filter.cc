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
// Author: mmohabey@google.com (Megha Mohabey)

#include "net/instaweb/rewriter/public/suppress_prehead_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"

namespace net_instaweb {

SuppressPreheadFilter::SuppressPreheadFilter(RewriteDriver* driver)
    : HtmlWriterFilter(driver),
      driver_(driver),
      current_writer_(NULL),
      pre_head_writer_(&pre_head_),
      content_type_meta_tag_writer_(&content_type_meta_tag_) {
  Clear();
}

void SuppressPreheadFilter::StartDocument() {
  Clear();
  original_writer_ = driver_->writer();
  // If the request was flushed early then do not flush the pre head again.
  if (driver_->flushed_early()) {
    // Change the writer to suppress the bytes from being written to the
    // response. Also for storing the new pre head information in property
    // cache.
    set_writer(&pre_head_writer_);
  } else {
    // We have not flushed early so both store the pre_head and allow it to be
    // written to the response.
    pre_head_and_response_writer_.reset(new SplitWriter(
        driver_->writer(), &pre_head_writer_));
    set_writer(pre_head_and_response_writer_.get());
  }
}

void SuppressPreheadFilter::StartElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kNoscript) {
    in_no_script_ = true;
  }
  // If first <head> is seen then do not suppress the bytes.
  if (element->keyword() == HtmlName::kHead && !seen_first_head_) {
    seen_first_head_ = true;
    set_writer(original_writer_);
  }
  if (!in_no_script_ && element->keyword() == HtmlName::kMeta) {
    const HtmlElement::Attribute* equiv;
    const char* attribute;
    // HTTP-EQUIV case.
    if (((equiv = element->FindAttribute(HtmlName::kHttpEquiv)) != NULL &&
         element->FindAttribute(HtmlName::kContent) != NULL &&
         (attribute = equiv->DecodedValueOrNull()) != NULL &&
         StringCaseEqual(attribute, HttpAttributes::kContentType)) ||
    // Charset case.
        element->FindAttribute(HtmlName::kCharset) != NULL) {
      current_writer_ = driver_->writer();
      content_type_meta_tag_and_response_writer_.reset(new SplitWriter(
          current_writer_, &content_type_meta_tag_writer_));
      set_writer(content_type_meta_tag_and_response_writer_.get());
    }
  }
  HtmlWriterFilter::StartElement(element);
}

void SuppressPreheadFilter::EndElement(HtmlElement* element) {
  HtmlWriterFilter::EndElement(element);
  if (current_writer_ != NULL) {
    set_writer(current_writer_);
    current_writer_ = NULL;
  }
  if (element->keyword() == HtmlName::kNoscript) {
    in_no_script_ = false;
  }
}

void SuppressPreheadFilter::Clear() {
  seen_first_head_ = false;
  in_no_script_ = false;
  pre_head_.clear();
  content_type_meta_tag_.clear();
  HtmlWriterFilter::Clear();
}

void SuppressPreheadFilter::EndDocument() {
  driver_->flush_early_info()->set_pre_head(pre_head_);
  driver_->flush_early_info()->set_content_type_meta_tag(
      content_type_meta_tag_);
  driver_->SaveOriginalHeaders();
}

}  // namespace net_instaweb
