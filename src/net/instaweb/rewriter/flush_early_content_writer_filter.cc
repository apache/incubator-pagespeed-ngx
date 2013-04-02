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

#include <set>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/device_properties.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/flush_early_info_finder.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
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

struct ResourceInfo {
 public:
  ResourceInfo(const GoogleString& url,
               const GoogleString& original_url,
               int64 time_to_download,
               bool is_pagespeed_resource,
               bool in_head)
      : url_(url),
        original_url_(original_url),
        time_to_download_(time_to_download),
        is_pagespeed_resource_(is_pagespeed_resource),
        in_head_(in_head) {}

  GoogleString url_;
  GoogleString original_url_;
  int64 time_to_download_;
  bool is_pagespeed_resource_;
  bool in_head_;
};

namespace {

// Following constants are used to determine the number of additional resources
// that can be flushed early if origin server is slow to respond. Time taken to
// download any resource is calculated as
// (resource_size_in_bytes / kConnectionSpeedBytesPerMs). For every
// kMaxParallelDownload resources, extra kTtfbMs is added. kTtfbMs, kDnsTimeMs
// and kTimeToConnectMs is added before any resource download time is added.
const int kTtfbMs = 60;
const int kDnsTimeMs = 50;
const int kTimeToConnectMs = 55;
const int kMaxParallelDownload = 6;
const int kGzipMultiplier = 3;
const int64 kConnectionSpeedBytesPerMs = 1 * 1024 * 1024 / (8 * 1000);  // 1Mbps

inline int64 TimeToDownload(int64 size) {
  return size / (kConnectionSpeedBytesPerMs * kGzipMultiplier);
}

// Returns true if attr has a valid url (returned in gurl), false otherwise.
bool ExtractUrl(const HtmlElement::Attribute* attr,
                const RewriteDriver* driver,
                GoogleUrl* gurl,
                GoogleString* original_url) {
  if (attr == NULL) {
    return false;
  }
  StringPiece url(attr->DecodedValueOrNull());
  if (url.empty()) {
    return false;
  }
  gurl->Reset(driver->base_url(), url);
  if (!gurl->is_valid()) {
    return false;
  }
  StringVector decoded_url;
  if (driver->DecodeUrl(*gurl, &decoded_url) && decoded_url.size() == 1) {
    // An encoded URL.
    *original_url = decoded_url.at(0);
  } else {
    // Flush early does not handle combined rewritten URLs right now.
    // So, we should not enter this block. But, if we do, we log the rewritten
    // URL as is.
    *original_url = gurl->spec_c_str();
  }
  return true;
}

// Returns the ContentType enum value for a given semantic_type.
FlushEarlyResourceInfo::ContentType GetContentType(
    semantic_type::Category category) {
  switch (category) {
    case semantic_type::kScript: return FlushEarlyResourceInfo::JS;
    case semantic_type::kImage: return FlushEarlyResourceInfo::IMAGE;
    case semantic_type::kStylesheet: return FlushEarlyResourceInfo::CSS;
    default: return FlushEarlyResourceInfo::UNKNOWN_CONTENT_TYPE;
  }
}

}  // namespace

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
  DCHECK(driver_->request_headers() != NULL);
  prefetch_mechanism_ = driver_->user_agent_matcher()->GetPrefetchMechanism(
      driver_->user_agent());
  current_element_ = NULL;
  FlushEarlyInfoFinder* finder =
      driver_->server_context()->flush_early_info_finder();
  if (finder != NULL && finder->IsMeaningful(driver_)) {
    finder->UpdateFlushEarlyInfoInDriver(driver_);
    FlushEarlyRenderInfo* flush_early_render_info =
        driver_->flush_early_render_info();
    if (flush_early_render_info != NULL &&
        flush_early_render_info->private_cacheable_url_size() > 0) {
      private_cacheable_resources_.reset(new StringSet(
          flush_early_render_info->private_cacheable_url().begin(),
          flush_early_render_info->private_cacheable_url().end()));
    }
  }
  FlushEarlyInfo* flush_early_info = driver_->flush_early_info();
  // Set max_available_time_ms_. If average_fetch_latency_ms is not present,
  // then max_available_time_ms_ will be zero and no extra resources will be
  // flushed. For multiple domain shards, it will be somewhat less optimal.
  if (flush_early_info->has_average_fetch_latency_ms()) {
    max_available_time_ms_ = flush_early_info->average_fetch_latency_ms();
  }
  {
    ScopedMutex lock(driver_->log_record()->mutex());
    driver_->log_record()->logging_info()->mutable_flush_early_flow_info()
        ->set_available_time_ms(max_available_time_ms_);
  }
  time_consumed_ms_ = kDnsTimeMs + kTimeToConnectMs + kTtfbMs;
  defer_javascript_enabled_ =
      driver_->options()->Enabled(RewriteOptions::kDeferJavascript);
  // TODO(ksimbili): Enable flush_more_resources_early_if_time_permits after
  // tuning the RTT, bandwidth numbers for mobile.
  flush_more_resources_early_if_time_permits_ =
      driver_->options()->flush_more_resources_early_if_time_permits() &&
      !driver_->device_properties()->IsMobile();
}

