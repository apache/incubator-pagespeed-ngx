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

#include "net/instaweb/util/public/shared_mem_statistics.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/file_writer.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/null_mutex.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/writer.h"

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
// /mod_pagespeed_statistics: variable_names, Histogram Names.
// IMPORTANT: Do not include kTimestampVariable here, or else DumpToWriter
// will hang.
const char* const kImportant[] = {
  "cache_hits", "cache_misses", "image_rewrites",
  "instrumentation_filter_script_added_count",
  "num_fallback_responses_served", "num_flushes",  "slurp_404_count",
  "Html Time us Histogram"
};
}  // namespace

namespace net_instaweb {

// Our shared memory storage format is an array of (mutex, int64).
SharedMemVariable::SharedMemVariable(const StringPiece& name)
    : name_(name.as_string()),
      value_ptr_(NULL),
      logger_(NULL) {
}

int64 SharedMemVariable::Get64() const {
  if (mutex_.get() != NULL) {
    ScopedMutex hold_lock(mutex_.get());
    return *value_ptr_;
  } else {
    return -1;
  }
}

int64 SharedMemVariable::Get64LockHeld() const {
  return *value_ptr_;
}

int SharedMemVariable::Get() const {
  return Get64();
}

void SharedMemVariable::Set(int new_value) {
  if (mutex_.get() != NULL) {
    {
      ScopedMutex hold_lock(mutex_.get());
      *value_ptr_ = new_value;
    }
    // The variable was changed, so dump statistics if past the update interval.
    if (logger_ != NULL) {
      logger_->UpdateAndDumpIfRequired();
    }
  }
}

void SharedMemVariable::SetLockHeldNoUpdate(int64 new_value) {
  *value_ptr_ = new_value;
}

void SharedMemVariable::SetConsoleStatisticsLogger(
    ConsoleStatisticsLogger* logger) {
  logger_ = logger;
}

void SharedMemVariable::Add(int delta) {
  if (mutex_.get() != NULL) {
    {
      ScopedMutex hold_lock(mutex_.get());
      *value_ptr_ += delta;
    }
    // The variable was changed, so dump statistics if past the update interval.
    if (logger_ != NULL) {
      logger_->UpdateAndDumpIfRequired();
    }
  }
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

AbstractMutex* SharedMemVariable::mutex() {
  return mutex_.get();
}

SharedMemConsoleStatisticsLogger::SharedMemConsoleStatisticsLogger(
    const int64 update_interval_ms, const StringPiece& log_file,
    SharedMemVariable* var, MessageHandler* message_handler,
    Statistics* stats, FileSystem* file_system, Timer* timer)
      : last_dump_timestamp_(var),
        message_handler_(message_handler),
        statistics_(stats),
        file_system_(file_system),
        timer_(timer),
        update_interval_ms_(update_interval_ms) {
  log_file.CopyToString(&logfile_name_);
}

SharedMemConsoleStatisticsLogger::~SharedMemConsoleStatisticsLogger() {
}

void SharedMemConsoleStatisticsLogger::UpdateAndDumpIfRequired() {
  int64 current_time_ms = timer_->NowMs();
  AbstractMutex* mutex = last_dump_timestamp_->mutex();
  if (mutex == NULL) {
    return;
  }
  // Avoid blocking if the dump is already happening in another thread/process.
  if (mutex->TryLock()) {
    if (current_time_ms >=
        (last_dump_timestamp_->Get64LockHeld() + update_interval_ms_)) {
      // It's possible we'll need to do some of the following here for
      // cross-process consistency:
      // - flush the logfile before unlock to force out buffered data
      FileSystem::OutputFile* statistics_log_file =
          file_system_->OpenOutputFileForAppend(
              logfile_name_.c_str(), message_handler_);
      if (statistics_log_file != NULL) {
        FileWriter statistics_writer(statistics_log_file);
        statistics_->DumpConsoleVarsToWriter(
            current_time_ms, &statistics_writer, message_handler_);
        statistics_writer.Flush(message_handler_);
        file_system_->Close(statistics_log_file, message_handler_);
      } else {
        message_handler_->Message(kError,
                                  "Error opening statistics log file %s.",
                                  logfile_name_.c_str());
      }
      // Update timestamp regardless of file write so we don't hit the same
      // error many times in a row.
      last_dump_timestamp_->SetLockHeldNoUpdate(current_time_ms);
    }
    mutex->Unlock();
  }
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
    int64 logging_interval_ms, const StringPiece& logging_file, bool logging,
    const GoogleString& filename_prefix, AbstractSharedMem* shm_runtime,
    MessageHandler* message_handler, FileSystem* file_system, Timer* timer)
    : shm_runtime_(shm_runtime), filename_prefix_(filename_prefix),
      frozen_(false), logger_(NULL) {
  if (logging) {
    if (logging_file.size() > 0) {
      // Variables account for the possibility that the Logger is NULL.
      // Only 1 Statistics object per process, so this shouldn't be too slow.
      for (int i = 0, n = arraysize(kImportant); i < n; ++i) {
        important_variables_.insert(kImportant[i]);
      }
      SharedMemVariable* timestampVar = AddVariable(kTimestampVariable);
      logger_.reset(new SharedMemConsoleStatisticsLogger(
          logging_interval_ms, logging_file, timestampVar,
          message_handler, this, file_system, timer));
      // The Logger needs a Variable which needs a Logger, hence the setter.
      timestampVar->SetConsoleStatisticsLogger(logger_.get());
      logger_->UpdateAndDumpIfRequired();
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
    var->SetConsoleStatisticsLogger(logger_.get());
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
  size_t per_var = shm_runtime_->SharedMutexSize() +
                   sizeof(int64);  // NOLINT(runtime/sizeof)
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
    GoogleString var_as_str = Integer64ToString(var->Get64());
    writer->Write(StringPrintf("%s: %s\n", var_name.c_str(),
        var_as_str.c_str()), message_handler);
  }

  for (int i = 0, n = histograms_size(); i < n; ++i) {
    Histogram* histogram = histograms(i);
    GoogleString histogram_name = histogram_names(i);
    if (IsIgnoredVariable(histogram_name)) {
      continue;
    }
    writer->Write(StringPrintf("histogram: %s\n",
        histogram_name.c_str()), message_handler);

    for (int j = 0, n = histogram->NumBuckets(); j < n; j++) {
      double lower_bound = histogram->BucketStart(j);
      double upper_bound = histogram->BucketLimit(j);
      double value = histogram->BucketCount(j);
      if (value == 0) {
        continue;
      }
      GoogleString result = StringPrintf("[%f, %f): %f\n",
          lower_bound, upper_bound, value);
      writer->Write(result, message_handler);
    }
  }
  writer->Flush(message_handler);
}

}  // namespace net_instaweb
