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
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/statistics_template.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;
class AbstractSharedMem;
class AbstractSharedMemSegment;
class AbstractMutex;

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
  int64 Get64() const;
  virtual int Get() const;
  virtual void Set(int newValue);
  virtual void Add(int delta);

 private:
  friend class SharedMemTimedVariable;
  friend class SharedMemStatistics;

  explicit SharedMemVariable(const StringPiece& name);

  void AttachTo(AbstractSharedMemSegment* segment_, size_t offset,
                MessageHandler* message_handler);

  // Called on initialization failure, to make sure it's clear if we
  // share some state with parent.
  void Reset();

  // The name of this variable.
  const GoogleString name_;

  // Lock protecting us. NULL if for some reason initialization failed.
  scoped_ptr<AbstractMutex> mutex_;

  // The data...
  volatile int64* value_ptr_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemVariable);
};

// TODO(fangfei): shared memory histogram.
// NullStatisticsHistogram is for temporary util we have a shared memory
// histogram implemented.
class SharedMemStatistics : public StatisticsTemplate<SharedMemVariable,
    FakeHistogram, FakeTimedVariable> {
 public:
  SharedMemStatistics(AbstractSharedMem* shm_runtime,
                      const GoogleString& filename_prefix);
  virtual ~SharedMemStatistics();

  // This method initializes or attaches to shared memory. You should call this
  // exactly once in each process/thread, after all calls to AddVariables have
  // been done. The root process (the one that starts all the other child
  // threads and processes) must be the first one to make the call, with
  // parent = true, with all other calling it with = false.
  void InitVariables(bool parent, MessageHandler* message_handler);

  // This should be called from the root process as it is about to exit, when
  // no further children are expected to start.
  void GlobalCleanup(MessageHandler* message_handler);

 protected:
  virtual SharedMemVariable* NewVariable(const StringPiece& name, int index);

 private:
  GoogleString SegmentName() const;

  // Create mutexes in the segment, with per_var bytes being used,
  // counting the mutex, for each variable.
  bool InitMutexes(size_t per_var, MessageHandler* message_handler);

  AbstractSharedMem* shm_runtime_;
  GoogleString filename_prefix_;
  scoped_ptr<AbstractSharedMemSegment> segment_;
  bool frozen_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemStatistics);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_STATISTICS_H_
