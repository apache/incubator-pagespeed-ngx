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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_STATISTICS_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_STATISTICS_H_

#include <cstddef>
#include <set>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/statistics_template.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractSharedMem;
class AbstractSharedMemSegment;
class FileSystem;
class MessageHandler;
class Timer;
class Writer;

// An implementation of Statistics using our shared memory infrastructure.
// These statistics will be shared amongst all processes and threads
// spawned by our host.  Note that we will be obtaining a per-variable mutex for
// every read and write to these variables.  Since this may be expensive,
// we may need each thread to keep a local cache and infrequently write
// through to this Statistics object.  TODO(abliss): actually do this.
//
// Because we must allocate shared memory segments and mutexes before any child
// processes and threads are created, all AddVariable calls must be done in
// the host before it starts forking/threading. Once all variables are added,
// you must call InitVariables(true), and then InitVariables(false) in every
// kid.
//
// If a variable fails to initialize (due to either its mutex or the shared
// memory segment not working), it will not increment in that process (and a
// warning message will be logged).  If the variable fails to initialize in the
// process that happens to serve a statistics page, then the variable will show
// up with value -1.
class SharedMemVariable : public Variable {
 public:
  virtual ~SharedMemVariable() {}
  int64 Get64() const;
  virtual int Get() const;
  virtual void Set(int newValue);
  virtual void Add(int delta);
  virtual StringPiece GetName() const { return name_; }
  AbstractMutex* mutex();

 private:
  friend class SharedMemConsoleStatisticsLogger;
  friend class SharedMemStatistics;
  friend class SharedMemTimedVariable;

  explicit SharedMemVariable(const StringPiece& name);

  void AttachTo(AbstractSharedMemSegment* segment_, size_t offset,
                MessageHandler* message_handler);

  // Called on initialization failure, to make sure it's clear if we
  // share some state with parent.
  void Reset();

  void SetConsoleStatisticsLogger(ConsoleStatisticsLogger* logger);

  // Set the variable assuming that the lock is already held. Also, doesn't call
  // ConsoleStatisticsLogger::UpdateAndDumpIfRequired. (This method is intended
  // for use from within ConsoleStatisticsLogger::UpdateAndDumpIfRequired, so
  // the lock is already held and updating again would introduce a loop.)
  void SetLockHeldNoUpdate(int64 newValue);

  // Get the variable's value assuming that the lock is already held.
  int64 Get64LockHeld() const;

  // The name of this variable.
  const GoogleString name_;

  // Lock protecting us. NULL if for some reason initialization failed.
  scoped_ptr<AbstractMutex> mutex_;

  // The data...
  volatile int64* value_ptr_;

  // The object used to log updates to a file. Owned by Statistics object, with
  // a copy shared with every Variable. Note that this may be NULL if
  // SetConsoleStatisticsLogger has not yet been called.
  ConsoleStatisticsLogger* logger_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemVariable);
};

class SharedMemConsoleStatisticsLogger : public ConsoleStatisticsLogger {
 public:
  SharedMemConsoleStatisticsLogger(
      const int64 update_interval_ms, const StringPiece& log_file,
      SharedMemVariable* var, MessageHandler* message_handler,
      Statistics* stats, FileSystem *file_system, Timer* timer);
  virtual ~SharedMemConsoleStatisticsLogger();
  virtual void UpdateAndDumpIfRequired();

 private:
  // The last_dump_timestamp not only contains the time of the last dump,
  // it also controls locking so that multiple threads can't dump at once.
  SharedMemVariable* last_dump_timestamp_;
  MessageHandler* message_handler_;
  Statistics* statistics_;  // Needed so we can dump the stats contained here.
  // file_system_ and timer_ are owned by someone who called the constructor
  // (usually Apache's ResourceManager).
  FileSystem* file_system_;
  Timer* timer_;    // Used to retrieve timestamps
  const int64 update_interval_ms_;
  GoogleString logfile_name_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemConsoleStatisticsLogger);
};

