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

#ifndef PAGESPEED_KERNEL_UTIL_SIMPLE_STATS_H_
#define PAGESPEED_KERNEL_UTIL_SIMPLE_STATS_H_

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/statistics_template.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class ThreadSystem;

// These variables are thread-safe.
class SimpleStatsVariable : public MutexedScalar {
 public:
  SimpleStatsVariable(StringPiece name, Statistics* stats);
  virtual ~SimpleStatsVariable();
  virtual StringPiece GetName() const { return StringPiece(); }

  void set_mutex(AbstractMutex* mutex) { mutex_.reset(mutex); }

 protected:
  virtual AbstractMutex* mutex() const { return mutex_.get(); }
  virtual int64 GetLockHeld() const;
  virtual int64 SetReturningPreviousValueLockHeld(int64 value);

 private:
  int64 value_;
  scoped_ptr<AbstractMutex> mutex_;
  DISALLOW_COPY_AND_ASSIGN(SimpleStatsVariable);
};

// Simple name/value pair statistics implementation.
class SimpleStats : public ScalarStatisticsTemplate<SimpleStatsVariable> {
 public:
  // SimpleStats will not take ownership of thread_system.  The thread system is
  // used to instantiate mutexes to allow SimpleStatsVariable to be thread-safe.
  explicit SimpleStats(ThreadSystem* thread_system);
  virtual ~SimpleStats();

  void SetThreadSystem(ThreadSystem* x);
  ThreadSystem* thread_system() const { return thread_system_; }

  virtual CountHistogram* NewHistogram(StringPiece name);
  virtual Var* NewVariable(StringPiece name);
  virtual UpDown* NewUpDownCounter(StringPiece name);

 private:
  ThreadSystem* thread_system_;  // Not owned by this class.

  DISALLOW_COPY_AND_ASSIGN(SimpleStats);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_UTIL_SIMPLE_STATS_H_
