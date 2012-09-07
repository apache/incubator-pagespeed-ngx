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
#include "net/instaweb/rewriter/public/flush_early_info_finder.h"
#include "net/instaweb/rewriter/public/meta_tag_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

SuppressPreheadFilter::SuppressPreheadFilter(RewriteDriver* driver)
    : HtmlWriterFilter(driver),
      driver_(driver),
      pre_head_writer_(&pre_head_) {
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
        original_writer_, &pre_head_writer_));
    set_writer(pre_head_and_response_writer_.get());
  }
  // Setting the charset in response headers related initialization.
  response_headers_.CopyFrom(*(driver_->response_headers()));
  FlushEarlyInfoFinder* finder =
      driver_->server_context()->flush_early_info_finder();
  if (finder != NULL && finder->IsMeaningful()) {
    finder->UpdateFlushEarlyInfoInDriver(driver_);
    charset_ = finder->GetCharset(driver_);
  }
}

void SuppressPreheadFilter::StartElement(HtmlElement* element) {
  if (noscript_element_ == NULL && element->keyword() == HtmlName::kNoscript) {
    noscript_element_ = element;  // Record top-level <noscript>
  } else if (element->keyword() == HtmlName::kHead && !seen_first_head_) {
    // If first <head> is seen then do not suppress the bytes.
    seen_first_head_ = true;
    set_writer(original_writer_);
  }
  HtmlWriterFilter::StartElement(element);
}

void SuppressPreheadFilter::EndElement(HtmlElement* element) {
  HtmlWriterFilter::EndElement(element);
  if (noscript_element_ == NULL &&
      element->keyword() == HtmlName::kMeta) {
    MetaTagFilter::ExtractAndUpdateMetaTagDetails(element, &response_headers_);
  }
  if (element == noscript_element_) {
    noscript_element_ = NULL;  // We are exitting the top-level <noscript>
  }
}

void SuppressPreheadFilter::Clear() {
  seen_first_head_ = false;
  noscript_element_ = NULL;
  pre_head_.clear();
  charset_.clear();
  pre_head_and_response_writer_.reset(NULL);
  response_headers_.Clear();
  HtmlWriterFilter::Clear();
}

void SuppressPreheadFilter::EndDocument() {
  driver_->flush_early_info()->set_pre_head(pre_head_);
  if (!charset_.empty()) {
    GoogleString type = StrCat(";charset=", charset_);
    // Set the charset if it is not already set.
    response_headers_.MergeContentType(type);
  }
  driver_->SaveOriginalHeaders(response_headers_);
}

}  // namespace net_instaweb
