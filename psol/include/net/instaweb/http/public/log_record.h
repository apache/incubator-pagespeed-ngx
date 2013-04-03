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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_LOG_RECORD_H_
#define NET_INSTAWEB_HTTP_PUBLIC_LOG_RECORD_H_

#include <map>
#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest_prod.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

// If your .cc file needs to use the types declared in logging_proto.h,
// you must also include net/instaweb/http/public/logging_proto_impl.h
// See that header file for an explanation of why this is necessary.


namespace net_instaweb {

class AbstractMutex;

// This class is a wrapper around a protobuf used to collect logging
// information. It also provides a simple aggregation mechanism for
// collecting the ids of applied rewriters.
//
// Care and feeding of log records:
//  (1) All logging must be done through log records. No class should
//      have static members of any logging proto class. Log records
//      can either create the logging protos, or will take ownership of them.
//  (2) All access and manipulation of log data must be guarded by the log
//      record's mutex. Commonly repeated logging operations should be factored
//      into functions in this class (and be so guarded therein).
//  (3) In most cases, log records should be created and owned by request
//      contexts.

// Subclasses may wrap some other type of protobuf; they must still provide
// access to a LogRecord instance, however.
class LogRecord  {
 public:
  // Construct a LogRecord with a new LoggingInfo proto and caller-
  // supplied mutex. This class takes ownership of the mutex.
  explicit LogRecord(AbstractMutex* mutex);
  virtual ~LogRecord();

  // For compatibility with older logging methods, returns a comma-joined string
  // concatenating the sorted coalesced rewriter ids of APPLIED_OK entries in
  // the rewriter_info array. Each id will appear once in the string if any
  // number of successful rewrites for that id have been logged.
  GoogleString AppliedRewritersString();

  // Create a new rewriter logging submessage for |rewriter_id|, returning a
  // pointer to it for later access. Note that this can return NULL if the
  // size of rewriter_info has grown too large. It is the caller's
  // responsibility to handle this safely.
  RewriterInfo* NewRewriterInfo(const char* rewriter_id);

  // Creates a new rewriter logging submessage for |rewriter_id|,
  // and sets status it.
  void SetRewriterLoggingStatus(
      const char* rewriter_id, RewriterInfo::RewriterApplicationStatus status);

  // Creates a new rewriter logging submessage for |rewriter_id|,
  // sets status and the url index.
  void SetRewriterLoggingStatus(
      const char* rewriter_id, const GoogleString& url,
      RewriterInfo::RewriterApplicationStatus status);

  // Log the HTML level status for a filter.  This should be called only once
  // per filter, at the point where it is determined the filter is either
  // active or not.
  void LogRewriterHtmlStatus(const char* rewriter_id,
                             RewriterStats::RewriterHtmlStatus status);

  // Log the status of a rewriter application on a resource.
  // TODO(gee): I'd really prefer rewriter_id was an enum.
  void LogRewriterApplicationStatus(
      const char* rewriter_id, RewriterInfo::RewriterApplicationStatus status);

  // Return the LoggingInfo proto wrapped by this class. Calling code must
  // guard any reads and writes to this using mutex().
  virtual LoggingInfo* logging_info();

  // Atomically sets is_html_response in the logging proto.
  void SetIsHtml(bool is_html);

  // Adds a new cohort info with the given cohort name and returns its index.
  int AddPropertyCohortInfo(const GoogleString& cohort);

  // Updates the cohort info at the specified index, to include the given
  // property in the last of properties found in the cache.
  void AddFoundPropertyToCohortInfo(int index, const GoogleString& property);

  // Updates the cohort info at the specified index, to indicate whether it was
  // a cache hit.
  void SetCacheStatusForCohortInfo(int index, bool found, int key_state);

  // Updates the cohort info at the specified index with the device and cache
  // type.
  void SetDeviceAndCacheTypeForCohortInfo(
      int index, int device_type, int cache_type);

  // Mutex-guarded log mutation convenience methods. The rule of thumb is that
  // if a single-field update to a logging proto occurs multiple times, it
  // should be factored out into a method on this class.
  void SetBlinkRequestFlow(int flow);
  void SetCacheHtmlRequestFlow(int flow);
  void SetIsOriginalResourceCacheable(bool cacheable);
  void SetTimingRequestStartMs(int64 ms);
  void SetTimingHeaderFetchMs(int64 ms);
  void SetTimingFetchMs(int64 ms);
  int64 GetTimingFetchMs();
  void SetTimingProcessingTimeMs(int64 ms);
  // Sets time_to_start_fetch_ms in the TimingInfo submessage as an offset from
  // timing_info.request_start_ms (|start_time_ms| is an absolute time value
  // and is converted into the offset). If request_start_ms is unset, this is a
  // silent no-op. This may be called several times in sucession, for example
  // in the case of retried fetches. In that case, if time_to_start_fetch_ms has
  // already been set in the log record, this is again a silent no-op.
  void UpdateTimingInfoWithFetchStartTime(int64 start_time_ms);