void FlushEarlyContentWriterFilter::EndDocument() {
  ResourceInfoList::iterator it = js_resources_info_.begin();
  for (; it != js_resources_info_.end(); ++it) {
    ResourceInfo* js_resource_info = *it;
    bool is_flushed = false;
    if (time_consumed_ms_ + js_resource_info->time_to_download_ <
        max_available_time_ms_) {
      is_flushed = true;
      FlushResources(js_resource_info->url_,
                     js_resource_info->time_to_download_,
                     js_resource_info->is_pagespeed_resource_,
                     semantic_type::kScript);
    }
    GoogleUrl gurl(driver_->base_url(), js_resource_info->url_);
    FlushEarlyResourceInfo::ResourceType resource_type =
        GetResourceType(gurl, js_resource_info->is_pagespeed_resource_);
    RewriterInfo::RewriterApplicationStatus status = is_flushed ?
        RewriterInfo::APPLIED_OK : RewriterInfo::NOT_APPLIED;
    driver_->log_record()->LogFlushEarlyActivity(
       RewriteOptions::FilterId(RewriteOptions::kFlushSubresources),
       js_resource_info->original_url_,
       status,
       FlushEarlyResourceInfo::JS,
       resource_type,
       true /* affected by bandwidth */,
       js_resource_info->in_head_);
  }
  TryFlushingDeferJavascriptEarly();

  if (insert_close_script_) {
    WriteToOriginalWriter("})()</script>");
  }

  if (!flush_early_content_.empty()) {
    WriteToOriginalWriter(flush_early_content_);
  }

  if (num_resources_flushed_ > 0) {
    num_resources_flushed_early_->IncBy(num_resources_flushed_);
  }
  WriteToOriginalWriter(
      StringPrintf(kPrefetchStartTimeScript, num_resources_flushed_));
  Clear();
}

void FlushEarlyContentWriterFilter::TryFlushingDeferJavascriptEarly() {
  bool is_flushed = false;
  bool is_bandwidth_affected = false;
  const RewriteOptions* options = driver_->options();
  // We don't flush defer js here if SplitHtml filter is enabled since blink js
  // contains defer js.
  bool should_try_flushing_early_js_defer_script =
      !options->Enabled(RewriteOptions::kSplitHtml) &&
      defer_javascript_enabled_ &&
      driver_->device_properties()->SupportsJsDefer(
          driver_->options()->enable_aggressive_rewriters_for_mobile()) &&
      flush_more_resources_early_if_time_permits_;
  if (should_try_flushing_early_js_defer_script) {
    StaticAssetManager* static_asset_manager =
        driver_->server_context()->static_asset_manager();
    GoogleString defer_js = static_asset_manager->GetAsset(
            StaticAssetManager::kDeferJs, options);
    int64 time_to_download = TimeToDownload(defer_js.size());
    is_bandwidth_affected = true;
    if (time_consumed_ms_ + time_to_download < max_available_time_ms_) {
      GoogleString defer_js_url = static_asset_manager->GetAssetUrl(
          StaticAssetManager::kDeferJs, options);
      FlushResources(defer_js_url, time_to_download, false,
                     semantic_type::kScript);
      is_flushed = true;
    }
  }
  RewriterInfo::RewriterApplicationStatus status = is_flushed ?
      RewriterInfo::APPLIED_OK : RewriterInfo::NOT_APPLIED;
  driver_->log_record()->LogFlushEarlyActivity(
       RewriteOptions::FilterId(RewriteOptions::kFlushSubresources),
       "",  // defer-js url need not be logged.
       status,
       FlushEarlyResourceInfo::JS,
       FlushEarlyResourceInfo::DEFERJS_SCRIPT,
       is_bandwidth_affected,
       !in_body_);
}

