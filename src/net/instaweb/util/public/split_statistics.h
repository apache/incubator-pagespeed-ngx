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

// Author: morlovich@google.com (Maksim Orlovich)
//
// SplitStatistics is intended for deployments where statistics information is
// collected both split over various disjoint domains (e.g. vhosts) and
// globally, with the class making sure to update both the local and global
// fragments appropriately. Also included are its variable, timed variable,
// and histogram implementations.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SPLIT_STATISTICS_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SPLIT_STATISTICS_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/statistics_template.h"

namespace net_instaweb {

class AbstractMutex;
class MessageHandler;
class ThreadSystem;
class Writer;

// A statistics variable that forwards writes to two other Variable objects,
// but reads only from one.
class SplitVariable : public Variable {
 public:
  // Variable 'rw' will be used to read and write, variable 'w'
  // will be used for writes only. Does not take ownership of either
  // 'rw' or 'w'. 'rw' and 'w' must be non-NULL.
  SplitVariable(Variable* rw, Variable* w);
  virtual ~SplitVariable();
  virtual int Get() const;
  virtual void Set(int delta);
  virtual int64 Get64() const;
  virtual StringPiece GetName() const;
  virtual void Add(int delta);

 private:
  Variable* rw_;
  Variable* w_;
  DISALLOW_COPY_AND_ASSIGN(SplitVariable);
};

class SplitConsoleStatisticsLogger : public ConsoleStatisticsLogger {
 public:
  // a and b may be NULL. Does not take ownership of either 'a' or 'b'.
  SplitConsoleStatisticsLogger(ConsoleStatisticsLogger* a,
                               ConsoleStatisticsLogger* b);
  virtual ~SplitConsoleStatisticsLogger();
  virtual void UpdateAndDumpIfRequired();

 private:
  ConsoleStatisticsLogger* a_;
  ConsoleStatisticsLogger* b_;
  DISALLOW_COPY_AND_ASSIGN(SplitConsoleStatisticsLogger);
};

// A histogram that forwards writes to two other Histogram objects,
// but reads only from one.
class SplitHistogram : public Histogram {
 public:
  // Histogram 'rw' will be used to read and write, histogram 'w'
  // will be used for writes only. Does not take ownership of either
  // 'rw' or 'w'. 'rw' and 'w' must be non-NULL.
  SplitHistogram(ThreadSystem* thread_system, Histogram* rw, Histogram* w);
  virtual ~SplitHistogram();

  // Reimplementation of the histogram API. See the base class for method
  // descriptions.
  virtual void Add(double value);
  virtual void Clear();
  virtual void Render(int index, Writer* writer, MessageHandler* handler);
  virtual int MaxBuckets();
  virtual void EnableNegativeBuckets();
  virtual void SetMinValue(double value);
  virtual void SetMaxValue(double value);
  virtual void SetMaxBuckets(int i);
  virtual double BucketStart(int index);
  virtual double BucketLimit(int index);
  virtual double BucketCount(int index);

 protected:
  virtual double AverageInternal();
  virtual double PercentileInternal(const double perc);
  virtual double StandardDeviationInternal();
  virtual double CountInternal();
  virtual double MaximumInternal();
  virtual double MinimumInternal();

  virtual AbstractMutex* lock();

 private:
  scoped_ptr<AbstractMutex> lock_;
  Histogram* rw_;
  Histogram* w_;

  DISALLOW_COPY_AND_ASSIGN(SplitHistogram);
};

// A timed variable that forwards writes writes to two other TimedVariable
// objects, but reads only from one.
class SplitTimedVariable : public TimedVariable {
 public:
  // TimedVariable 'rw' will be used to read and write, histogram 'w'
  // will be used for writes only. Does not take ownership of either
  // 'rw' or 'w'. 'rw' and 'w' must be non-NULL.
  SplitTimedVariable(TimedVariable* rw, TimedVariable* w);
  virtual ~SplitTimedVariable();

  virtual void IncBy(int64 delta);
  virtual int64 Get(int level);
  virtual void Clear();

 private:
  TimedVariable* rw_;
  TimedVariable* w_;

  DISALLOW_COPY_AND_ASSIGN(SplitTimedVariable);
};

class SplitStatistics
    : public StatisticsTemplate<SplitVariable, SplitHistogram,
                                SplitTimedVariable> {
 public:
  // Initializes a statistics splitter which proxies 'local' but also forwards
  // writes to 'global' for aggregation with other SplitStatistics instances.
  // Takes ownership of 'local', but not thread_system or global.
  //
  // Note that before AddVariable or similar methods are invoked on this object
  // (which is usually done by static ::Initialize(Statistics* methods) they
  // must have been invoked on both local and global statistics objects for
  // the same object names.
  SplitStatistics(ThreadSystem* thread_system,
                  Statistics* local,
                  Statistics* global);

  virtual ~SplitStatistics();

 protected:
  virtual SplitVariable* NewVariable(const StringPiece& name, int index);
  virtual SplitHistogram* NewHistogram(const StringPiece& name);
  virtual SplitTimedVariable* NewTimedVariable(const StringPiece& name,
                                               int index);
  // TODO(morlovich): implement console_logger().

 private:
  ThreadSystem* thread_system_;
  scoped_ptr<Statistics> local_;
  Statistics* global_;
  DISALLOW_COPY_AND_ASSIGN(SplitStatistics);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SPLIT_STATISTICS_H_