  // Override SetBlinkInfoImpl if necessary.
  void SetBlinkInfo(const GoogleString& user_agent);

  // Override SetCacheHtmlInfoImpl if necessary.
  void SetCacheHtmlLoggingInfo(const GoogleString& user_agent);

  // Log a RewriterInfo for the flush early filter.
  void LogFlushEarlyActivity(
      const char* id,
      const GoogleString& url,
      RewriterInfo::RewriterApplicationStatus status,
      FlushEarlyResourceInfo::ContentType content_type,
      FlushEarlyResourceInfo::ResourceType resource_type,
      bool is_bandwidth_affected,
      bool in_head);

  // Log a RewriterInfo for the image rewrite filter.
  void LogImageRewriteActivity(
      const char* id,
      const GoogleString& url,
      RewriterInfo::RewriterApplicationStatus status,
      bool is_image_inlined,
      bool is_critical_image,
      bool try_low_res_src_insertion,
      bool low_res_src_inserted,
      int low_res_data_size);

  // TODO(gee): Change the callsites.
  void LogJsDisableFilter(const char* id, bool has_pagespeed_no_defer);

  void LogLazyloadFilter(const char* id,
                         RewriterInfo::RewriterApplicationStatus status,
                         bool is_blacklisted, bool is_critical);

  // Mutex-guarded log-writing operations. Derived classes should override
  // *Impl methods. Returns false if the log write attempt failed.
  bool WriteLog();

  // Return the mutex associated with this instance. Calling code should
  // guard reads and writes of LogRecords
  AbstractMutex* mutex() { return mutex_.get(); }

  // Sets the maximum number of RewriterInfo submessages that can accumulate in
  // the LoggingInfo proto wrapped by this class.
  void SetRewriterInfoMaxSize(int x);

  // Sets whether urls should be logged. This could potentially generate a lot
  // of logs data, so this should be switched on only for debugging.
  void SetAllowLoggingUrls(bool allow_logging_urls);

  // Sets whether URL indices should be logged for every rewriter application
  // or not.
  void SetLogUrlIndices(bool log_url_indices);

  // Sets the number of critical images in HTML.
  void SetNumHtmlCriticalImages(int num_html_critical_images);

  // Sets the number of critical images in CSS.
  void SetNumCssCriticalImages(int num_css_critical_images);

  // Sets image related statistics.
  void SetImageStats(int num_img_tags, int num_inlined_img_tags);

  // Sets critical CSS related byte counts (all uncompressed).
  void SetCriticalCssInfo(int critical_inlined_bytes,
                          int original_external_bytes,
                          int overhead_bytes);

  // Log information related to the user agent and device making the request.
  void LogDeviceInfo(
      int device_type,
      bool supports_image_inlining,
      bool supports_lazyload_images,
      bool supports_critical_images_beacon,
      bool supports_deferjs,
      bool supports_webp,
      bool supports_webplossless_alpha,
      bool is_bot,
      bool supports_split_html,
      bool can_preload_resources);

 protected:
  // Non-initializing default constructor for subclasses. Subclasses that invoke
  // this constructor should implement and call their own initializer that
  // instantiates the wrapped logging proto and calls set_mutex with a valid
  // Mutex object.
  LogRecord();

  void set_mutex(AbstractMutex* m);

  // Implements setting Blink-specific log information; base impl is a no-op.
  virtual void SetBlinkInfoImpl(const GoogleString& user_agent) {}

  // Implements setting CacheHtml-specific log information
  void SetCacheHtmlInfoImpl(const GoogleString& user_agent) {}
  // Implements writing a log, base implementation is a no-op. Returns false if
  // writing failed.
  virtual bool WriteLogImpl() { return true; }

 private:
  // Called on construction.
  void InitLogging();

  void PopulateUrl(
      const GoogleString& url, RewriteResourceInfo* rewrite_resource_info);

  // Fill LoggingInfo proto with information collected from LogRewriterStatus
  // and LogRewrite.
  void PopulateRewriterStatusCounts();

  scoped_ptr<LoggingInfo> logging_info_;

  // Thus must be set. Implementation constructors must minimally default this
  // to a NullMutex.
  scoped_ptr<AbstractMutex> mutex_;

  // The maximum number of rewrite info logs stored for a single request.
  int rewriter_info_max_size_;

  // Allow urls to be logged.
  bool allow_logging_urls_;

  // Allow url indices to be logged.
  bool log_url_indices_;

  // Map which maintains the url to index for logging urls.
  StringIntMap url_index_map_;

  // Stats collected from calls to LogRewrite.
  struct RewriterStatsInternal {
    RewriterStats::RewriterHtmlStatus html_status;

    // RewriteInfo::RewriterApplicationStatus -> count.
    std::map<int, int> status_counts;

    RewriterStatsInternal() : html_status(RewriterStats::UNKNOWN_STATUS) {}
  };
  typedef std::map<GoogleString, RewriterStatsInternal> RewriterStatsMap;
  RewriterStatsMap rewriter_stats_;

  DISALLOW_COPY_AND_ASSIGN(LogRecord);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_LOG_RECORD_H_
