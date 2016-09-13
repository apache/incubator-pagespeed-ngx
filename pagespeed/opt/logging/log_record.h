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

#ifndef PAGESPEED_OPT_LOGGING_LOG_RECORD_H_
#define PAGESPEED_OPT_LOGGING_LOG_RECORD_H_

#include <map>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest_prod.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/image_types.pb.h"
#include "pagespeed/opt/logging/enums.pb.h"
#include "pagespeed/opt/logging/logging_proto.h"
#include "pagespeed/opt/logging/logging_proto_impl.h"

// TODO(gee):  Hmm, this sort of sucks.
// If your .cc file needs to use the types declared in logging_proto.h,
// you must also include pagespeed/opt/logging/logging_proto_impl.h
// See that header file for an explanation of why this is necessary.

namespace net_instaweb {

class AbstractMutex;
class RequestTimingInfo;

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
// access to a LoggingInfo instance, however.
class AbstractLogRecord  {
 public:
  // Construct a AbstractLogRecord with a new LoggingInfo proto and caller-
  // supplied mutex. This class takes ownership of the mutex.
  explicit AbstractLogRecord(AbstractMutex* mutex);
  virtual ~AbstractLogRecord();

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
      const char* rewriter_id, RewriterApplication::Status status);

  // Creates a new rewriter logging submessage for |rewriter_id|,
  // sets status and the url index.
  void SetRewriterLoggingStatus(
      const char* rewriter_id, const GoogleString& url,
      RewriterApplication::Status status) {
    SetRewriterLoggingStatusHelper(rewriter_id, url, status);
  }

  // Log the HTML level status for a filter.  This should be called only once
  // per filter, at the point where it is determined the filter is either
  // active or not.
  void LogRewriterHtmlStatus(const char* rewriter_id,
                             RewriterHtmlApplication::Status status);

  // Log the status of a rewriter application on a resource.
  // TODO(gee): I'd really prefer rewriter_id was an enum.
  void LogRewriterApplicationStatus(
      const char* rewriter_id, RewriterApplication::Status status);

  // TODO(gee): Deprecate raw access to proto.
  // Return the LoggingInfo proto wrapped by this class. Calling code must
  // guard any reads and writes to this using mutex().
  virtual LoggingInfo* logging_info() = 0;

  // TODO(huibao): Rename LogImageBackgroundRewriteActivity() to make it clear
  // that it will log even when the rewriting finishes in the line-of-request.

  // Log image rewriting activity, which may not finish when the request
  // processing is done. The outcome is a new log record with request type
  // set to "BACKGROUND_REWRITE".
  void LogImageBackgroundRewriteActivity(
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
      int resized_height);

  // Atomically sets is_html_response in the logging proto.
  void SetIsHtml(bool is_html);

  // Updated the cohort info to set the found to true for the given
  // property.
  virtual void AddFoundPropertyToCohortInfo(
      int page_type, const GoogleString& cohort,
      const GoogleString& property) = 0;

  // Updated the cohort info to set the retrieved to true for the given
  // property.
  virtual void AddRetrievedPropertyToCohortInfo(
      int page_type, const GoogleString& cohort,
      const GoogleString& property) = 0;

  // Updates the cohort info to update the cache key state.
  virtual void SetCacheStatusForCohortInfo(
      int page_type, const GoogleString& cohort,
      bool found, int key_state) = 0;

  // Mutex-guarded log mutation convenience methods. The rule of thumb is that
  // if a single-field update to a logging proto occurs multiple times, it
  // should be factored out into a method on this class.
  void SetIsOriginalResourceCacheable(bool cacheable);

  // Log a RewriterInfo for the image rewrite filter.
  virtual void LogImageRewriteActivity(
      const char* id,
      const GoogleString& url,
      RewriterApplication::Status status,
      bool is_image_inlined,
      bool is_critical_image,
      bool is_url_rewritten,
      int size,
      bool try_low_res_src_insertion,
      bool low_res_src_inserted,
      ImageType low_res_image_type,
      int low_res_data_size) = 0;

  // TODO(gee): Change the callsites.
  void LogJsDisableFilter(const char* id, bool has_pagespeed_no_defer);

  void LogLazyloadFilter(const char* id,
                         RewriterApplication::Status status,
                         bool is_blacklisted, bool is_critical);

  // Mutex-guarded log-writing operations. Derived classes should override
  // *Impl methods. Returns false if the log write attempt failed.
  bool WriteLog();

  // Return the mutex associated with this instance. Calling code should
  // guard reads and writes of AbstractLogRecords
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
  virtual void SetImageStats(int num_img_tags, int num_inlined_img_tags,
                             int num_critical_images_used) = 0;

  // Sets the number of external resources on an HTML page.
  virtual void SetResourceCounts(int num_external_css, int num_scripts) = 0;

  // Sets critical CSS related byte counts (all uncompressed).
  void SetCriticalCssInfo(int critical_inlined_bytes,
                          int original_external_bytes,
                          int overhead_bytes);

