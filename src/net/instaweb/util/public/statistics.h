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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_STATISTICS_H_
#define NET_INSTAWEB_UTIL_PUBLIC_STATISTICS_H_

#include <map>
#include "base/logging.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/null_mutex.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;
class Writer;

class Variable {
 public:
  virtual ~Variable();
  // TODO(sligocki): int -> int64
  virtual int Get() const = 0;
  virtual void Set(int delta) = 0;
  virtual int64 Get64() const = 0;
  // Return some name representing the variable, provided that the specific
  // implementation has some sensible way of doing so.
  virtual StringPiece GetName() const = 0;

  virtual void Add(int delta) { Set(delta + Get()); }
  void Clear() { Set(0); }
};

// Class that manages dumping statistics periodically to a file.
class ConsoleStatisticsLogger {
 public:
  virtual ~ConsoleStatisticsLogger();

  // If it's been longer than kStatisticsDumpIntervalMs, update the
  // timestamp to now and dump the current state of the Statistics.
  virtual void UpdateAndDumpIfRequired() = 0;
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
  // Maxmum number of buckets. This number can be used to allocate a buffer for
  // Histogram.
  virtual int MaxBuckets() = 0;
  // Allow histogram have negative values.
  virtual void EnableNegativeBuckets() = 0;
  // Set the minimum value allowed in histogram.
  virtual void SetMinValue(double value) = 0;
  // Set the value upper-bound of a histogram,
  // the value range in histogram is [MinValue, MaxValue) or
  // (-MaxValue, MaxValue) if enabled negative buckets.
  virtual void SetMaxValue(double value) = 0;
  // Set the maximum number of buckets.
  virtual void SetMaxBuckets(int i) = 0;

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

  // Lower bound of a bucket. If index == MaxBuckets() + 1, returns the
  // upper bound of the histogram. DCHECK if index is in the range of
  // [0, MaxBuckets()+1].
  virtual double BucketStart(int index) = 0;
  // Upper bound of a bucket.
  virtual double BucketLimit(int index) {
    return BucketStart(index + 1);
  }
  // Value of a bucket.
  virtual double BucketCount(int index) = 0;

 protected:
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
};

class NullHistogram : public Histogram {
 public:
  NullHistogram() {}
  virtual ~NullHistogram();
  virtual void Add(const double value) { }
  virtual void Clear() { }
  virtual int MaxBuckets() { return 0; }
  virtual void EnableNegativeBuckets() { }
  virtual void SetMinValue(double value) { }
  virtual void SetMaxValue(double value) { }
  virtual void SetMaxBuckets(int i) { }
  virtual GoogleString GetName() const { return ""; }

 protected:
  virtual AbstractMutex* lock() { return &mutex_; }
  virtual double AverageInternal() { return 0.0; }
  virtual double PercentileInternal(const double perc) { return 0.0; }
  virtual double StandardDeviationInternal() { return 0.0; }
  virtual double CountInternal() { return 0.0; }
  virtual double MaximumInternal() { return 0.0; }
  virtual double MinimumInternal() { return 0.0; }
  virtual double BucketStart(int index) { return 0.0; }
  virtual double BucketCount(int index) { return 0.0; }

 private:
  NullMutex mutex_;

  DISALLOW_COPY_AND_ASSIGN(NullHistogram);
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

// TimedVariable implementation that only updates a basic Variable.
class FakeTimedVariable : public TimedVariable {
 public:
  explicit FakeTimedVariable(Variable* var) : var_(var) {
  }
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
      return var_->Get64();
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
  virtual ~Statistics();

  // Add a new variable, or returns an existing one of that name.
  // The Variable* is owned by the Statistics class -- it should
  // not be deleted by the caller.
  virtual Variable* AddVariable(const StringPiece& name) = 0;

  // Like AddVariable, but asks the implementation to scope the variable to the
  // entire process, even if statistics are generally partitioned by domains or
  // the like. Default implementation simply forwards to AddVariable.
  virtual Variable* AddGlobalVariable(const StringPiece& name);

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
  // Export statistics to a writer. Statistics in a group are exported in one
  // table. This only exports console-related variables, as opposed to all
  // variables, as the above does.
  // Empty implementation because most Statistics don't need this. It's
  // here because in the context in which it is needed we only have access to a
  // Statistics*, rather than the specific subclass.
  // current_time_ms: the time at which the dump was triggered
  virtual void DumpConsoleVarsToWriter(
      int64 current_time_ms, Writer* writer, MessageHandler* message_handler) {}
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
  // Return the ConsoleStatisticsLogger associated with this Statistics.
  virtual ConsoleStatisticsLogger* console_logger() const { return NULL; }

 protected:
  // A helper for subclasses that do not fully implement timed variables.
  FakeTimedVariable* NewFakeTimedVariable(const StringPiece& name, int index);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_STATISTICS_H_
