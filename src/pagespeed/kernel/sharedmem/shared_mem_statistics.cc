// Copyright 2010 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: morlovich@google.com (Maksim Orlovich)

#include "pagespeed/kernel/sharedmem/shared_mem_statistics.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/abstract_shared_mem.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics_logger.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/writer.h"

namespace net_instaweb {

namespace {

// Default number of buckets for histogram, refers to stats/histogram.
const int kDefaultNumBuckets = 500;

// We always allocate 2 extra buckets, one for values below the specified
// range, and one for values above.
const int kOutOfBoundsCatcherBuckets = 2;

// Default upper bound of values in histogram. Can be reset by SetMaxValue().
const double kMaxValue = 5000;
const char kStatisticsObjName[] = "statistics";

// Variable name for the timestamp used to decide whether we should dump
// statistics.
const char kTimestampVariable[] = "timestamp_";

// Variables to keep for the console. These are the same names used in
// /mod_pagespeed_statistics.
// TODO(sligocki): Move into statistics_logger.cc and rename to be more
// descriptive.
const char* const kImportant[] = {
  // Variables used in /pagespeed_console
  "serf_fetch_failure_count", "serf_fetch_request_count",
  "resource_url_domain_rejections", "resource_url_domain_acceptances",
  "num_cache_control_not_rewritable_resources",
  "num_cache_control_rewritable_resources",
  "cache_backend_misses", "cache_backend_hits", "cache_expirations",
  "css_filter_parse_failures", "css_filter_blocks_rewritten",
  "javascript_minification_failures", "javascript_blocks_minified",
  "image_rewrites", "image_rewrites_dropped_nosaving_resize",
  "image_rewrites_dropped_nosaving_noresize",
  "image_norewrites_high_resolution",
  "image_rewrites_dropped_decode_failure"
  "image_rewrites_dropped_server_write_fail"
  "image_rewrites_dropped_mime_type_unknown"
  "image_norewrites_high_resolution",
  "css_combine_opportunities", "css_file_count_reduction",

  // Variables used by /mod_pagespeed_temp_statistics_graphs
  // Note: It's fine that there are duplicates here.
  // TODO(sligocki): Remove this in favor of the /pagespeed_console vars.
  // Should we also add other stats for future/other use?
  "num_flushes", "cache_hits", "cache_misses",
  "num_fallback_responses_served", "slurp_404_count", "page_load_count",
  "total_page_load_ms", "num_rewrites_executed", "num_rewrites_dropped",
  "resource_404_count", "serf_fetch_request_count",
  "serf_fetch_bytes_count", "image_ongoing_rewrites",
  "javascript_total_bytes_saved", "css_filter_total_bytes_saved",
  "image_rewrite_total_bytes_saved", "image_norewrites_high_resolution",
  "image_rewrites_dropped_due_to_load", "image_rewrites_dropped_intentionally",
  "memcached_get_count", "memcached_hit_latency_us",
  "memcached_insert_latency_us", "memcached_insert_size_bytes",
  "memcached_lookup_size_bytes", "memcached_hits", "memcached_misses",
  "flatten_imports_charset_mismatch", "flatten_imports_invalid_url",
  "flatten_imports_limit_exceeded", "flatten_imports_minify_failed",
  "flatten_imports_recursion", "css_filter_parse_failures",
  "converted_meta_tags", "javascript_minification_failures"
};

}  // namespace

// Our shared memory storage format is an array of (mutex, int64).
SharedMemVariable::SharedMemVariable(const StringPiece& name)
    : name_(name.as_string()),
      value_ptr_(NULL) {
}

int64 SharedMemVariable::GetLockHeld() const {
  return *value_ptr_;
}

int64 SharedMemVariable::SetReturningPreviousValueLockHeld(int64 new_value) {
  int64 previous_value = *value_ptr_;
  *value_ptr_ = new_value;
  return previous_value;
}

void SharedMemVariable::AttachTo(
    AbstractSharedMemSegment* segment, size_t offset,
    MessageHandler* message_handler) {
  mutex_.reset(segment->AttachToSharedMutex(offset));
  if (mutex_.get() == NULL) {
    message_handler->Message(
        kError, "Unable to attach to mutex for statistics variable %s",
        name_.c_str());
  }

  value_ptr_ = reinterpret_cast<volatile int64*>(
      segment->Base() + offset + segment->SharedMutexSize());
}

void SharedMemVariable::Reset() {
  mutex_.reset();
}

AbstractMutex* SharedMemVariable::mutex() const {
  return mutex_.get();
}

SharedMemHistogram::SharedMemHistogram()
    : num_buckets_(kDefaultNumBuckets + kOutOfBoundsCatcherBuckets),
      buffer_(NULL) {
}

SharedMemHistogram::~SharedMemHistogram() {
}

void SharedMemHistogram::Init() {
  if (buffer_ == NULL) {
    return;
  }

  ScopedMutex hold_lock(mutex_.get());
  buffer_->enable_negative_ = false;
  buffer_->min_value_ = 0;
  buffer_->max_value_ = kMaxValue;
  ClearInternal();
}

void SharedMemHistogram::DCheckRanges() const {
  DCHECK_LT(buffer_->min_value_, buffer_->max_value_);
}

void SharedMemHistogram::AttachTo(
    AbstractSharedMemSegment* segment, size_t offset,
    MessageHandler* message_handler) {
  mutex_.reset(segment->AttachToSharedMutex(offset));
  if (mutex_.get() == NULL) {
    message_handler->Message(
        kError, "Unable to attach to mutex for statistics histogram");
    Reset();
    return;
  }
  buffer_ = reinterpret_cast<HistogramBody*>(const_cast<char*>(
      segment->Base() + offset + segment->SharedMutexSize()));
}

void SharedMemHistogram::Reset() {
  mutex_.reset(new NullMutex);
  buffer_ = NULL;
}

int SharedMemHistogram::FindBucket(double value) {
  DCHECK(buffer_ != NULL);
  // We add +1 in most of these case here to skip the leftmost catcher bucket.
  // (The one exception is when using index_zero, which already included the
  //  offset).
  if (buffer_->enable_negative_) {
    if (value > 0) {
      // When value > 0 and bucket_->max_value_ = +Inf,
      // value - (-bucket_->max_value) will cause overflow.
      int index_zero = FindBucket(0);
      double lower_bound = BucketStart(index_zero);
      double diff = value - lower_bound;
      return index_zero + diff / BucketWidth();
    } else {
      return 1 + (value - (-buffer_->max_value_)) / BucketWidth();
    }
  } else {
    return 1 + (value - buffer_->min_value_) / BucketWidth();
  }
}

void SharedMemHistogram::Add(double value) {
  if (buffer_ == NULL) {
    return;
  }
  ScopedMutex hold_lock(mutex_.get());
  // See if we should put the value in one of the out-of-bounds catcher buckets,
  // in which case we will change index from -1.
  int index = -1;
  if (buffer_->enable_negative_) {
    // If negative buckets is enabled, the minimum value in-range in Histogram
    // is -buffer_->max_value_.
    if (value < -buffer_->max_value_) {
      index = 0;
    } else if (value >= buffer_->max_value_) {
      index = num_buckets_ - 1;
    }
  } else {
    if (value < buffer_->min_value_) {
      index = 0;
    } else if (value >= buffer_->max_value_) {
      index = num_buckets_ - 1;
    }
  }

  if (index == -1) {
    // Not clearly edge buckets, so compute the value's position.
    index = FindBucket(value);
  }

  if (index < 0 || index >= num_buckets_) {
    LOG(ERROR) << "Invalid bucket index found for" << value;
    return;
  }
  buffer_->values_[index]++;
  // Update actual min & max values;
  if (buffer_->count_ == 0) {
    buffer_->min_ = value;
    buffer_->max_ = value;
  } else if (value < buffer_->min_) {
    buffer_->min_ = value;
  } else if (value > buffer_->max_) {
    buffer_->max_ = value;
  }
  buffer_->count_++;
  buffer_->sum_ += value;
  buffer_->sum_of_squares_ += value * value;
}

void SharedMemHistogram::Clear() {
  if (buffer_ == NULL) {
    return;
  }

  ScopedMutex hold_lock(mutex_.get());
  ClearInternal();
}

void SharedMemHistogram::ClearInternal() {
  // Throw away data.
  buffer_->min_ = 0;
  buffer_->max_ = 0;
  buffer_->count_ = 0;
  buffer_->sum_ = 0;
  buffer_->sum_of_squares_ = 0;
  for (int i = 0; i < num_buckets_; ++i) {
    buffer_->values_[i] = 0;
  }
}

int SharedMemHistogram::NumBuckets() {
  return num_buckets_;
}

void SharedMemHistogram::EnableNegativeBuckets() {
  if (buffer_ == NULL) {
    return;
  }
  DCHECK_EQ(0, buffer_->min_value_) << "Cannot call EnableNegativeBuckets and"
                                        "SetMinValue on the same histogram.";

  ScopedMutex hold_lock(mutex_.get());
  if (!buffer_->enable_negative_) {
    buffer_->enable_negative_ = true;
    ClearInternal();
  }
}

void SharedMemHistogram::SetMinValue(double value) {
  if (buffer_ == NULL) {
    return;
  }
  DCHECK_EQ(false, buffer_->enable_negative_) << "Cannot call"
      "EnableNegativeBuckets and SetMinValue on the same histogram.";
  DCHECK_LT(value, buffer_->max_value_) << "Lower-bound of a histogram "
      "should be smaller than its upper-bound.";

  ScopedMutex hold_lock(mutex_.get());
  if (buffer_->min_value_ != value) {
    buffer_->min_value_ = value;
    ClearInternal();
  }
}

void SharedMemHistogram::SetMaxValue(double value) {
  if (buffer_ == NULL) {
    return;
  }
  DCHECK_LT(0, value) << "Upper-bound of a histogram should be larger than 0.";
  DCHECK_LT(buffer_->min_value_, value) << "Upper-bound of a histogram should "
      "be larger than its lower-bound.";
  ScopedMutex hold_lock(mutex_.get());
  if (buffer_->max_value_ != value) {
    buffer_->max_value_ = value;
    ClearInternal();
  }
}

void SharedMemHistogram::SetSuggestedNumBuckets(int i) {
  DCHECK_GT(i, 0) << "Number of buckets should be larger than 0";
  num_buckets_ = i + kOutOfBoundsCatcherBuckets;
}

double SharedMemHistogram::AverageInternal() {
  if (buffer_ == NULL) {
    return -1.0;
  }
  if (buffer_->count_ == 0) {
    return 0.0;
  }
  return buffer_->sum_ / buffer_->count_;
}

// Return estimated value that is larger than perc% of all data.
// e.g. Percentile(50) is the median. Percentile(99) is the value larger than
// 99% of the data.
double SharedMemHistogram::PercentileInternal(const double perc) {
  if (buffer_ == NULL) {
    return -1.0;
  }
  if (buffer_->count_ == 0 || perc < 0) {
    return 0.0;
  }
  // Floor of count_below is the number of values below the percentile.
  // We are indeed looking for the next value in histogram.
  double count_below = floor(buffer_->count_ * perc / 100);
  double count = 0;
  int i;
  // Find the bucket which is closest to the bucket that contains
  // the number we want.
  for (i = 0; i < num_buckets_; ++i) {
    if (count + buffer_->values_[i] <= count_below) {
      count += buffer_->values_[i];
      if (count == count_below) {
        // The first number in (i+1)th bucket is the number we want. Its
        // estimated value is the lower-bound of (i+1)th bucket.
        return BucketStart(i+1);
      }
    } else {
      break;
    }
  }
  // The (count_below + 1 - count)th number in bucket i is the number we want.
  // However, we do not know its exact value as we do not have a trace of all
  // values.
  double fraction = (count_below + 1 - count) / BucketCount(i);
  double bound = std::min(BucketWidth(), buffer_->max_ - BucketStart(i));
  double ret = BucketStart(i) + fraction * bound;
  return ret;
}

double SharedMemHistogram::StandardDeviationInternal() {
  if (buffer_ == NULL) {
    return -1.0;
  }
  if (buffer_->count_ == 0) {
    return 0.0;
  }
  const double v = (buffer_->sum_of_squares_ * buffer_->count_ -
                   buffer_->sum_ * buffer_->sum_) /
                   (buffer_->count_ * buffer_->count_);
  if (v < buffer_->sum_of_squares_ * std::numeric_limits<double>::epsilon()) {
    return 0.0;
  }
  return std::sqrt(v);
}

double SharedMemHistogram::CountInternal() {
  if (buffer_ == NULL) {
    return -1.0;
  }
  return buffer_->count_;
}

double SharedMemHistogram::MaximumInternal() {
  if (buffer_ == NULL) {
    return -1.0;
  }
  return buffer_->max_;
}

double SharedMemHistogram::MinimumInternal() {
  if (buffer_ == NULL) {
    return -1.0;
  }
  return buffer_->min_;
}

double SharedMemHistogram::BucketStart(int index) {
  if (buffer_ == NULL) {
    return -1.0;
  }
  DCHECK(index >= 0 && index <= num_buckets_) <<
      "Queried index is out of boundary.";
  if (index == num_buckets_) {
    // BucketLimit(i) = BucketStart(i+1).
    // Bucket index goes from 0 to num_buckets -1.
    // BucketLimit(num_buckets - 1) = BucketStart(num_buckets),
    // and BucketLimit(num_buckets - 1) is +infinity as we make our
    // outermost buckets catch everything that would otherwise fall out
    // of range.
    return std::numeric_limits<double>::infinity();
  }
  if (index == 0) {
    return -std::numeric_limits<double>::infinity();
  }

  index -= 1;  // Skip over the left out-of-bounds catcher bucket.

  if (buffer_->enable_negative_) {
    // should not use (max - min) / buckets, in case max = + Inf.
    return (index * BucketWidth() + -buffer_->max_value_);
  }
  return (buffer_->min_value_ + index * BucketWidth());
}

double SharedMemHistogram::BucketCount(int index) {
  if (buffer_ == NULL) {
    return -1.0;
  }

  if (index < 0 || index >= num_buckets_) {
    return -1.0;
  }
  return buffer_->values_[index];
}

double SharedMemHistogram::BucketWidth() {
  if (buffer_ == NULL) {
    return -1.0;
  }
  double max = buffer_->max_value_;
  double min = buffer_->min_value_;
  double bucket_width = 0;

  if (buffer_->enable_negative_) {
    bucket_width = max * 2 / (num_buckets_ - kOutOfBoundsCatcherBuckets);
  } else {
    bucket_width = (max - min) / (num_buckets_ - kOutOfBoundsCatcherBuckets);
  }
  DCHECK_NE(0, bucket_width);
  return bucket_width;
}

SharedMemStatistics::SharedMemStatistics(
    int64 logging_interval_ms, int64 max_logfile_size_kb,
    const StringPiece& logging_file, bool logging,
    const GoogleString& filename_prefix, AbstractSharedMem* shm_runtime,
    MessageHandler* message_handler, FileSystem* file_system, Timer* timer)
    : shm_runtime_(shm_runtime), filename_prefix_(filename_prefix),
      frozen_(false) {
  if (logging) {
    if (logging_file.size() > 0) {
      // Variables account for the possibility that the Logger is NULL.
      // Only 1 Statistics object per process, so this shouldn't be too slow.
      for (int i = 0, n = arraysize(kImportant); i < n; ++i) {
        important_variables_.insert(kImportant[i]);
      }
      SharedMemVariable* timestamp_var = AddVariable(kTimestampVariable);
      console_logger_.reset(new StatisticsLogger(
          logging_interval_ms, max_logfile_size_kb, logging_file,
          timestamp_var, message_handler, this, file_system, timer));
    } else {
      message_handler->Message(kError,
          "Error: ModPagespeedStatisticsLoggingFile is required if "
          "ModPagespeedStatisticsLogging is enabled.");
    }
  }
}

SharedMemStatistics::~SharedMemStatistics() {
}

SharedMemVariable* SharedMemStatistics::NewVariable(const StringPiece& name,
                                                    int index) {
  if (frozen_) {
    LOG(ERROR) << "Cannot add variable " << name
               << " after SharedMemStatistics is frozen!";
    return NULL;
  } else {
    SharedMemVariable* var = new SharedMemVariable(name);
    return var;
  }
}

SharedMemHistogram* SharedMemStatistics::NewHistogram(const StringPiece& name) {
  if (frozen_) {
    LOG(ERROR) << "Cannot add histogram after SharedMemStatistics is frozen!";
    return NULL;
  } else {
    return new SharedMemHistogram();
  }
}

FakeTimedVariable* SharedMemStatistics::NewTimedVariable(
    const StringPiece& name, int index) {
  return NewFakeTimedVariable(name, index);
}

bool SharedMemStatistics::InitMutexes(size_t per_var,
                                      MessageHandler* message_handler) {
  for (size_t i = 0; i < variables_size(); ++i) {
    SharedMemVariable* var = variables(i);
    if (!segment_->InitializeSharedMutex(i * per_var, message_handler)) {
      message_handler->Message(
          kError, "Unable to create mutex for statistics variable %s",
          var->name_.c_str());
      return false;
    }
  }
  size_t pos = variables_size() * per_var;
  for (size_t i = 0; i < histograms_size();) {
    if (!segment_->InitializeSharedMutex(pos, message_handler)) {
      message_handler->Message(
          kError, "Unable to create mutex for statistics histogram %s",
          histogram_names(i).c_str());
      return false;
    }
    SharedMemHistogram* hist = histograms(i);
    pos += hist->AllocationSize(shm_runtime_);
    i++;
  }
  return true;
}

void SharedMemStatistics::Init(bool parent,
                               MessageHandler* message_handler) {
  frozen_ = true;

  // Compute size of shared memory
  size_t per_var = shm_runtime_->SharedMutexSize() + sizeof(int64);
  size_t total = variables_size() * per_var;
  for (size_t i = 0; i < histograms_size(); ++i) {
    SharedMemHistogram* hist = histograms(i);
    total += hist->AllocationSize(shm_runtime_);
  }
  bool ok = true;
  if (parent) {
    // In root process -> initialize shared memory.
    segment_.reset(
        shm_runtime_->CreateSegment(SegmentName(), total, message_handler));
    ok = (segment_.get() != NULL);

    // Init the locks
    if (ok) {
      if (!InitMutexes(per_var, message_handler)) {
        // We had a segment but could not make some mutex. In this case,
        // we can't predict what would happen if the child process tried
        // to touch messed up mutexes. Accordingly, we blow away the
        // segment.
        segment_.reset(NULL);
        shm_runtime_->DestroySegment(SegmentName(), message_handler);
      }
    }
  } else {
    // Child -> attach to existing segment
    segment_.reset(
        shm_runtime_->AttachToSegment(SegmentName(), total, message_handler));
    ok = (segment_.get() != NULL);
  }

  if (!ok) {
    message_handler->Message(
        kWarning, "Problem during shared memory setup; "
                  "statistics functionality unavailable.");
  }

  // Now make the variable objects actually point to the right things.
  for (size_t i = 0; i < variables_size(); ++i) {
    if (ok) {
      variables(i)->AttachTo(segment_.get(), i * per_var, message_handler);
    } else {
      variables(i)->Reset();
    }
  }
  // Initialize Histogram buffers.
  size_t pos = variables_size() * per_var;
  for (size_t i = 0; i < histograms_size();) {
    SharedMemHistogram* hist = histograms(i);
    if (ok) {
      hist->AttachTo(segment_.get(), pos, message_handler);
      if (parent) {
        hist->Init();
      }
      // Either because they were just initialized or because this is a child
      // init and they were initialized in the parent, the histogram's min and
      // max should be set sensibly by this point.
      hist->DCheckRanges();
    } else {
      hist->Reset();
    }
    pos += hist->AllocationSize(shm_runtime_);
    i++;
  }
}

void SharedMemStatistics::GlobalCleanup(MessageHandler* message_handler) {
  if (segment_.get() != NULL) {
    shm_runtime_->DestroySegment(SegmentName(), message_handler);
  }
}

GoogleString SharedMemStatistics::SegmentName() const {
  return StrCat(filename_prefix_, kStatisticsObjName);
}

bool SharedMemStatistics::IsIgnoredVariable(const GoogleString& var_name) {
  return (important_variables_.find(var_name) == important_variables_.end());
}

void SharedMemStatistics::DumpConsoleVarsToWriter(
    int64 current_time_ms, Writer* writer, MessageHandler* message_handler) {
  writer->Write(StringPrintf("timestamp: %s\n",
      Integer64ToString(current_time_ms).c_str()), message_handler);

  for (int i = 0, n = variables_size(); i < n; ++i) {
    Variable* var = variables(i);
    GoogleString var_name = var->GetName().as_string();
    if (IsIgnoredVariable(var_name)) {
      continue;
    }
    GoogleString var_as_str = Integer64ToString(var->Get());
    writer->Write(StringPrintf("%s: %s\n", var_name.c_str(),
        var_as_str.c_str()), message_handler);
  }

  // Note: We used to dump histogram data as well, but that data is quite large
  // and we don't have a plan to use it in the console, so it was removed.

  writer->Flush(message_handler);
}

}  // namespace net_instaweb