class SharedMemHistogram : public Histogram {
 public:
  virtual ~SharedMemHistogram();
  virtual void Add(double value);
  virtual void Clear();
  virtual int MaxBuckets();
  // Call the following functions after statistics->Init and before add values.
  // EnableNegativeBuckets, SetMinValue and SetMaxValue will
  // cause resetting Histogram.
  virtual void EnableNegativeBuckets();
  // Set the minimum value allowed in histogram.
  virtual void SetMinValue(double value);
  // Set the upper-bound of value in histogram,
  // The value range in histogram is [MinValue, MaxValue) or
  // (-MaxValue, MaxValue) if negative buckets are enabled.
  virtual void SetMaxValue(double value);
  // We rely on MaxBuckets to allocate memory segment for histogram. If we want
  // to call SetMaxBuckets(), we should call it right after AddHistogram().
  virtual void SetMaxBuckets(int i);
  // Return the allocation size for this Histogram object except Mutex size.
  // Shared memory space should include a mutex, HistogramBody and
  // sizeof(double) * MaxBuckets(). Here we do not know mutex size.
  size_t AllocationSize() {
    size_t total = sizeof(HistogramBody) + sizeof(double) * MaxBuckets();
    return total;
  }

 protected:
  virtual AbstractMutex* lock() {
    return mutex_.get();
  }
  virtual double AverageInternal();
  virtual double PercentileInternal(const double perc);
  virtual double StandardDeviationInternal();
  virtual double CountInternal();
  virtual double MaximumInternal();
  virtual double MinimumInternal();
  virtual double BucketStart(int index);
  virtual double BucketCount(int index);

 private:
  friend class SharedMemStatistics;
  SharedMemHistogram();
  void AttachTo(AbstractSharedMemSegment* segment, size_t offset,
                MessageHandler* message_handler);
  double BucketWidth();
  int FindBucket(double value);
  void Init();
  void Reset();
  void ClearInternal();  // expects mutex_ held, buffer_ != NULL
  const GoogleString name_;
  scoped_ptr<AbstractMutex> mutex_;
  // TODO(fangfei): implement a non-shared-mem histogram.
  struct HistogramBody {
    // Enable negative values in histogram, false by default.
    bool enable_negative_;
    // Minimum value allowed in Histogram, 0 by default.
    double min_value_;
    // Maximum value allowed in Histogram,
    // numeric_limits<double>::max() by default.
    double max_value_;
    // Real minimum value.
    double min_;
    // Real maximum value.
    double max_;
    double count_;
    double sum_;
    double sum_of_squares_;
    // Histogram buckets data.
    double values_[1];
  };
  // Maximum number of buckets in Histogram.
  int max_buckets_;
  HistogramBody* buffer_;  // may be NULL if init failed.
  DISALLOW_COPY_AND_ASSIGN(SharedMemHistogram);
};

class SharedMemStatistics : public StatisticsTemplate<SharedMemVariable,
    SharedMemHistogram, FakeTimedVariable> {
 public:
  SharedMemStatistics(int64 logging_interval_ms,
                      const StringPiece& logging_file, bool logging,
                      const GoogleString& filename_prefix,
                      AbstractSharedMem* shm_runtime,
                      MessageHandler* message_handler, FileSystem* file_system,
                      Timer* timer);
  virtual ~SharedMemStatistics();

  // This method initializes or attaches to shared memory. You should call this
  // exactly once in each process/thread, after all calls to AddVariables,
  // AddHistograms and SetMaxBuckets have been done.
  // The root process (the one that starts all the other child
  // threads and processes) must be the first one to make the call, with
  // parent = true, with all other calling it with = false.
  void Init(bool parent, MessageHandler* message_handler);

  // This should be called from the root process as it is about to exit, when
  // no further children are expected to start.
  void GlobalCleanup(MessageHandler* message_handler);

  ConsoleStatisticsLogger* console_logger() const { return logger_.get(); }

  virtual void DumpConsoleVarsToWriter(int64 current_time_ms, Writer* writer,
                                       MessageHandler* message_handler);
  // Return whether to ignore the variable with a given name as unneeded by the
  // console.
  bool IsIgnoredVariable(const GoogleString& var_name);

 protected:
  virtual SharedMemVariable* NewVariable(const StringPiece& name, int index);
  virtual SharedMemHistogram* NewHistogram(const StringPiece& name);
  virtual FakeTimedVariable* NewTimedVariable(const StringPiece& name,
                                              int index);

 private:
  GoogleString SegmentName() const;

  // Create mutexes in the segment, with per_var bytes being used,
  // counting the mutex, for each variable.
  bool InitMutexes(size_t per_var, MessageHandler* message_handler);

  AbstractSharedMem* shm_runtime_;
  GoogleString filename_prefix_;
  scoped_ptr<AbstractSharedMemSegment> segment_;
  bool frozen_;
  scoped_ptr<SharedMemConsoleStatisticsLogger> logger_;
  // The variables that we're interested in displaying on the console.
  std::set<GoogleString> important_variables_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemStatistics);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_STATISTICS_H_
