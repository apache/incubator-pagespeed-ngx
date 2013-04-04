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

// Author: marq@google.com (Mark Cogan)

#include "net/instaweb/http/public/log_record.h"

#include <map>
#include <set>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

const char kRewriterIdSeparator[] = ",";

LogRecord::LogRecord(AbstractMutex* mutex) : mutex_(mutex) {
  // TODO(gee): Remove multiple initialization methods.
  InitLogging();
}

// Non-initializing constructor for subclasses to invoke.
LogRecord::LogRecord()
    : rewriter_info_max_size_(-1),
      allow_logging_urls_(false),
      log_url_indices_(false) {}

void LogRecord::InitLogging() {
  logging_info_.reset(new LoggingInfo);
  rewriter_info_max_size_ = -1;
  allow_logging_urls_ = false;
  log_url_indices_ = false;
}

LogRecord::~LogRecord() {
  mutex_->DCheckUnlocked();
  // Please do not add non-diagnostic functionality here.
  //
  // LogRecords are typically owned by reference counted objects, and
  // doing work in the dtor will result in actions being taken at
  // unpredictable times, leading to difficult to diagnose performance
  // and correctness bugs.
}

void LogRecord::set_mutex(AbstractMutex* m) {
  CHECK(mutex_.get() == NULL);
  mutex_.reset(m);
}

LoggingInfo* LogRecord::logging_info() {
  return logging_info_.get();
}

void LogRecord::SetIsHtml(bool is_html) {
  ScopedMutex lock(mutex_.get());
  logging_info()->set_is_html_response(true);
}

int LogRecord::AddPropertyCohortInfo(const GoogleString& cohort) {
  ScopedMutex lock(mutex_.get());
  PropertyCohortInfo* cohort_info =
      logging_info()->mutable_property_page_info()->add_cohort_info();
  cohort_info->set_name(cohort);
  return logging_info()->property_page_info().cohort_info_size() - 1;
}

void LogRecord::AddFoundPropertyToCohortInfo(
    int index, const GoogleString& property) {
  ScopedMutex lock(mutex_.get());
  logging_info()->mutable_property_page_info()->mutable_cohort_info(index)->
      add_properties_found(property);
}

void LogRecord::SetCacheStatusForCohortInfo(
    int index, bool found, int key_state) {
  ScopedMutex lock(mutex_.get());
  PropertyCohortInfo* cohort_info =
      logging_info()->mutable_property_page_info()->mutable_cohort_info(index);
  cohort_info->set_is_cache_hit(found);
  cohort_info->set_cache_key_state(key_state);
}

void LogRecord::SetDeviceAndCacheTypeForCohortInfo(int index, int device_type,
                                                   int cache_type) {
  ScopedMutex lock(mutex_.get());
  PropertyCohortInfo* cohort_info =
      logging_info()->mutable_property_page_info()->mutable_cohort_info(index);
  cohort_info->set_device_type(device_type);
  cohort_info->set_cache_type(cache_type);
}

RewriterInfo* LogRecord::NewRewriterInfo(const char* rewriter_id) {
  ScopedMutex lock(mutex_.get());
  if (rewriter_info_max_size_ != -1 &&
      logging_info()->rewriter_info_size() >= rewriter_info_max_size_) {
    if (!logging_info()->rewriter_info_size_limit_exceeded()) {
      VLOG(1) << "Exceeded size limit for rewriter info.";
      logging_info()->set_rewriter_info_size_limit_exceeded(true);
    }
    return NULL;
  }
  RewriterInfo* rewriter_info = logging_info()->add_rewriter_info();
  rewriter_info->set_id(rewriter_id);
  return rewriter_info;
}

void LogRecord::SetRewriterLoggingStatus(
    const char* id, RewriterApplication::Status status) {
  SetRewriterLoggingStatus(id, "", status);
}