void FlushEarlyContentWriterFilter::StartElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kBody) {
    in_body_ = true;
  }
  if (prefetch_mechanism_ == UserAgentMatcher::kPrefetchNotSupported ||
      current_element_ != NULL) {
    // Do nothing.
  } else {
    semantic_type::Category category;
    // Extract the resource urls from the page.
    HtmlElement::Attribute* attr = resource_tag_scanner::ScanElement(
        element, driver_, &category);
    HtmlElement::Attribute* size_attr =
        element->FindAttribute(HtmlName::kPagespeedSize);
    int64 size = 0;
    if (size_attr != NULL) {
      const char* size_attr_value = size_attr->DecodedValueOrNull();
      if (size_attr_value != NULL) {
        if (!StringToInt64(size_attr_value, &size)) {
          size = 0;
        }
      }
    }

    if (category == semantic_type::kScript &&
        (defer_javascript_enabled_ || in_body_)) {
      // Don't flush javascript resources if defer_javascript is enabled.
      // TOOD(nikhilmadan): Check if the User-Agent supports defer_javascript.
      GoogleUrl gurl;
      GoogleString original_url;
      if (flush_more_resources_early_if_time_permits_ &&
          ExtractUrl(attr, driver_, &gurl, &original_url)) {
        bool is_pagespeed_resource =
            driver_->server_context()->IsNonStalePagespeedResource(gurl);
        // Scripts can be flushed for kPrefetchLinkScriptTag prefetch
        // mechanism only if defer_javascript is disabled and
        // flush_more_resources_in_ie_and_firefox is enabled.
        bool can_flush_js_for_prefetch_link_script_tag =
            prefetch_mechanism_ ==
            UserAgentMatcher::kPrefetchLinkScriptTag &&
            driver_->options()->flush_more_resources_in_ie_and_firefox() &&
            !defer_javascript_enabled_;
        if ((prefetch_mechanism_ == UserAgentMatcher::kPrefetchImageTag ||
             can_flush_js_for_prefetch_link_script_tag) &&
            IsFlushable(gurl, is_pagespeed_resource) && size > 0) {
          // TODO(pulkitg): Add size of private resources also.
          // TODO(pulkitg): Add a mechanism to flush javascript if
          // defer_javascript is enabled and prefetch mechanism is
          // kPrefetchLinkScriptTag.
          int64 time_to_download = TimeToDownload(size);
          ResourceInfo* js_info = new ResourceInfo(
              attr->DecodedValueOrNull(), original_url, time_to_download,
              is_pagespeed_resource, !in_body_);
          js_resources_info_.push_back(js_info);
        } else {
          FlushEarlyResourceInfo::ResourceType resource_type =
              GetResourceType(gurl, is_pagespeed_resource);
          driver_->log_record()->LogFlushEarlyActivity(
              RewriteOptions::FilterId(RewriteOptions::kFlushSubresources),
              original_url,
              RewriterInfo::NOT_APPLIED,
              FlushEarlyResourceInfo::JS,
              resource_type,
              false /* not affected by bandwidth */,
              !in_body_);
        }
      }
    } else if (category == semantic_type::kPrefetch) {
      // Flush the element as such if category is kPrefetch.
      current_element_ = element;
      set_writer(original_writer_);
      if (insert_close_script_) {
        WriteToOriginalWriter("})()</script>");
        insert_close_script_ = false;
      }
    } else {
      GoogleUrl gurl;
      GoogleString original_url;
      if (ExtractUrl(attr, driver_, &gurl, &original_url)) {
        bool call_flush_resources = true;
        int64 time_to_download = 0;
        bool is_bandwidth_affected = false;
        bool is_flushed = false;
        if (category == semantic_type::kImage) {
          time_to_download = size / kConnectionSpeedBytesPerMs;
          bool is_prefetch_mechanism_ok =
              (prefetch_mechanism_ == UserAgentMatcher::kPrefetchImageTag ||
               prefetch_mechanism_ ==
                   UserAgentMatcher::kPrefetchLinkScriptTag);
          bool is_bandwidth_available = (size > 0) &&
              (max_available_time_ms_ > time_consumed_ms_ + time_to_download);
          call_flush_resources = is_prefetch_mechanism_ok &&
              is_bandwidth_available;
          is_bandwidth_affected = is_prefetch_mechanism_ok;
        } else {
          time_to_download =
              size / (kConnectionSpeedBytesPerMs * kGzipMultiplier);
        }
        bool is_pagespeed_resource =
            driver_->server_context()->IsNonStalePagespeedResource(gurl);
        if (call_flush_resources &&
            IsFlushable(gurl, is_pagespeed_resource)) {
          StringPiece url(attr->DecodedValueOrNull());
          FlushResources(url, time_to_download, is_pagespeed_resource,
                         category);
          is_flushed = true;
        }
        FlushEarlyResourceInfo::ResourceType resource_type =
            GetResourceType(gurl, is_pagespeed_resource);
        RewriterInfo::RewriterApplicationStatus status = is_flushed ?
            RewriterInfo::APPLIED_OK : RewriterInfo::NOT_APPLIED;
        driver_->log_record()->LogFlushEarlyActivity(
            RewriteOptions::FilterId(RewriteOptions::kFlushSubresources),
            original_url,
            status,
            GetContentType(category),
            resource_type,
            is_bandwidth_affected,
            !in_body_);
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
  in_body_ = false;
  insert_close_script_ = false;
  num_resources_flushed_ = 0;
  prefetch_mechanism_ = UserAgentMatcher::kPrefetchNotSupported;
  original_writer_ = NULL;
  private_cacheable_resources_.reset(NULL);
  HtmlWriterFilter::Clear();
  time_consumed_ms_ = 0;
  max_available_time_ms_ = 0;
  STLDeleteElements(&js_resources_info_);
  defer_javascript_enabled_ = false;
  flush_early_content_.clear();
  flush_more_resources_early_if_time_permits_ = false;
}

bool FlushEarlyContentWriterFilter::IsFlushable(
    const GoogleUrl& gurl, bool is_pagespeed_resource) {
  return is_pagespeed_resource ||
      (private_cacheable_resources_ != NULL &&
       private_cacheable_resources_->find(gurl.spec_c_str()) !=
           private_cacheable_resources_->end());
}

void FlushEarlyContentWriterFilter::FlushResources(
    StringPiece url, int64 time_to_download,
    bool is_pagespeed_resource, semantic_type::Category category) {
  // Check if they are rewritten. If so, insert the appropriate code to
  // make the browser load these resource early.
  if (is_pagespeed_resource) {
    driver_->increment_num_flushed_early_pagespeed_resources();
    // For every seventh request, there will be one extra RTT.
    if (driver_->num_flushed_early_pagespeed_resources() %
        kMaxParallelDownload == 0) {
      time_consumed_ms_ += kTtfbMs;
    }
  }
  time_consumed_ms_ += time_to_download;

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
        StringPrintf(kPrefetchLinkRelSubresourceHtml, url.as_string().c_str()));
  } else if (prefetch_mechanism_ ==
             UserAgentMatcher::kPrefetchLinkScriptTag) {
    if (category == semantic_type::kScript) {
      StrAppend(&flush_early_content_,
                StringPrintf(kPrefetchScriptTagHtml, url.as_string().c_str()));
    } else if (category == semantic_type::kStylesheet) {
      StrAppend(&flush_early_content_,
                StringPrintf(kPrefetchLinkTagHtml, url.as_string().c_str()));
    } else if (category == semantic_type::kImage &&
        driver_->options()->flush_more_resources_in_ie_and_firefox()) {
      if (!insert_close_script_) {
        WriteToOriginalWriter("<script type=\"text/javascript\">"
                              "(function(){");
        insert_close_script_ = true;
      }
      WriteToOriginalWriter(
          StringPrintf(kPrefetchImageTagHtml, url.as_string().c_str()));
    }
  }
}

void FlushEarlyContentWriterFilter::WriteToOriginalWriter(
    const GoogleString& in) {
  original_writer_->Write(in, driver_->message_handler());
}

FlushEarlyResourceInfo::ResourceType
FlushEarlyContentWriterFilter::GetResourceType(const GoogleUrl& gurl,
                                               bool is_pagespeed_resource) {
  if (is_pagespeed_resource) {
    return FlushEarlyResourceInfo::PAGESPEED;
  }
  if (private_cacheable_resources_ != NULL &&
      private_cacheable_resources_->find(gurl.spec_c_str()) !=
      private_cacheable_resources_->end()) {
    return FlushEarlyResourceInfo::PRIVATE_CACHEABLE;
  }
  return FlushEarlyResourceInfo::NON_PAGESPEED;
}

}  // namespace net_instaweb
