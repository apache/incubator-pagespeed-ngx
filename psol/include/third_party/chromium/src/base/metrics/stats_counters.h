// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_STATS_COUNTERS_H_
#define BASE_METRICS_STATS_COUNTERS_H_
#pragma once

#include <string>

#include "base/base_api.h"
#include "base/metrics/stats_table.h"
#include "base/time.h"

namespace base {

// StatsCounters are dynamically created values which can be tracked in
// the StatsTable.  They are designed to be lightweight to create and
// easy to use.
//
// Since StatsCounters can be created dynamically by name, there is
// a hash table lookup to find the counter in the table.  A StatsCounter
// object can be created once and used across multiple threads safely.
//
// Example usage:
//    {
//      StatsCounter request_count("RequestCount");
//      request_count.Increment();
//    }
//
// Note that creating counters on the stack does work, however creating
// the counter object requires a hash table lookup.  For inner loops, it
// may be better to create the counter either as a member of another object
// (or otherwise outside of the loop) for maximum performance.
//
// Internally, a counter represents a value in a row of a StatsTable.
// The row has a 32bit value for each process/thread in the table and also
// a name (stored in the table metadata).
//
// NOTE: In order to make stats_counters usable in lots of different code,
// avoid any dependencies inside this header file.
//

//------------------------------------------------------------------------------
// Define macros for ease of use. They also allow us to change definitions
// as the implementation varies, or depending on compile options.
//------------------------------------------------------------------------------
// First provide generic macros, which exist in production as well as debug.
#define STATS_COUNTER(name, delta) do { \
  base::StatsCounter counter(name); \
  counter.Add(delta); \
} while (0)

#define SIMPLE_STATS_COUNTER(name) STATS_COUNTER(name, 1)

#define RATE_COUNTER(name, duration) do { \
  base::StatsRate hit_count(name); \
  hit_count.AddTime(duration); \
} while (0)

// Define Debug vs non-debug flavors of macros.
#ifndef NDEBUG

#define DSTATS_COUNTER(name, delta) STATS_COUNTER(name, delta)
#define DSIMPLE_STATS_COUNTER(name) SIMPLE_STATS_COUNTER(name)
#define DRATE_COUNTER(name, duration) RATE_COUNTER(name, duration)

#else  // NDEBUG

#define DSTATS_COUNTER(name, delta) do {} while (0)
#define DSIMPLE_STATS_COUNTER(name) do {} while (0)
#define DRATE_COUNTER(name, duration) do {} while (0)

#endif  // NDEBUG

//------------------------------------------------------------------------------
// StatsCounter represents a counter in the StatsTable class.
class BASE_API StatsCounter {
 public:
  // Create a StatsCounter object.
  explicit StatsCounter(const std::string& name);
  virtual ~StatsCounter();

  // Sets the counter to a specific value.
  void Set(int value);

  // Increments the counter.
  void Increment() {
    Add(1);
  }

  virtual void Add(int value);

  // Decrements the counter.
  void Decrement() {
    Add(-1);
  }

  void Subtract(int value) {
    Add(-value);
  }

  // Is this counter enabled?
  // Returns false if table is full.
  bool Enabled() {
    return GetPtr() != NULL;
  }

  int value() {
    int* loc = GetPtr();
    if (loc) return *loc;
    return 0;
  }

 protected:
  StatsCounter();

  // Returns the cached address of this counter location.
  int* GetPtr();

  std::string name_;
  // The counter id in the table.  We initialize to -1 (an invalid value)
  // and then cache it once it has been looked up.  The counter_id is
  // valid across all threads and processes.
  int32 counter_id_;
};


// A StatsCounterTimer is a StatsCounter which keeps a timer during
// the scope of the StatsCounterTimer.  On destruction, it will record
// its time measurement.
class BASE_API StatsCounterTimer : protected StatsCounter {
 public:
  // Constructs and starts the timer.
  explicit StatsCounterTimer(const std::string& name);
  virtual ~StatsCounterTimer();

  // Start the timer.
  void Start();

  // Stop the timer and record the results.
  void Stop();

  // Returns true if the timer is running.
  bool Running();

  // Accept a TimeDelta to increment.
  virtual void AddTime(TimeDelta time);

 protected:
  // Compute the delta between start and stop, in milliseconds.
  void Record();

  TimeTicks start_time_;
  TimeTicks stop_time_;
};

// A StatsRate is a timer that keeps a count of the number of intervals added so
// that several statistics can be produced:
//    min, max, avg, count, total
class BASE_API StatsRate : public StatsCounterTimer {
 public:
  // Constructs and starts the timer.
  explicit StatsRate(const std::string& name);
  virtual ~StatsRate();

  virtual void Add(int value);

 private:
  StatsCounter counter_;
  StatsCounter largest_add_;
};


// Helper class for scoping a timer or rate.
template<class T> class StatsScope {
 public:
  explicit StatsScope<T>(T& timer)
      : timer_(timer) {
    timer_.Start();
  }

  ~StatsScope() {
    timer_.Stop();
  }

  void Stop() {
    timer_.Stop();
  }

 private:
  T& timer_;
};

}  // namespace base

#endif  // BASE_METRICS_STATS_COUNTERS_H_