void LogRecord::SetRewriterLoggingStatus(
    const char* id, const GoogleString& url,
    RewriterApplication::Status application_status) {
  LogRewriterApplicationStatus(id, application_status);

  RewriterInfo* rewriter_info = NewRewriterInfo(id);
  if (rewriter_info == NULL) {
    return;
  }

  ScopedMutex lock(mutex_.get());
  if ((allow_logging_urls_ || log_url_indices_) && url != "") {
    PopulateUrl(url, rewriter_info->mutable_rewrite_resource_info());
  }

  rewriter_info->set_status(application_status);
}

void LogRecord::LogRewriterHtmlStatus(
    const char* rewriter_id,
    RewriterStats::RewriterHtmlStatus status) {
  ScopedMutex lock(mutex_.get());
  DCHECK(RewriterStats::RewriterHtmlStatus_IsValid(status)) << status;
  // TODO(gee): Verify this is called only once?
  rewriter_stats_[rewriter_id].html_status = status;
}

void LogRecord::LogRewriterApplicationStatus(
    const char* rewriter_id,
    RewriterApplication::Status status) {
  ScopedMutex lock(mutex_.get());
  DCHECK(RewriterApplication::Status_IsValid(status));
  RewriterStatsInternal* stats = &rewriter_stats_[rewriter_id];
  stats->status_counts[status]++;
}

void LogRecord::SetBlinkRequestFlow(int flow) {
  DCHECK(BlinkInfo::BlinkRequestFlow_IsValid(flow));
  ScopedMutex lock(mutex_.get());
  logging_info()->mutable_blink_info()->set_blink_request_flow(
      static_cast<BlinkInfo::BlinkRequestFlow>(flow));
}

void LogRecord::SetCacheHtmlRequestFlow(int flow) {
  DCHECK(CacheHtmlLoggingInfo::CacheHtmlRequestFlow_IsValid(flow));
  ScopedMutex lock(mutex_.get());
  CacheHtmlLoggingInfo* cache_html_logging_info =
      logging_info()->mutable_cache_html_logging_info();
  cache_html_logging_info->set_cache_html_request_flow(
      static_cast<CacheHtmlLoggingInfo::CacheHtmlRequestFlow>(flow));
}

void LogRecord::SetIsOriginalResourceCacheable(bool cacheable) {
  ScopedMutex lock(mutex_.get());
  logging_info()->set_is_original_resource_cacheable(cacheable);
}

void LogRecord::SetTimingRequestStartMs(int64 ms) {
  ScopedMutex lock(mutex_.get());
  logging_info()->mutable_timing_info()->set_request_start_ms(ms);
}

void LogRecord::SetTimingHeaderFetchMs(int64 ms) {
  ScopedMutex lock(mutex_.get());
  logging_info()->mutable_timing_info()->set_header_fetch_ms(ms);
}

void LogRecord::SetTimingFetchMs(int64 ms) {
  ScopedMutex lock(mutex_.get());
  logging_info()->mutable_timing_info()->set_fetch_ms(ms);
}

int64 LogRecord::GetTimingFetchMs() {
  ScopedMutex lock(mutex_.get());
  if (logging_info()->has_timing_info()) {
    return logging_info()->timing_info().fetch_ms();
  } else {
    return 0;
  }
}

void LogRecord::SetTimingProcessingTimeMs(int64 ms) {
  ScopedMutex lock(mutex_.get());
  logging_info()->mutable_timing_info()->set_processing_time_ms(ms);
}

void LogRecord::UpdateTimingInfoWithFetchStartTime(int64 start_time_ms) {
  ScopedMutex lock(mutex_.get());
  TimingInfo* timing_info = logging_info()->mutable_timing_info();
  if (timing_info->has_request_start_ms()) {
    timing_info->set_time_to_start_fetch_ms(
      start_time_ms - timing_info->request_start_ms());
  }
}

void LogRecord::SetBlinkInfo(const GoogleString& user_agent) {
  ScopedMutex lock(mutex_.get());
  SetBlinkInfoImpl(user_agent);
}

