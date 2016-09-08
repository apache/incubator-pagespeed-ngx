/*
 * Copyright 2010 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef PAGESPEED_KERNEL_BASE_STATISTICS_H_
#define PAGESPEED_KERNEL_BASE_STATISTICS_H_

#include <map>

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"

namespace net_instaweb {

class MessageHandler;
class Statistics;
class StatisticsLogger;
class Writer;

// Variables can normally only be increased, not decreased.  However, for
// testing, They can also be Cleared.
//
// TODO(jmarantz): consider renaming this to Counter or maybe UpCounter.
class Variable {
 public:
  virtual ~Variable();

  virtual int64 Get() const = 0;
  // Return some name representing the variable, provided that the specific
  // implementation has some sensible way of doing so.
  virtual StringPiece GetName() const = 0;

  // Adds 'delta' to the variable's value, returning the result.
  int64 Add(int64 non_negative_delta) {
    DCHECK_LE(0, non_negative_delta);
    return AddHelper(non_negative_delta);
  }

  virtual void Clear() = 0;

 protected:
  // This is virtual so that subclasses can add platform-specific atomicity.
  virtual int64 AddHelper(int64 delta) = 0;
};

// UpDownCounters are variables that can also be decreased (e.g. Add
// of a negative number) or Set to an arbitrary value.
//
// TODO(jmarantz): Make this not inherit from Variable, which will simplify the
// 'CheckNotNegative' and its ifndefs, but will require us to do more accurate
// type bookkeeping in tests, etc.
//
// TODO(jmarantz): consider renaming Variable->Counter, UpDownCounter->Variable.
class UpDownCounter {
 public:
  virtual ~UpDownCounter();

  virtual int64 Get() const = 0;
  // Return some name representing the variable, provided that the specific
  // implementation has some sensible way of doing so.
  virtual StringPiece GetName() const = 0;

  // Sets the specified value, returning the previous value.  This can be
  // used to by two competing threads/processes to determine which thread
  // modified the value first.  The default implementation is non-atomic,
  // but implementations can override to provide an atomic version.
  //
  // Non-atomic implementations may result in multiple concurrent updates
  // each returning the old value.  In an atomic implementation, only one
  // concurrent update will return the old value.
  virtual int64 SetReturningPreviousValue(int64 value);

  virtual void Set(int64 value) = 0;
  void Clear() { Set(0); }
  int64 Add(int64 delta) { return AddHelper(delta); }

 protected:
  // This is virtual so that subclasses can add platform-specific atomicity.
  virtual int64 AddHelper(int64 delta) = 0;
};

// Scalar value protected by a mutex. Mutex must fully protect access
// to underlying scalar. For example, in mod_pagespeed and
// ngx_pagespeed, variables are stored in shared memory and accessible
// from any process on a machine, so the mutex must provide protection
// across separate processes.
//
// StatisticsLogger depends upon these mutexes being cross-process so that
// several processes using the same file system don't clobber each others logs.
//
// TODO(jmarantz): this could be done using templates rather than using virtual
// methods, as MutexedScalar does not implement an abstract interface.
class MutexedScalar {
 public:
  virtual ~MutexedScalar();

  // Subclasses should not define these methods, instead define the *LockHeld()
  // methods below.
  int64 Get() const;
  void Set(int64 value);
  int64 SetReturningPreviousValue(int64 value);
  int64 AddHelper(int64 delta);

 protected:
  friend class StatisticsLogger;

  virtual AbstractMutex* mutex() const = 0;

  // Get/Setters that may only be called if you already hold the mutex.
  virtual int64 GetLockHeld() const = 0;
  virtual int64 SetReturningPreviousValueLockHeld(int64 value) = 0;

  // These are implemented based on GetLockHeld() and
  // SetReturningPreviousLockHeld().
  void SetLockHeld(int64 value);
  int64 AddLockHeld(int64 delta);
};

class Histogram {
 public:
  virtual ~Histogram();
  // Record a value in its bucket.
  virtual void Add(double value) = 0;
  // Throw away all data.
  virtual void Clear() = 0;
  // True if the histogram is empty.
  bool Empty() {
    ScopedMutex hold(lock());
    return CountInternal() == 0;
  }
  // Write Histogram Data to the writer.
  // Default implementation does not include histogram graph, but only raw
  // histogram data table. It looks like:
  // ________________________________________
  // |  TITLE String                         |
  // |  Avg: StdDev: Median: 90%: 95%: 99%   |
  // |  Raw Histogram Data:                  |
  // |  [0,1] 1 25% 25%  |||||               |
  // |  [2,3] 1 25% 50%  |||||               |
  // |  [4,5] 2 50% 100% ||||||||||          |
  // |_______________________________________|
  virtual void Render(int index, Writer* writer, MessageHandler* handler);

  // Returns number of buckets the histogram actually has.
  virtual int NumBuckets() = 0;
  // Allow histogram have negative values.
  virtual void EnableNegativeBuckets() = 0;
  // Set the minimum value allowed in histogram.
  virtual void SetMinValue(double value) = 0;
  // Set the value upper-bound of a histogram,
  // the value range in histogram is [MinValue, MaxValue) or
  // [-MaxValue, MaxValue) if enabled negative buckets.
  virtual void SetMaxValue(double value) = 0;

  // Set the suggested number of buckets for the histogram. The implementation
  // may chose to use a somewhat different number.
  virtual void SetSuggestedNumBuckets(int i) = 0;

  // Returns average of the values added.
  double Average() {
    ScopedMutex hold(lock());
    return AverageInternal();
  }
  // Return estimated value that is greater than perc% of all data.
  // e.g. Percentile(20) returns the value which is greater than
  // 20% of data.
  double Percentile(const double perc) {
    ScopedMutex hold(lock());
    return PercentileInternal(perc);
  }
  double StandardDeviation() {
    ScopedMutex hold(lock());
    return StandardDeviationInternal();
  }
  double Count() {
    ScopedMutex hold(lock());
    return CountInternal();
  }
  double Maximum() {
    ScopedMutex hold(lock());
    return MaximumInternal();
  }
  double Minimum() {
    ScopedMutex hold(lock());
    return MinimumInternal();
  }
  double Median() {
    return Percentile(50);
  }

  // Formats the histogram statistics as an HTML table row.  This
  // is intended for use in Statistics::RenderHistograms.
  //
  // The <tr> element id is given id=hist_row_%d where %d is from the index.
  // Included in the row an input radio button which is initiated in state
  // 'selected' for index==0.
  GoogleString HtmlTableRow(const GoogleString& title, int index);

  // Lower bound of a bucket. If index == NumBuckets() + 1, returns the
  // upper bound of the histogram. DCHECK if index is in the range of
  // [0, NumBuckets()+1].
  virtual double BucketStart(int index) = 0;
  // Upper bound of a bucket.
  virtual double BucketLimit(int index) {
    return BucketStart(index + 1);
  }
  // Value of a bucket.
  virtual double BucketCount(int index) = 0;

 protected:
  Histogram() {}

  // Note that these *Internal interfaces require the mutex to be held.
  virtual double AverageInternal() = 0;
  virtual double PercentileInternal(const double perc) = 0;
  virtual double StandardDeviationInternal() = 0;
  virtual double CountInternal() = 0;
  virtual double MaximumInternal() = 0;
  virtual double MinimumInternal() = 0;

  virtual AbstractMutex* lock() = 0;

  // Helper function of Render(), write entries of histogram raw data table.
  // Each entry includes bucket range, bucket count, percentage,
  // cumulative percentage, bar. It looks like:
  // [0,1] 1 5%  5%  ||||
  // [2,3] 2 10% 15% ||||||||
  // Precondition: mutex held.
  void WriteRawHistogramData(Writer* writer, MessageHandler* handler);

 private:
  DISALLOW_COPY_AND_ASSIGN(Histogram);
};

// Trivial implementation. But Count() returns a meaningful value.
class CountHistogram : public Histogram {
 public:
  // Takes ownership of mutex.
  explicit CountHistogram(AbstractMutex* mutex);
  virtual ~CountHistogram();
  virtual void Add(double value) {
    ScopedMutex hold(lock());
    ++count_;
  }
  virtual void Clear() {
    ScopedMutex hold(lock());
    count_ = 0;
  }
  virtual int NumBuckets() { return 0; }
  virtual void EnableNegativeBuckets() { }
  virtual void SetMinValue(double value) { }
  virtual void SetMaxValue(double value) { }
  virtual void SetSuggestedNumBuckets(int i) { }
  virtual GoogleString GetName() const { return ""; }

 protected:
  virtual AbstractMutex* lock() LOCK_RETURNED(mutex_) { return mutex_.get(); }
  virtual double AverageInternal() { return 0.0; }
  virtual double PercentileInternal(const double perc) { return 0.0; }
  virtual double StandardDeviationInternal() { return 0.0; }
  virtual double CountInternal() EXCLUSIVE_LOCKS_REQUIRED(lock()) {
    return count_;
  }
  virtual double MaximumInternal() { return 0.0; }
  virtual double MinimumInternal() { return 0.0; }
  virtual double BucketStart(int index) { return 0.0; }
  virtual double BucketCount(int index) { return 0.0; }

 private:
  scoped_ptr<AbstractMutex> mutex_;
  int count_ GUARDED_BY(mutex_);

  DISALLOW_COPY_AND_ASSIGN(CountHistogram);
};

// TimedVariable is a statistic class returns the amount added in the
// last interval, which could be last 10 seconds, last minute
// last one hour and total.
class TimedVariable {
 public:
  // The intervals for which we keep stats.
  enum Levels { TENSEC, MINUTE, HOUR, START };
  virtual ~TimedVariable();
  // Update the stat value. delta is in milliseconds.
  virtual void IncBy(int64 delta) = 0;
  // Get the amount added over the last time interval
  // specified by "level".
  virtual int64 Get(int level) = 0;
  // Throw away all data.
  virtual void Clear() = 0;
};

// TimedVariable implementation that only updates a basic UpDownCounter.
class FakeTimedVariable : public TimedVariable {
 public:
  FakeTimedVariable(StringPiece name, Statistics* stats);
  virtual ~FakeTimedVariable();
  // Update the stat value. delta is in milliseconds.
  virtual void IncBy(int64 delta) {
    var_->Add(delta);
  }
  // Get the amount added over the last time interval
  // specified by "level".
  virtual int64 Get(int level) {
    // This is a default implementation. Variable can only return the
    // total value. This should be override in subclass if we want the
    // values for different levels.
    if (level == START) {
      return var_->Get();
    }
    return 0;
  }
  // Throw away all data.
  virtual void Clear() {
    return var_->Clear();
  }

 protected:
  Variable* var_;
};

// Base class for implementations of monitoring statistics.
class Statistics {
 public:
  // Default group for use with AddTimedVariable.
  static const char kDefaultGroup[];

  Statistics() {}
  virtual ~Statistics();

  // Add a new variable, or returns an existing one of that name.
  // The UpDownCounter* is owned by the Statistics class -- it should
  // not be deleted by the caller.
  virtual UpDownCounter* AddUpDownCounter(const StringPiece& name) = 0;

  // Like AddVariable, but asks the implementation to scope the variable to the
  // entire process, even if statistics are generally partitioned by domains or
  // the like. Default implementation simply forwards to AddVariable.
  virtual UpDownCounter* AddGlobalUpDownCounter(const StringPiece& name);

  // Find a variable from a name, returning NULL if not found.
  virtual UpDownCounter* FindUpDownCounter(const StringPiece& name) const = 0;

  // Find a variable from a name, aborting if not found.
  UpDownCounter* GetUpDownCounter(const StringPiece& name) const {
    UpDownCounter* var = FindUpDownCounter(name);
    CHECK(var != NULL) << "UpDownCounter not found: " << name;
    return var;
  }

  // Add a new variable, or returns an existing one of that name.
  // The Variable* is owned by the Statistics class -- it should
  // not be deleted by the caller.
  virtual Variable* AddVariable(const StringPiece& name) = 0;

  // Find a variable from a name, returning NULL if not found.
  virtual Variable* FindVariable(const StringPiece& name) const = 0;

  // Find a variable from a name, aborting if not found.
  Variable* GetVariable(const StringPiece& name) const {
    Variable* var = FindVariable(name);
    CHECK(var != NULL) << "Variable not found: " << name;
    return var;
  }


  // Add a new histogram, or returns an existing one of that name.
  // The Histogram* is owned by the Statistics class -- it should not
  // be deleted by the caller.
  virtual Histogram* AddHistogram(const StringPiece& name) = 0;
  // Find a histogram from a name, returning NULL if not found.
  virtual Histogram* FindHistogram(const StringPiece& name) const = 0;
  // Find a histogram from a name, aborting if not found.
  Histogram* GetHistogram(const StringPiece& name) const {
    Histogram* hist = FindHistogram(name);
    CHECK(hist != NULL) << "Histogram not found: " << name;
    return hist;
  }

  // Add a new TimedVariable, or returns an existing one of that name.
  // The TimedVariable* is owned by the Statistics class -- it should
  // not be deleted by the caller. Each stat belongs to a group, such as
  // "Statistics", "Disk Statistics", etc.
  virtual TimedVariable* AddTimedVariable(
      const StringPiece& name, const StringPiece& group) = 0;
  // Find a TimedVariable from a name, returning NULL if not found.
  virtual TimedVariable* FindTimedVariable(
      const StringPiece& name) const = 0;
  // Find a TimedVariable from a name, aborting if not found.
  TimedVariable* GetTimedVariable(
      const StringPiece& name) const {
    TimedVariable* stat = FindTimedVariable(name);
    CHECK(stat != NULL) << "TimedVariable not found: " << name;
    return stat;
  }
  // Return the names of all the histograms for render.
  virtual const StringVector& HistogramNames() = 0;
  // Return the map of groupnames and names of all timedvariables for render.
  virtual const std::map<GoogleString, StringVector>& TimedVariableMap() = 0;
  // Dump the variable-values to a writer.
  virtual void Dump(Writer* writer, MessageHandler* handler) = 0;
  // Dump the variable-values in JSON format to a writer.
  virtual void DumpJson(Writer* writer, MessageHandler* message_handler) = 0;
  virtual void RenderTimedVariables(Writer* writer,
                                    MessageHandler* handler);
  // Write all the histograms in this Statistic object to a writer.
  virtual void RenderHistograms(Writer* writer, MessageHandler* handler);
  // Set all variables to 0.
  // Throw away all data in histograms and stats.
  virtual void Clear() = 0;

  // This is implemented as NULL here because most Statistics don't
  // need it. In the context in which it is needed we only have access to a
  // Statistics*, rather than the specific subclass, hence its being here.
  // Return the StatisticsLogger associated with this Statistics.
  virtual StatisticsLogger* console_logger() { return NULL; }

  // Testing helper method to look up a statistics numeric value by name.
  // Please do not use this in production code.  This finds the current
  // value whether it is stored in a Variable, UpDownCounter, or TimedVariable.
  //
  // If the statistics is not found, the program check-fails.
  int64 LookupValue(StringPiece stat_name);

 private:
  DISALLOW_COPY_AND_ASSIGN(Statistics);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_STATISTICS_H_
