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

#include "net/instaweb/rewriter/public/flush_early_content_writer_filter.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

const char FlushEarlyContentWriterFilter::kPrefetchLinkRelSubresourceHtml[] =
    "<link rel=\"subresource\" href=\"%s\"/>\n";
const char FlushEarlyContentWriterFilter::kPrefetchImageTagHtml[] =
    "new Image().src=\"%s\";";
const char FlushEarlyContentWriterFilter::kPrefetchScriptTagHtml[] =
    "<script type=\"psa_prefetch\" src=\"%s\"></script>\n";
const char FlushEarlyContentWriterFilter::kPrefetchLinkTagHtml[] =
    "<link rel=\"stylesheet\" href=\"%s\" media=\"print\" "
    "disabled=\"true\"/>\n";

const char FlushEarlyContentWriterFilter::kPrefetchStartTimeScript[] =
    "<script type='text/javascript'>"
    "window.mod_pagespeed_prefetch_start = Number(new Date());"
    "window.mod_pagespeed_num_resources_prefetched = %d"
    "</script>";

const char FlushEarlyContentWriterFilter::kNumResourcesFlushedEarly[] =
    "num_resources_flushed_early";

FlushEarlyContentWriterFilter::FlushEarlyContentWriterFilter(
    RewriteDriver* driver)
    : HtmlWriterFilter(driver),
      driver_(driver),
      num_resources_flushed_early_(
          driver->statistics()->GetTimedVariable(kNumResourcesFlushedEarly)) {
  Clear();
}

void FlushEarlyContentWriterFilter::StartDocument() {
  Clear();
  // Note that we set a NullWriter as the writer for this driver, and directly
  // write whatever we need to original_writer_.
  original_writer_ = driver_->writer();
  set_writer(&null_writer_);
  prefetch_mechanism_ = driver_->user_agent_matcher().GetPrefetchMechanism(
      driver_->user_agent());
  current_element_ = NULL;
}

void FlushEarlyContentWriterFilter::EndDocument() {
  if (insert_close_script_) {
    WriteToOriginalWriter("})()</script>");
  }
  if (num_resources_flushed_ > 0) {
    num_resources_flushed_early_->IncBy(num_resources_flushed_);
    WriteToOriginalWriter(
        StringPrintf(kPrefetchStartTimeScript, num_resources_flushed_));
  }
}

void FlushEarlyContentWriterFilter::StartElement(HtmlElement* element) {
  if (prefetch_mechanism_ == UserAgentMatcher::kPrefetchNotSupported ||
      current_element_ != NULL) {
    // Do nothing.
  } else {
    semantic_type::Category category;
    // Extract the resource urls from the page.
    HtmlElement::Attribute* attr = resource_tag_scanner::ScanElement(
        element, driver_, &category);
    if (category == semantic_type::kScript &&
        driver_->options()->Enabled(RewriteOptions::kDeferJavascript)) {
      // Don't flush javascript resources if defer_javascript is enabled.
      // TOOD(nikhilmadan): Check if the User-Agent supports defer_javascript.
    } else if (category == semantic_type::kPrefetch) {
      // Flush the element as such if category is kPrefetch.
      current_element_ = element;
      set_writer(original_writer_);
      if (insert_close_script_) {
        WriteToOriginalWriter("})()</script>");
        insert_close_script_ = false;
      }
    } else if (attr != NULL) {
      StringPiece url(attr->DecodedValueOrNull());
      if (!url.empty()) {
        GoogleUrl gurl(driver_->base_url(), url);
        // Check if they are rewritten. If so, insert the appropriate code to
        // make the browser load these resource early.
        if (driver_->server_context()->IsPagespeedResource(gurl)) {
          ++num_resources_flushed_;
          if (prefetch_mechanism_ == UserAgentMatcher::kPrefetchImageTag) {
            if (!insert_close_script_) {
              WriteToOriginalWriter("<script type=\"text/javascript\">"
                                    "(function(){");
              insert_close_script_ = true;
            }
            WriteToOriginalWriter(
                StringPrintf(kPrefetchImageTagHtml, url.as_string().c_str()));
          } else if (prefetch_mechanism_ ==
                     UserAgentMatcher::kPrefetchLinkRelSubresource) {
            WriteToOriginalWriter(
                StringPrintf(kPrefetchLinkRelSubresourceHtml,
                             url.as_string().c_str()));
          } else if (prefetch_mechanism_ ==
                     UserAgentMatcher::kPrefetchLinkScriptTag) {
            if (category == semantic_type::kScript) {
              WriteToOriginalWriter(
                  StringPrintf(kPrefetchScriptTagHtml, url.as_string().c_str()));
            } else {
              WriteToOriginalWriter(
                  StringPrintf(kPrefetchLinkTagHtml, url.as_string().c_str()));
            }
          }
        }
      }
    }
  }
  HtmlWriterFilter::StartElement(element);
}

void FlushEarlyContentWriterFilter::EndElement(HtmlElement* element) {
  HtmlWriterFilter::EndElement(element);
  if (current_element_ == element) {
    current_element_ = NULL;
    set_writer(&null_writer_);
  }
}

void FlushEarlyContentWriterFilter::Clear() {
  insert_close_script_ = false;
  num_resources_flushed_ = 0;
  prefetch_mechanism_ = UserAgentMatcher::kPrefetchNotSupported;
  original_writer_ = NULL;
  HtmlWriterFilter::Clear();
}

void FlushEarlyContentWriterFilter::WriteToOriginalWriter(
    const GoogleString& in) {
  original_writer_->Write(in, driver_->message_handler());
}

}  // namespace net_instaweb
