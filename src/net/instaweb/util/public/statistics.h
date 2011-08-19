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

  virtual void Add(int delta) { Set(delta + Get()); }
  void Clear() { Set(0); }
};

class Histogram {
 public:
  virtual ~Histogram();
  // Record a value in its bucket.
  virtual void Add(double value) = 0;
  // Throw away all data.
  virtual void Clear() = 0;
  // True if the histogram is empty.
  virtual bool Empty() {
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
  virtual void Render(const StringPiece& title, Writer* writer,
                      MessageHandler* handler);
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
  // Record a value in its bucket.
  virtual double Average() {
    ScopedMutex hold(lock());
    return AverageInternal();
  }
  // Return estimated value that is greater than perc% of all data.
  // e.g. Percentile(20) returns the value which is greater than
  // 20% of data.
  virtual double Percentile(const double perc) {
    ScopedMutex hold(lock());
    return PercentileInternal(perc);
  }
  virtual double StandardDeviation() {
    ScopedMutex hold(lock());
    return StandardDeviationInternal();
  }
  virtual double Count() {
    ScopedMutex hold(lock());
    return CountInternal();
  }
  virtual double Maximum() {
    ScopedMutex hold(lock());
    return MaximumInternal();
  }
  virtual double Minimum() {
    ScopedMutex hold(lock());
    return MinimumInternal();
  }
  virtual double Median() {
    return Percentile(50);
  }

 protected:
  virtual AbstractMutex* lock() = 0;
  virtual double AverageInternal() = 0;
  virtual double PercentileInternal(const double perc) = 0;
  virtual double StandardDeviationInternal() = 0;
  virtual double CountInternal() = 0;
  virtual double MaximumInternal() = 0;
  virtual double MinimumInternal() = 0;
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
  // Helper function of Render(), write entries of histogram raw data table.
  // Each entry includes bucket range, bucket count, percentage,
  // cumulative percentage, bar. It looks like:
  // [0,1] 1 5%  5%  ||||
  // [2,3] 2 10% 15% ||||||||
  virtual void WriteRawHistogramData(Writer* writer, MessageHandler* handler);
};

// FakeHistogram is an empty implemenation of Histogram.
class FakeHistogram : public Histogram {
 public:
  FakeHistogram() {}
  virtual ~FakeHistogram();
  virtual void Add(const double value) { }
  virtual void Clear() { }
  virtual bool Empty() { return true; }
  virtual int MaxBuckets() { return 0; }
  virtual void EnableNegativeBuckets() { }
  virtual void SetMinValue(double value) { }
  virtual void SetMaxValue(double value) { }
  virtual void SetMaxBuckets(int i) { }

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
  class FakeMutex : public AbstractMutex {
   public:
    FakeMutex() {}
    virtual ~FakeMutex() { }
    virtual void Lock() { }
    virtual void Unlock() { }
  };
  FakeMutex mutex_;
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

// FakeTimedVariable is an implementation of abstract class TimedVariable
// based on class Variable. This class could be derived in AprStatistics,
// NullStatistics, SpreadSheet, etc. In VarzStatistics, we have class
// VarzTimedVariable which is implemented based on google class
// TenSecMinHourStat instead of class Variable, that is derived from
// TimedVaraible.
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

// Helps build a statistics that can be exported as a CSV file.
class Statistics {
 public:
  virtual ~Statistics();

  // Add a new variable, or returns an existing one of that name.
  // The Variable* is owned by the Statistics class -- it should
  // not be deleted by the caller.
  virtual Variable* AddVariable(const StringPiece& name) = 0;

  // Find a variable from a name, returning NULL if not found.
  virtual Variable* FindVariable(const StringPiece& name) const = 0;

  // Find a variable from a name, aborting if not found.
  virtual Variable* GetVariable(const StringPiece& name) const {
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
  virtual Histogram* GetHistogram(const StringPiece& name) const {
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
  virtual TimedVariable* GetTimedVariable(
      const StringPiece& name) const {
    TimedVariable* stat = FindTimedVariable(name);
    CHECK(stat != NULL) << "TimedVariable not found: " << name;
    return stat;
  }
  // Return the names of all the histograms for render.
  virtual StringVector& HistogramNames() = 0;
  // Return the map of groupnames and names of all timedvariables for render.
  virtual std::map<GoogleString, StringVector>& TimedVariableMap() = 0;
  // Dump the variable-values to a writer.
  virtual void Dump(Writer* writer, MessageHandler* handler) = 0;
  // Export statistics to a writer. Statistics in a group are exported in one
  // table.
  virtual void RenderTimedVariables(Writer* writer,
                                    MessageHandler* handler);
  // Write all the histograms in this Statistic object to a writer.
  virtual void RenderHistograms(Writer* writer, MessageHandler* handler);
  // Set all variables to 0.
  // Throw away all data in histograms and stats.
  virtual void Clear() = 0;

 protected:
  virtual Histogram* NewHistogram();
  virtual TimedVariable* NewTimedVariable(const StringPiece& name, int index);
  virtual Variable* NewVariable(const StringPiece& name, int index) = 0;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_STATISTICS_H_
