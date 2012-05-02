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
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/null_mutex.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

// Default max number of buckets for histogram, refers to stats/histogram.
const int kMaxBuckets = 500;
// Default upper bound of values in histogram. Can be reset by SetMaxValue().
const double kMaxValue = 5000;
const char kStatisticsObjName[] = "statistics";
}  // namespace

namespace net_instaweb {

// Our shared memory storage format is an array of (mutex, int64).
SharedMemVariable::SharedMemVariable(const StringPiece& name)
    : name_(name.as_string()),
      value_ptr_(NULL) {
}

int64 SharedMemVariable::Get64() const {
  if (mutex_.get() != NULL) {
    ScopedMutex hold_lock(mutex_.get());
    return *value_ptr_;
  } else {
    return -1;
  }
}

int SharedMemVariable::Get() const {
  return Get64();
}

void SharedMemVariable::Set(int new_value) {
  if (mutex_.get() != NULL) {
    ScopedMutex hold_lock(mutex_.get());
    *value_ptr_ = new_value;
  }
}

void SharedMemVariable::Add(int delta) {
  if (mutex_.get() != NULL) {
    ScopedMutex hold_lock(mutex_.get());
    *value_ptr_ += delta;
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

SharedMemHistogram::SharedMemHistogram() : max_buckets_(kMaxBuckets),
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
  buffer_->min_ = 0;
  buffer_->max_ = 0;
  buffer_->count_ = 0;
  buffer_->sum_ = 0;
  buffer_->sum_of_squares_ = 0;
  for (int i = 0; i < max_buckets_; ++i) {
    buffer_->values_[i] = 0;
  }
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
  if (buffer_->enable_negative_) {
    if (value > 0) {
      // When value > 0 and bucket_->max_value_ = +Inf,
      // value - (-bucket_->max_value) will cause overflow.
      int index_zero = FindBucket(0);
      double lower_bound = BucketStart(index_zero);
      double diff = value - lower_bound;
      return index_zero + diff / BucketWidth();
    } else {
      return (value - (-buffer_->max_value_)) / BucketWidth();
    }
  } else {
    return (value - buffer_->min_value_) / BucketWidth();
  }
}

void SharedMemHistogram::Add(double value) {
  if (buffer_ == NULL) {
    return;
  }
  ScopedMutex hold_lock(mutex_.get());
  if (buffer_->enable_negative_) {
    // If negative buckets is enabled, the minimum value allowed in Histogram
    // is -buffer_->max_value_;
    // The default min_value_ is 0, it's fine to add 0 to histogram.
    // But the |value| should be smaller than max_value_.
    // When |value| == max_value_, the return
    // value of FindBuckets() is max_buckets, which is out of boundary.
    if (value <= -buffer_->max_value_ ||
        value >= buffer_->max_value_ ) {
      return;
    }
  } else {
    if (value < buffer_->min_value_ || value >= buffer_->max_value_) {
      return;
    }
  }
  int index = FindBucket(value);
  if (index < 0 || index >= max_buckets_) {
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
  for (int i = 0; i < max_buckets_; ++i) {
    buffer_->values_[i] = 0;
  }
}

int SharedMemHistogram::MaxBuckets() {
  return max_buckets_;
}

void SharedMemHistogram::EnableNegativeBuckets() {
  if (buffer_ == NULL) {
    return;
  }
  DCHECK_EQ(0, buffer_->min_value_) << "Cannot call EnableNegativeBuckets and"
                                        "SetMinValue on the same histogram.";

  ScopedMutex hold_lock(mutex_.get());
  buffer_->enable_negative_ = true;
  ClearInternal();
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
  buffer_->min_value_ = value;
  ClearInternal();
}

void SharedMemHistogram::SetMaxValue(double value) {
  if (buffer_ == NULL) {
    return;
  }
  DCHECK_LT(0, value) << "Upper-bound of a histogram should be larger than 0.";
  ScopedMutex hold_lock(mutex_.get());
  buffer_->max_value_ = value;
  ClearInternal();
}

void SharedMemHistogram::SetMaxBuckets(int i) {
  DCHECK_GT(i, 0) << "Maximum number of buckets should be larger than 0";
  max_buckets_ = i;
}

double SharedMemHistogram::AverageInternal() {
  if (buffer_ == NULL) {
    return -1.0;
  }
  if (buffer_->count_ == 0) return 0.0;
  return buffer_->sum_ / buffer_->count_;
}

// Return estimated value that is larger than perc% of all data.
// e.g. Percentile(50) is the median. Percentile(99) is the value larger than
// 99% of the data.
double SharedMemHistogram::PercentileInternal(const double perc) {
  if (buffer_ == NULL) {
    return -1.0;
  }
  if (buffer_->count_ == 0 || perc < 0) return 0.0;
  // Floor of count_below is the number of values below the percentile.
  // We are indeed looking for the next value in histogram.
  double count_below = floor(buffer_->count_ * perc / 100);
  double count = 0;
  int i;
  // Find the bucket which is closest to the bucket that contains
  // the number we want.
  for (i = 0; i < max_buckets_; ++i) {
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
  if (buffer_->count_ == 0) return 0.0;
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
  DCHECK(index >= 0 && index <= max_buckets_) <<
      "Queried index is out of boundary.";
  if (index == max_buckets_) {
    // BucketLimit(i) = BucketStart(i+1).
    // Bucket index goes from 0 to max_buckets -1.
    // BuketLimit(max_buckets - 1) = BucketStart(max_buckets).
    // In this case, we return the upper_bound of the Histogram.
    return buffer_->max_value_;
  }
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

  if (index < 0 || index >= max_buckets_) {
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
    bucket_width = max * 2 / max_buckets_;
  } else {
    bucket_width = (max - min) / max_buckets_;
  }
  DCHECK_NE(0, bucket_width);
  return bucket_width;
}

SharedMemStatistics::SharedMemStatistics(AbstractSharedMem* shm_runtime,
                                         const GoogleString& filename_prefix)
    : shm_runtime_(shm_runtime), filename_prefix_(filename_prefix),
      frozen_(false) {
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
    return new SharedMemVariable(name);
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
    pos += shm_runtime_->SharedMutexSize() + hist->AllocationSize();
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
    total += shm_runtime_->SharedMutexSize() + hist->AllocationSize();
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
      if (parent) hist->Init();
    } else {
      hist->Reset();
    }
    pos += shm_runtime_->SharedMutexSize() + hist->AllocationSize();
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

}  // namespace net_instaweb