void LogRecord::SetCacheHtmlLoggingInfo(const GoogleString& user_agent) {
  SetCacheHtmlInfoImpl(user_agent);
}

bool LogRecord::WriteLog() {
  ScopedMutex lock(mutex_.get());
  PopulateRewriterStatusCounts();
  return WriteLogImpl();
}

GoogleString LogRecord::AppliedRewritersString() {
  mutex_->DCheckLocked();
  StringSet applied_rewriters;
  for (int i = 0, e = logging_info()->rewriter_info_size();
       i < e; ++i) {
    RewriterInfo info = logging_info()->rewriter_info(i);
    if (info.status() == RewriterApplication::APPLIED_OK) {
      applied_rewriters.insert(info.id());
    }
  }

  // TODO(gee): Use the rewriter stats to construct the string.

  GoogleString rewriters_str;
  for (StringSet::iterator begin = applied_rewriters.begin(),
       iter = applied_rewriters.begin(), end = applied_rewriters.end();
       iter != end; ++iter) {
    if (iter != begin) {
      StrAppend(&rewriters_str, kRewriterIdSeparator);
    }
    DCHECK((*iter).find(kRewriterIdSeparator) == GoogleString::npos) <<
       "No comma should appear in a rewriter ID";
    StrAppend(&rewriters_str, *iter);
  }
  return rewriters_str;
}

void LogRecord::SetRewriterInfoMaxSize(int x) {
  ScopedMutex lock(mutex_.get());
  rewriter_info_max_size_ = x;
}

void LogRecord::SetAllowLoggingUrls(bool allow_logging_urls) {
  ScopedMutex lock(mutex_.get());
  allow_logging_urls_ = allow_logging_urls;
}

void LogRecord::SetLogUrlIndices(bool log_url_indices) {
  ScopedMutex lock(mutex_.get());
  log_url_indices_ = log_url_indices;
}

void LogRecord::LogFlushEarlyActivity(
    const char* id,
    const GoogleString& url,
    RewriterApplication::Status status,
    FlushEarlyResourceInfo::ContentType content_type,
    FlushEarlyResourceInfo::ResourceType resource_type,
    bool is_bandwidth_affected,
    bool in_head) {
  RewriterInfo* rewriter_info = NewRewriterInfo(id);
  if (rewriter_info == NULL) {
    return;
  }

  ScopedMutex lock(mutex_.get());
  if ((allow_logging_urls_ || log_url_indices_) && url != "") {
    PopulateUrl(url, rewriter_info->mutable_rewrite_resource_info());
  }
  rewriter_info->set_status(status);
  FlushEarlyResourceInfo* flush_early_resource_info =
      rewriter_info->mutable_flush_early_resource_info();
  flush_early_resource_info->set_content_type(content_type);
  flush_early_resource_info->set_resource_type(resource_type);
  flush_early_resource_info->set_is_bandwidth_affected(is_bandwidth_affected);
  flush_early_resource_info->set_in_head(in_head);
}

void LogRecord::LogImageRewriteActivity(
    const char* id,
    const GoogleString& url,
    RewriterApplication::Status status,
    bool is_image_inlined,
    bool is_critical_image,
    bool try_low_res_src_insertion,
    bool low_res_src_inserted,
    int low_res_data_size) {
  LogRewriterApplicationStatus(id, status);

  RewriterInfo* rewriter_info = NewRewriterInfo(id);
  if (rewriter_info == NULL) {
    return;
  }

  ScopedMutex lock(mutex_.get());
  RewriteResourceInfo* rewrite_resource_info =
      rewriter_info->mutable_rewrite_resource_info();
  if ((allow_logging_urls_ || log_url_indices_) && url != "") {
    PopulateUrl(url, rewrite_resource_info);
  }

  rewrite_resource_info->set_is_inlined(is_image_inlined);
  rewrite_resource_info->set_is_critical(is_critical_image);
  if (try_low_res_src_insertion) {
    ImageRewriteResourceInfo* image_rewrite_resource_info =
        rewriter_info->mutable_image_rewrite_resource_info();
    image_rewrite_resource_info->set_is_low_res_src_inserted(
        low_res_src_inserted);
    image_rewrite_resource_info->set_low_res_size(low_res_data_size);
  }

  rewriter_info->set_status(status);
}

