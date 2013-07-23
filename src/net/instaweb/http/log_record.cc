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

AbstractLogRecord::AbstractLogRecord(AbstractMutex* mutex)
    : mutex_(mutex),
      rewriter_info_max_size_(-1),
      allow_logging_urls_(false),
      log_url_indices_(false) {
}

AbstractLogRecord::~AbstractLogRecord() {
  mutex_->DCheckUnlocked();
  // Please do not add non-diagnostic functionality here.
  //
  // AbstractLogRecords are typically owned by reference counted objects, and
  // doing work in the dtor will result in actions being taken at
  // unpredictable times, leading to difficult to diagnose performance
  // and correctness bugs.
}

void AbstractLogRecord::SetIsHtml(bool is_html) {
  ScopedMutex lock(mutex_.get());
  logging_info()->set_is_html_response(true);
}

RewriterInfo* AbstractLogRecord::NewRewriterInfo(const char* rewriter_id) {
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

void AbstractLogRecord::SetRewriterLoggingStatus(
    const char* id, RewriterApplication::Status status) {
  SetRewriterLoggingStatus(id, "", status);
}

RewriterInfo* AbstractLogRecord::SetRewriterLoggingStatusHelper(
    const char* id, const GoogleString& url,
    RewriterApplication::Status application_status) {
  LogRewriterApplicationStatus(id, application_status);

  RewriterInfo* rewriter_info = NewRewriterInfo(id);
  if (rewriter_info == NULL) {
    return NULL;
  }

  ScopedMutex lock(mutex_.get());
  if ((allow_logging_urls_ || log_url_indices_) && url != "") {
    PopulateUrl(url, rewriter_info->mutable_rewrite_resource_info());
  }

  rewriter_info->set_status(application_status);
  return rewriter_info;
}

void AbstractLogRecord::LogRewriterHtmlStatus(
    const char* rewriter_id,
    RewriterHtmlApplication::Status status) {
  ScopedMutex lock(mutex_.get());
  DCHECK(RewriterHtmlApplication::Status_IsValid(status)) << status;
  // TODO(gee): Verify this is called only once?
  rewriter_stats_[rewriter_id].html_status = status;
}

void AbstractLogRecord::LogRewriterApplicationStatus(
    const char* rewriter_id,
    RewriterApplication::Status status) {
  ScopedMutex lock(mutex_.get());
  DCHECK(RewriterApplication::Status_IsValid(status));
  RewriterStatsInternal* stats = &rewriter_stats_[rewriter_id];
  stats->status_counts[status]++;
}

void AbstractLogRecord::SetBlinkRequestFlow(int flow) {
  DCHECK(BlinkInfo::BlinkRequestFlow_IsValid(flow));
  ScopedMutex lock(mutex_.get());
  logging_info()->mutable_blink_info()->set_blink_request_flow(
      static_cast<BlinkInfo::BlinkRequestFlow>(flow));
}

void AbstractLogRecord::SetCacheHtmlRequestFlow(int flow) {
  DCHECK(CacheHtmlLoggingInfo::CacheHtmlRequestFlow_IsValid(flow));
  ScopedMutex lock(mutex_.get());
  CacheHtmlLoggingInfo* cache_html_logging_info =
      logging_info()->mutable_cache_html_logging_info();
  cache_html_logging_info->set_cache_html_request_flow(
      static_cast<CacheHtmlLoggingInfo::CacheHtmlRequestFlow>(flow));
}

void AbstractLogRecord::SetIsOriginalResourceCacheable(bool cacheable) {
  ScopedMutex lock(mutex_.get());
  logging_info()->set_is_original_resource_cacheable(cacheable);
}

void AbstractLogRecord::SetBlinkInfo(const GoogleString& user_agent) {
  ScopedMutex lock(mutex_.get());
  SetBlinkInfoImpl(user_agent);
}

void AbstractLogRecord::SetCacheHtmlLoggingInfo(
    const GoogleString& user_agent) {
  ScopedMutex lock(mutex_.get());
  SetCacheHtmlLoggingInfoImpl(user_agent);
}

bool AbstractLogRecord::WriteLog() {
  ScopedMutex lock(mutex_.get());
  PopulateRewriterStatusCounts();
  return WriteLogImpl();
}

GoogleString AbstractLogRecord::AppliedRewritersString() {
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

void AbstractLogRecord::SetRewriterInfoMaxSize(int x) {
  ScopedMutex lock(mutex_.get());
  rewriter_info_max_size_ = x;
}

void AbstractLogRecord::SetAllowLoggingUrls(bool allow_logging_urls) {
  ScopedMutex lock(mutex_.get());
  allow_logging_urls_ = allow_logging_urls;
}

void AbstractLogRecord::SetLogUrlIndices(bool log_url_indices) {
  ScopedMutex lock(mutex_.get());
  log_url_indices_ = log_url_indices;
}

void AbstractLogRecord::LogFlushEarlyActivity(
    const char* id,
    const GoogleString& url,
    RewriterApplication::Status status,
    FlushEarlyResourceInfo::ContentType content_type,
    FlushEarlyResourceInfo::ResourceType resource_type,
    bool is_bandwidth_affected,
    bool in_head) {
  RewriterInfo* rewriter_info = SetRewriterLoggingStatusHelper(id, url, status);
  if (rewriter_info == NULL) {
    return;
  }

  ScopedMutex lock(mutex_.get());
  FlushEarlyResourceInfo* flush_early_resource_info =
      rewriter_info->mutable_flush_early_resource_info();
  flush_early_resource_info->set_content_type(content_type);
  flush_early_resource_info->set_resource_type(resource_type);
  flush_early_resource_info->set_is_bandwidth_affected(is_bandwidth_affected);
  flush_early_resource_info->set_in_head(in_head);
}

void AbstractLogRecord::LogJsDisableFilter(const char* id,
                                   bool has_pagespeed_no_defer) {
  RewriterInfo* rewriter_info = SetRewriterLoggingStatusHelper(
      id, "", RewriterApplication::APPLIED_OK);
  if (rewriter_info == NULL) {
    return;
  }

  ScopedMutex lock(mutex_.get());
  RewriteResourceInfo* rewrite_resource_info =
      rewriter_info->mutable_rewrite_resource_info();
  rewrite_resource_info->set_has_pagespeed_no_defer(has_pagespeed_no_defer);
}

void AbstractLogRecord::LogLazyloadFilter(
    const char* id, RewriterApplication::Status status,
    bool is_blacklisted, bool is_critical) {
  RewriterInfo* rewriter_info = SetRewriterLoggingStatusHelper(
      id, "", status);
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
}

void AbstractLogRecord::PopulateUrl(
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

void AbstractLogRecord::SetNumHtmlCriticalImages(int num_html_critical_images) {
  ScopedMutex lock(mutex_.get());
  logging_info()->set_num_html_critical_images(num_html_critical_images);
}

void AbstractLogRecord::SetNumCssCriticalImages(int num_css_critical_images) {
  ScopedMutex lock(mutex_.get());
  logging_info()->set_num_css_critical_images(num_css_critical_images);
}

void AbstractLogRecord::SetCriticalCssInfo(int critical_inlined_bytes,
                                   int original_external_bytes,
                                   int overhead_bytes) {
  ScopedMutex lock(mutex_.get());
  CriticalCssInfo* info = logging_info()->mutable_critical_css_info();
  info->set_critical_inlined_bytes(critical_inlined_bytes);
  info->set_original_external_bytes(original_external_bytes);
  info->set_overhead_bytes(overhead_bytes);
}

void AbstractLogRecord::PopulateRewriterStatusCounts() {
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
    if (stats_proto->html_status() == RewriterHtmlApplication::UNKNOWN_STATUS &&
        stats_proto->status_counts_size() > 0) {
      // The filter was active if there are any status counts.
      stats_proto->set_html_status(RewriterHtmlApplication::ACTIVE);
    }
  }
}

void AbstractLogRecord::LogIsXhr(bool is_xhr) {
  ScopedMutex lock(mutex_.get());
  logging_info()->set_is_xhr(is_xhr);
}

void AbstractLogRecord::LogImageBackgroundRewriteActivity(
    RewriterApplication::Status status,
    const GoogleString& url,
    const char* id,
    int original_size,
    int optimized_size,
    bool is_recompressed,
    ImageType original_image_type,
    ImageType optimized_image_type,
    bool is_resized,
    int original_width,
    int original_height,
    bool is_resized_using_rendered_dimensions,
    int resized_width,
    int resized_height) {
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
  image_rewrite_resource_info->set_original_width(original_width);
  image_rewrite_resource_info->set_original_height(original_height);
  image_rewrite_resource_info->set_is_resized_using_rendered_dimensions(
      is_resized_using_rendered_dimensions);
  image_rewrite_resource_info->set_resized_width(resized_width);
  image_rewrite_resource_info->set_resized_height(resized_height);
}

void AbstractLogRecord::SetBackgroundRewriteInfo(
    bool log_urls,
    bool log_url_indices,
    int max_rewrite_info_log_size) {
  SetAllowLoggingUrls(log_urls);
  SetLogUrlIndices(log_url_indices);
  SetRewriterInfoMaxSize(max_rewrite_info_log_size);
}

LogRecord::LogRecord(AbstractMutex* mutex)
    : AbstractLogRecord(mutex),
      logging_info_(new LoggingInfo) {
}

LogRecord::~LogRecord() {}

}  // namespace net_instaweb
