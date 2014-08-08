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

#include <memory>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/flush_early_info_finder.h"
#include "net/instaweb/rewriter/public/meta_tag_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"

namespace {

const char kCookieJs[] =
    "(function(){"
    "var data = %s;"
    "for (var i = 0; i < data.length; i++) {"
    "document.cookie = data[i];"
    "}})()";

const char kFetchLatencySeparator[] = ",";

const char kNumFetchLatencyEntries = 10;

}  // namespace

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
  response_headers_.reset(new ResponseHeaders(*driver_->response_headers()));
  charset_ = response_headers_->DetermineCharset();
  has_charset_ = !charset_.empty();
}

void SuppressPreheadFilter::PreHeadDone(HtmlElement* element) {
  seen_first_head_ = true;
  set_writer(original_writer_);
  if (driver_->flushed_early()) {
    SendCookies(element);
  }
}

// TODO(mmohabey): AddHead filter will not add a head in the following case:
// <html><noscript><head></head></noscript></html>. This will break the page if
// FlushSubresources filter is applied.
void SuppressPreheadFilter::StartElement(HtmlElement* element) {
  if (noscript_element_ == NULL && element->keyword() == HtmlName::kNoscript) {
    noscript_element_ = element;  // Record top-level <noscript>
  }
  if (!seen_first_head_ && noscript_element_ == NULL) {
    if (element->keyword() == HtmlName::kHtml) {
      seen_start_html_ = true;
    } else if (element->keyword() == HtmlName::kHead) {
      HtmlWriterFilter::StartElement(element);
      // If the element is Head, flush the node and set seen_first_head_.
      // If HtmlWriterFilter is holding off any bytes due to
      // HtmlElement::BRIEF_CLOSE then emit that.
      HtmlWriterFilter::TerminateLazyCloseElement();
      PreHeadDone(element);
      return;
    } else if (seen_start_html_ && element->keyword() != HtmlName::kHtml) {
      // If the element is other than HTML/HEAD, do not flush it. According to
      // http://www.whatwg.org/specs/web-apps/current-work/multipage/tree-construction.html#the-before-head-insertion-mode,
      // such nodes are part of head.
      PreHeadDone(element);
    }
  }
  HtmlWriterFilter::StartElement(element);
}

void SuppressPreheadFilter::EndElement(HtmlElement* element) {
  HtmlWriterFilter::EndElement(element);
  if (noscript_element_ == NULL &&
      element->keyword() == HtmlName::kMeta) {
    if (!has_charset_) {
      has_charset_ = MetaTagFilter::ExtractAndUpdateMetaTagDetails(
          element, response_headers_.get());
    }
    if (!has_x_ua_compatible_) {
      has_x_ua_compatible_ = ExtractAndUpdateXUACompatible(element);
    }
  }
  if (element == noscript_element_) {
    noscript_element_ = NULL;  // We are exitting the top-level <noscript>
  }
}

void SuppressPreheadFilter::Clear() {
  seen_start_html_ = false;
  seen_first_head_ = false;
  has_charset_ = false;
  has_x_ua_compatible_ = false;
  noscript_element_ = NULL;
  pre_head_.clear();
  charset_.clear();
  pre_head_and_response_writer_.reset(NULL);
  response_headers_.reset();
  HtmlWriterFilter::Clear();
}

void SuppressPreheadFilter::EndDocument() {
  int64 header_fetch_ms = -1;
  {
    bool is_cacheable_html = false;
    {
      AbstractLogRecord* log_record = driver_->log_record();
      ScopedMutex lock(log_record->mutex());
      // It is assumed that default value of is_original_resource_cacheable is
      // true. This field will be set only if original resource is not
      // cacheable.
      is_cacheable_html =
          (!log_record->logging_info()->has_is_original_resource_cacheable() ||
           log_record->logging_info()->is_original_resource_cacheable());
    }  // Release lock before calling GetFetchHeaderMs as it takes the same lock
    // TODO(gee): Fix this.

    // If the html is cacheable, then any resource other than the critical
    // resources may block the html download as html might get served from
    // cache. Thus header_fetch_ms is not populated in that case.
    if (!driver_->flushing_early() && !is_cacheable_html) {
      driver_->request_context()->timing_info().GetFetchHeaderLatencyMs(
          &header_fetch_ms);
    }
  }

  FlushEarlyInfo* flush_early_info = driver_->flush_early_info();

  if (header_fetch_ms >= 0) {
    UpdateFetchLatencyInFlushEarlyProto(header_fetch_ms, driver_);
  } else {
    flush_early_info->clear_average_fetch_latency_ms();
    flush_early_info->clear_last_n_fetch_latencies();
  }

  flush_early_info->set_pre_head(pre_head_);
  // See the description of the HttpOnly cookie in
  // http://tools.ietf.org/html/rfc6265#section-4.1.2.6
  flush_early_info->set_http_only_cookie_present(
      flush_early_info->http_only_cookie_present() ||
      response_headers_->HasAnyCookiesWithAttribute("HttpOnly", NULL));
  if (!has_charset_) {
    FlushEarlyInfoFinder* finder =
        driver_->server_context()->flush_early_info_finder();
    if (finder != NULL && finder->IsMeaningful(driver_)) {
      finder->UpdateFlushEarlyInfoInDriver(driver_);
      charset_ = finder->GetCharset(driver_);
      if (!charset_.empty()) {
        GoogleString type = StrCat(";charset=", charset_);
        response_headers_->MergeContentType(type);
      }
    }
  }
  driver_->SaveOriginalHeaders(*response_headers_);
}