void LogRecord::LogJsDisableFilter(const char* id,
                                   bool has_pagespeed_no_defer) {
  LogRewriterApplicationStatus(id, RewriterApplication::APPLIED_OK);

  RewriterInfo* rewriter_info = NewRewriterInfo(id);
  if (rewriter_info == NULL) {
    return;
  }

  ScopedMutex lock(mutex_.get());
  RewriteResourceInfo* rewrite_resource_info =
      rewriter_info->mutable_rewrite_resource_info();
  rewrite_resource_info->set_has_pagespeed_no_defer(has_pagespeed_no_defer);
  rewriter_info->set_status(RewriterApplication::APPLIED_OK);
}

void LogRecord::LogLazyloadFilter(
    const char* id, RewriterApplication::Status status,
    bool is_blacklisted, bool is_critical) {
  RewriterInfo* rewriter_info = NewRewriterInfo(id);
  if (rewriter_info == NULL) {
    return;
  }

  ScopedMutex lock(mutex_.get());
  RewriteResourceInfo* rewrite_resource_info =
      rewriter_info->mutable_rewrite_resource_info();
  if (is_blacklisted) {
    rewrite_resource_info->set_is_blacklisted(is_blacklisted);
  }
  if (is_critical) {
    rewrite_resource_info->set_is_critical(is_critical);
  }
  rewriter_info->set_status(status);
}

void LogRecord::PopulateUrl(
    const GoogleString& url, RewriteResourceInfo* rewrite_resource_info) {
  mutex()->DCheckLocked();
  std::pair<StringIntMap::iterator, bool> result = url_index_map_.insert(
      std::make_pair(url, 0));
  StringIntMap::iterator iter = result.first;
  if (result.second) {
    iter->second = url_index_map_.size() - 1;
    if (allow_logging_urls_) {
      ResourceUrlInfo* resource_url_info =
          logging_info()->mutable_resource_url_info();
      resource_url_info->add_url(url);
    }
  }

  rewrite_resource_info->set_original_resource_url_index(iter->second);
}

void LogRecord::SetNumHtmlCriticalImages(int num_html_critical_images) {
  ScopedMutex lock(mutex_.get());
  logging_info()->set_num_html_critical_images(num_html_critical_images);
}

void LogRecord::SetNumCssCriticalImages(int num_css_critical_images) {
  ScopedMutex lock(mutex_.get());
  logging_info()->set_num_css_critical_images(num_css_critical_images);
}

void LogRecord::SetImageStats(int num_img_tags, int num_inlined_img_tags) {
  ScopedMutex lock(mutex_.get());
  logging_info()->mutable_image_stats()->set_num_img_tags(num_img_tags);
  logging_info()->mutable_image_stats()
      ->set_num_inlined_img_tags(num_inlined_img_tags);
}

void LogRecord::SetCriticalCssInfo(int critical_inlined_bytes,
                                   int original_external_bytes,
                                   int overhead_bytes) {
  ScopedMutex lock(mutex_.get());
  CriticalCssInfo* info = logging_info()->mutable_critical_css_info();
  info->set_critical_inlined_bytes(critical_inlined_bytes);
  info->set_original_external_bytes(original_external_bytes);
  info->set_overhead_bytes(overhead_bytes);
}

