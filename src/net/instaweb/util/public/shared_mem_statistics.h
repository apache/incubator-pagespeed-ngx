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
#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/statistics_template.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/google_timer.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/gtest_prod.h"  // for FRIEND_TEST

namespace net_instaweb {

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

// Handles reading the logfile created by SharedMemConsoleStatisticsLogger.
class ConsoleStatisticsLogfileReader {
 public:
  ConsoleStatisticsLogfileReader(FileSystem::InputFile* file, int64 start_time,
                                 int64 end_time, int64 granularity_ms,
                                 MessageHandler* message_handler);
  ~ConsoleStatisticsLogfileReader();
  // Reads the next timestamp in the file to timestamp and the corresponding
  // chunk of data to data. Returns true if new data has been read.
  bool ReadNextDataBlock(int64* timestamp, GoogleString* data);
  int64 end_time() { return end_time_; }

 private:
  size_t BufferFind(const char* search_for, size_t start_at);
  int FeedBuffer();

  FileSystem::InputFile* file_;
  int64 start_time_;
  int64 end_time_;
  int64 granularity_ms_;
  MessageHandler* message_handler_;
  GoogleString buffer_;

  DISALLOW_COPY_AND_ASSIGN(ConsoleStatisticsLogfileReader);
};

class SharedMemConsoleStatisticsLogger : public ConsoleStatisticsLogger {
 public:
  SharedMemConsoleStatisticsLogger(
      const int64 update_interval_ms, const StringPiece& log_file,
      SharedMemVariable* var, MessageHandler* message_handler,
      Statistics* stats, FileSystem *file_system, Timer* timer);
  virtual ~SharedMemConsoleStatisticsLogger();
  virtual void UpdateAndDumpIfRequired();
  Timer* timer() { return timer_;}
  // Writes filtered variable and histogram data in JSON format to the given
  // writer. Variable data is a time series collected from with data points from
  // start_time to end_time, whereas histograms are aggregated histogram data as
  // of the given end_time. Granularity is the minimum time difference between
  // each successive data point.
  void DumpJSON(const std::set<GoogleString>& var_titles,
                const std::set<GoogleString>& hist_titles, int64 start_time,
                int64 end_time, int64 granularity_ms, Writer* writer,
                MessageHandler* message_handler) const;

 private:
  friend class SharedMemStatisticsTestBase;
  FRIEND_TEST(SharedMemStatisticsTestBase, TestNextDataBlock);
  FRIEND_TEST(SharedMemStatisticsTestBase, TestParseVarData);
  FRIEND_TEST(SharedMemStatisticsTestBase, TestParseHistData);
  FRIEND_TEST(SharedMemStatisticsTestBase, TestPrintJSONResponse);
  FRIEND_TEST(SharedMemStatisticsTestBase, TestParseDataFromReader);

  typedef std::vector<GoogleString> VariableInfo;
  typedef std::map<GoogleString, VariableInfo> VarMap;
  typedef std::pair<GoogleString, GoogleString> HistBounds;
  typedef std::pair<HistBounds, GoogleString> HistBarInfo;
  typedef std::vector<HistBarInfo> HistInfo;
  typedef std::map<GoogleString, HistInfo> HistMap;
  void ParseDataFromReader(const std::set<GoogleString>& var_titles,
                           const std::set<GoogleString>& hist_titles,
                           ConsoleStatisticsLogfileReader* reader,
                           std::vector<int64>* list_of_timestamps,
                           VarMap* parsed_var_data, HistMap* parsed_hist_data)
                           const;
  void ParseVarDataIntoMap(StringPiece logfile_var_data,
                           const std::set<GoogleString>& var_titles,
                           VarMap* parsed_var_data) const;
  HistMap ParseHistDataIntoMap(StringPiece logfile_hist_data,
                               const std::set<GoogleString>& hist_titles) const;
  void PrintVarDataAsJSON(const VarMap& parsed_var_data, Writer* writer,
                          MessageHandler* message_handler) const;
  void PrintHistDataAsJSON(const HistMap* parsed_hist_data, Writer* writer,
                           MessageHandler* message_handler) const;
  void PrintTimestampListAsJSON(const std::vector<int64>& list_of_timestamps,
                                Writer* writer,
                                MessageHandler* message_handler) const;
  void PrintJSON(const std::vector<int64> & list_of_timestamps,
                 const VarMap& parsed_var_data, const HistMap& parsed_hist_data,
                 Writer* writer, MessageHandler* message_handler) const;

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
  virtual int NumBuckets();
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

  // We rely on NumBuckets to allocate a memory segment for the histogram, so
  // this should be called right after AddHistogram() in the ::Initialize
  // process. Similarly, all the bounds must be initialized at that point, to
  // avoid clearing the histogram as new child processes attach to it.
  virtual void SetSuggestedNumBuckets(int i);

  // Return the amount of shared memory this Histogram objects needs for its
  // use.
  size_t AllocationSize(AbstractSharedMem* shm_runtime) {
    // Shared memory space should include a mutex, HistogramBody and the storage
    // for the actual buckets.
    return shm_runtime->SharedMutexSize() +  sizeof(HistogramBody)
        + sizeof(double) * NumBuckets();
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

  // Returns the width of normal buckets (as in not the two extreme outermost
  // buckets which have infinite width).
  double BucketWidth();

  // Finds a bucket that should contain the given value. Note that this does
  // not consider the catcher buckets for out-of-range values.
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
  // Number of buckets in this histogram.
  int num_buckets_;
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
  // AddHistograms and SetSuggestedNumBuckets (as well as any other histogram
  // range configurations) have been done.
  //
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