// TODO(marq): Make this a regular method instead of a static method, and have
// it inspect driver_ instead of passing it as a parameter.
void SuppressPreheadFilter::UpdateFetchLatencyInFlushEarlyProto(
    int64 latency, RewriteDriver* driver) {
  double average_fetch_latency = latency;
  GoogleString last_n_fetch_latency;
  FlushEarlyInfo* flush_early_info = driver->flush_early_info();
  if (flush_early_info->has_last_n_fetch_latencies() &&
      flush_early_info->has_average_fetch_latency_ms()) {
    last_n_fetch_latency = flush_early_info->last_n_fetch_latencies();
    average_fetch_latency = flush_early_info->average_fetch_latency_ms();
    StringPieceVector fetch_latency_vector;
    SplitStringPieceToVector(
        last_n_fetch_latency, kFetchLatencySeparator,
        &fetch_latency_vector, true);
    int num_fetch_latency = fetch_latency_vector.size();
    if (num_fetch_latency > kNumFetchLatencyEntries) {
      LOG(WARNING) << "Number of fetch latencies in flush early proto is more "
                 << "than " << kNumFetchLatencyEntries << " for url: "
                 << driver->url();
      average_fetch_latency = 0;
      last_n_fetch_latency = "";
    } else if (num_fetch_latency == kNumFetchLatencyEntries) {
      // If last_n_fetch_latency contains 'n' entries, then remove the entry
      // from the end and add new entry at the front. Also update the average
      // latency.
      int64 nth_latency;
      if (StringToInt64(
          fetch_latency_vector[kNumFetchLatencyEntries - 1].as_string(),
          &nth_latency)) {
        average_fetch_latency = ((average_fetch_latency * num_fetch_latency) -
            nth_latency + latency) / num_fetch_latency;
        last_n_fetch_latency = StrCat(
            Integer64ToString(latency), kFetchLatencySeparator,
            last_n_fetch_latency.substr(
                0, last_n_fetch_latency.find_last_of(kFetchLatencySeparator)));
      }
    } else {
      // last_n_fetch_latency does not contains 'n' entries. Add a new entry at
      // the front and update average.
      average_fetch_latency =
          (average_fetch_latency * (num_fetch_latency) + latency) /
          (num_fetch_latency + 1);
      last_n_fetch_latency = StrCat(
          Integer64ToString(latency), ",",
          flush_early_info->last_n_fetch_latencies());
    }
  } else {
    // Add entry in the proto if no information is present.
    last_n_fetch_latency = Integer64ToString(latency);
  }
  flush_early_info->set_average_fetch_latency_ms(average_fetch_latency);
  flush_early_info->set_last_n_fetch_latencies(last_n_fetch_latency);
}

bool SuppressPreheadFilter::ExtractAndUpdateXUACompatible(
    HtmlElement* element) {
  const HtmlElement::Attribute* equiv =
      element->FindAttribute(HtmlName::kHttpEquiv);
  const HtmlElement::Attribute* value =
      element->FindAttribute(HtmlName::kContent);
  if (equiv != NULL && value!= NULL) {
    StringPiece attribute = equiv->DecodedValueOrNull();
    StringPiece value_str = value->DecodedValueOrNull();
    if (!value_str.empty() && !attribute.empty()) {
      TrimWhitespace(&attribute);

      // http-equiv must equal "Content-Type" and content must not be blank.
      if (StringCaseEqual(attribute, HttpAttributes::kXUACompatible)) {
        if (!response_headers_->HasValue(attribute, value_str)) {
          response_headers_->Add(attribute, value_str);
          return true;
        }
      }
    }
  }
  return false;
}

void SuppressPreheadFilter::SendCookies(HtmlElement* element) {
  GoogleString cookie_str;
  const ResponseHeaders* response_headers = driver_->response_headers();
  if (response_headers->GetCookieString(&cookie_str)) {
    HtmlElement* script = driver_->NewElement(element, HtmlName::kScript);
    driver_->AddAttribute(script, HtmlName::kType, "text/javascript");
    driver_->AddAttribute(script, HtmlName::kPagespeedNoDefer, "");
    HtmlCharactersNode* script_code = driver_->NewCharactersNode(script,
        StringPrintf(kCookieJs, cookie_str.c_str()));
    driver_->PrependChild(element, script);
    driver_->AppendChild(script, script_code);
  }
}

}  // namespace net_instaweb