void LogRecord::PopulateRewriterStatusCounts() {
  mutex_->DCheckLocked();
  if (logging_info() == NULL) {
    return;
  }

  if (logging_info()->rewriter_stats_size() > 0) {
    DLOG(FATAL) <<  "PopulateRewriterStatusCounts should be called only once";
    return;
  }

  for (RewriterStatsMap::const_iterator iter = rewriter_stats_.begin();
       iter != rewriter_stats_.end();
       ++iter) {
    const GoogleString& rewriter_id = iter->first;
    const RewriterStatsInternal& stats = iter->second;
    RewriterStats* stats_proto = logging_info()->add_rewriter_stats();
    stats_proto->set_id(rewriter_id);
    stats_proto->set_html_status(stats.html_status);
    for (RewriteStatusCountMap::const_iterator iter =
             stats.status_counts.begin();
         iter != stats.status_counts.end();
         ++iter) {
      const RewriterApplication::Status application_status = iter->first;
      DCHECK(RewriterApplication::Status_IsValid(application_status));
      const int count = iter->second;
      CHECK_GE(count, 1);
      RewriteStatusCount* status_count = stats_proto->add_status_counts();
      status_count->set_application_status(application_status);
      status_count->set_count(count);
    }
  }
}

void LogRecord::LogDeviceInfo(
    int device_type,
    bool supports_image_inlining,
    bool supports_lazyload_images,
    bool supports_critical_images_beacon,
    bool supports_deferjs,
    bool supports_webp,
    bool supports_webplossless_alpha,
    bool is_bot,
    bool supports_split_html,
    bool can_preload_resources) {
  ScopedMutex lock(mutex_.get());
  DeviceInfo* device_info = logging_info()->mutable_device_info();
  device_info->set_device_type(device_type);
  device_info->set_supports_image_inlining(supports_image_inlining);
  device_info->set_supports_lazyload_images(supports_lazyload_images);
  device_info->set_supports_critical_images_beacon(
      supports_critical_images_beacon);
  device_info->set_supports_deferjs(supports_deferjs);
  device_info->set_supports_webp(supports_webp);
  device_info->set_supports_webplossless_alpha(supports_webplossless_alpha);
  device_info->set_is_bot(is_bot);
  device_info->set_supports_split_html(supports_split_html);
  device_info->set_can_preload_resources(can_preload_resources);
}

void LogRecord::LogImageBackgroundRewriteActivity(
    RewriterApplication::Status status,
    const GoogleString& url,
    const char* id,
    int original_size,
    int optimized_size,
    bool is_recompressed,
    ImageType original_image_type,
    ImageType optimized_image_type,
    bool is_resized) {
  RewriterInfo* rewriter_info = NewRewriterInfo(id);
  if (rewriter_info == NULL) {
    return;
  }

  ScopedMutex lock(mutex());
  RewriteResourceInfo* rewrite_resource_info =
      rewriter_info->mutable_rewrite_resource_info();

  // Log the URL and URL indices if rewriting failed and if logging them
  // are enabled.
  if ((status != RewriterApplication::APPLIED_OK) &&
      (allow_logging_urls_ || log_url_indices_) && !url.empty()) {
    PopulateUrl(url, rewrite_resource_info);
  }

  rewriter_info->set_id(id);
  rewriter_info->set_status(status);

  rewrite_resource_info->set_original_size(original_size);
  // Size of the optimized image is logged when it is different from that of
  // the original image.
  if (original_size != optimized_size) {
    rewrite_resource_info->set_optimized_size(optimized_size);
  }
  rewrite_resource_info->set_is_recompressed(is_recompressed);

  ImageRewriteResourceInfo* image_rewrite_resource_info =
      rewriter_info->mutable_image_rewrite_resource_info();
  image_rewrite_resource_info->set_original_image_type(
      original_image_type);
  // Type of the optimized image is logged when it is different from that of
  // the original image.
  if (original_image_type != optimized_image_type) {
    image_rewrite_resource_info->set_optimized_image_type(
        optimized_image_type);
  }

  image_rewrite_resource_info->set_is_resized(is_resized);
}

void LogRecord::SetBackgroundRewriteInfo(
    bool log_urls,
    bool log_url_indices,
    int max_rewrite_info_log_size) {
  SetAllowLoggingUrls(log_urls);
  SetLogUrlIndices(log_url_indices);
  SetRewriterInfoMaxSize(max_rewrite_info_log_size);
}

}  // namespace net_instaweb