  // Log information related to the user agent and device making the request.
  virtual void LogDeviceInfo(
      int device_type,
      bool supports_image_inlining,
      bool supports_lazyload_images,
      bool supports_critical_images_beacon,
      bool supports_deferjs,
      bool supports_webp_in_place,
      bool supports_webp_rewritten_urls,
      bool supports_webplossless_alpha,
      bool is_bot) = 0;

  // Log whether the request is an XmlHttpRequest.
  void LogIsXhr(bool is_xhr);

  // Sets initial information for background rewrite log.
  virtual void SetBackgroundRewriteInfo(
    bool log_urls,
    bool log_url_indices,
    int max_rewrite_info_log_size);

  // Set timing information in the logging implementation.
  virtual void SetTimingInfo(const RequestTimingInfo& timing_info) {}

 protected:
  // Implements writing a log, base implementation is a no-op. Returns false if
  // writing failed.
  virtual bool WriteLogImpl() = 0;

  // Helper function which creates a new rewriter logging submessage for
  // |rewriter_id|, sets status and the url index. It is intended to be called
  // only inside logging code.
  RewriterInfo* SetRewriterLoggingStatusHelper(
      const char* rewriter_id, const GoogleString& url,
      RewriterApplication::Status status);

 private:
  // Called on construction.
  void InitLogging();

  void PopulateUrl(
      const GoogleString& url, RewriteResourceInfo* rewrite_resource_info);

  // Fill LoggingInfo proto with information collected from LogRewriterStatus
  // and LogRewrite.
  void PopulateRewriterStatusCounts();

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
  typedef std::map<RewriterApplication::Status, int> RewriteStatusCountMap;
  struct RewriterStatsInternal {
    RewriterHtmlApplication::Status html_status;

    // RewriterApplication::Status -> count.
    RewriteStatusCountMap status_counts;

    RewriterStatsInternal()
        : html_status(RewriterHtmlApplication::UNKNOWN_STATUS) {}
  };
  typedef std::map<GoogleString, RewriterStatsInternal> RewriterStatsMap;
  RewriterStatsMap rewriter_stats_;

  DISALLOW_COPY_AND_ASSIGN(AbstractLogRecord);
};

// Simple AbstractLogRecord implementation which owns a LoggingInfo protobuf.
class LogRecord : public AbstractLogRecord {
 public:
  explicit LogRecord(AbstractMutex* mutex);

  virtual ~LogRecord();

  LoggingInfo* logging_info() { return logging_info_.get(); }

  virtual void SetImageStats(int num_img_tags, int num_inlined_img_tags,
                             int num_critical_images_used) {}

  virtual void SetResourceCounts(int num_external_css, int num_scripts) {}

  virtual void AddFoundPropertyToCohortInfo(
      int page_type, const GoogleString& cohort,
      const GoogleString& property) {}

  virtual void AddRetrievedPropertyToCohortInfo(
      int page_type, const GoogleString& cohort,
      const GoogleString& property) {}

  void SetCacheStatusForCohortInfo(
      int page_type, const GoogleString& cohort, bool found, int key_state) {}

  virtual void LogImageRewriteActivity(
      const char* id,
      const GoogleString& url,
      RewriterApplication::Status status,
      bool is_image_inlined,
      bool is_critical_image,
      bool is_url_rewritten,
      int size,
      bool try_low_res_src_insertion,
      bool low_res_src_inserted,
      ImageType low_res_image_type,
      int low_res_data_size) {}

  void LogDeviceInfo(
      int device_type,
      bool supports_image_inlining,
      bool supports_lazyload_images,
      bool supports_critical_images_beacon,
      bool supports_deferjs,
      bool supports_webp_in_place,
      bool supports_webp_rewritten_urls,
      bool supports_webplossless_alpha,
      bool is_bot) override {}

  bool WriteLogImpl() override { return true; }

 private:
  scoped_ptr<LoggingInfo> logging_info_;
};

// TODO(gee): I'm pretty sure the functionality can be provided by the previous
// ALR implementation, but for the time being leave this around to make the
// refactoring as limited as possible.
// AbstractLogRecord that copies logging_info() when in WriteLog.  This should
// be useful for testing any logging flow where an owned subordinate log record
// is needed.
class CopyOnWriteLogRecord : public LogRecord {
 public:
  CopyOnWriteLogRecord(AbstractMutex* logging_mutex, LoggingInfo* logging_info)
      : LogRecord(logging_mutex), logging_info_copy_(logging_info) {}

 protected:
  virtual bool WriteLogImpl() {
    logging_info_copy_->CopyFrom(*logging_info());
    return true;
  }

 private:
  LoggingInfo* logging_info_copy_;  // Not owned by us.

  DISALLOW_COPY_AND_ASSIGN(CopyOnWriteLogRecord);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_OPT_LOGGING_LOG_RECORD_H_
