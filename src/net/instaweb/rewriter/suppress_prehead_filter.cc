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
        driver_->writer(), &pre_head_writer_));
    set_writer(pre_head_and_response_writer_.get());
  }
}

void SuppressPreheadFilter::StartElement(HtmlElement* element) {
  // If first <head> is seen then do not suppress the bytes.
  if (element->keyword() == HtmlName::kHead && !seen_first_head_) {
    seen_first_head_ = true;
    set_writer(original_writer_);
    UpdateFlushEarlyInfo();
  }
  HtmlWriterFilter::StartElement(element);
}

void SuppressPreheadFilter::Clear() {
  seen_first_head_ = false;
  pre_head_.clear();
  HtmlWriterFilter::Clear();
}

void SuppressPreheadFilter::UpdateFlushEarlyInfo() {
  driver_->flush_early_info()->set_pre_head(pre_head_);
}

}  // namespace net_instaweb
