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

#ifndef NET_INSTAWEB_APACHE_APR_STATISTICS_H_
#define NET_INSTAWEB_APACHE_APR_STATISTICS_H_

#include <string>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/statistics_template.h"
#include "net/instaweb/util/public/string_util.h"
#include "apr_shm.h"
#include "apr.h"
#include "apr_errno.h"

struct apr_pool_t;
struct apr_global_mutex_t;
namespace net_instaweb {

class Writer;
class MessageHandler;

// An implementation of Statistics using the APR's Shared Memory module,
// apr_shm.  These statistics will be shared amongst all processes and threads
// spawned by Apache.  Note that we will be obtaining a global mutex for every
// read and write to these variables.  Since this may be expensive, it is
// recommended that each thread keep a local cache and infrequently write
// through to this Statistics object.  TODO(abliss): actually do this.
//
// Because we must allocate shared memory segments before the module forks off
// its children, all AddVariable calls must be in the post_config hook.  Once
// all variables are added, you must call InitVariables.
//
// If a variable fails to initialize (due to either its mutex or its shared
// memory segment not working), it will not increment in that process (and a
// warning message will be logged).  Other variables will work normally.  If the
// variable fails to initialize in the process that happens to serve the
// mod_pagespeed_statistics page, then the variable will show up with value -1.
//
// Implementation details heavily cribbed from mod_shm_counter by Aaron Bannert.

class AprVariable : public Variable {
 public:
  explicit AprVariable(const StringPiece& name);
  int64 Get64() const;
  virtual int Get() const;
  virtual void Set(int newValue);
  virtual void Add(int delta);
 private:
  friend class AprTimedVariable;
  friend class AprStatistics;
  friend class AprScopedGlobalLock;

  // Logs an error message and returns false if the result is not SUCCESS.
  bool CheckResult(
      const apr_status_t result, const StringPiece& verb,
      const StringPiece& filename = EmptyString::kEmptyString) const;
  // Initialize this variable's mutex
  bool InitMutex(const StringPiece& filename_prefix, apr_pool_t* pool,
                 bool parent);
  // Initialize this variable's shared memory segment.
  bool InitShm(const StringPiece& filename_prefix, apr_pool_t* pool,
               bool parent);
  const std::string& name() const { return name_; }
  // The global (cross-thread, cross-process) mutex protecting value_.
  // This is NULL if the variable has not yet been properly initialized.
  apr_global_mutex_t* mutex_;
  // The name of this variable.
  const std::string name_;
  // Pointer to the shared-memory segment containing our current value.
  apr_shm_t* shm_;
  // Offset within the shared-memory segment for our current value.
  int64* value_ptr_;
};

class AprStatistics : public StatisticsTemplate<AprVariable,
                                                NullHistogram,
                                                FakeTimedVariable> {
 public:
  AprStatistics(const StringPiece& filename_prefix);
  virtual ~AprStatistics();

  // Allocate shared memory segments and mutices for all variables.  This must
  // be called with parent=true from the post_config hook, and with parent=false
  // from the child_init hook. After this is called, you must no longer call
  // AddVariable.
  void InitVariables(bool parent);

  // Dump the statistics to the given writer.
  void Dump(Writer* writer, MessageHandler* message_handler);

  // Set all statistics to 0.
  void Clear();

  bool frozen() const { return frozen_; }

 protected:
  virtual AprVariable* NewVariable(const StringPiece& name, int index);

 private:
  bool frozen_;
  bool is_child_;
  const StringPiece& filename_prefix_;
  apr_pool_t* pool_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APR_STATISTICS_H_
